#include "Pacer.h"
#include "pch.h"
#include <chrono>
#include <thread>
#include <windows.h>
#include "../Plot/ImGuiPlots.h"
#include "FFmpegDecoder.h"
#include "PacerRational.h"
#include "Utils.hpp"

// Frame Pacing operation
// 4 threads use this class:
//
// Decoder thread (run from moonlight-common-c because DIRECT_SUBMIT)
//   * submits AVFrame frames to pacing queue via submitFrame()
//   * max frames in pacing queue = 4, will drop oldest frame if it's full (dropped by back pacing)
//
// vsyncEmulator thread:
//   * Creates our own 120hz vsync signal since Xbox UWP only supports 60hz
//   * attempts to send m_VsyncSignalled as accurately locked to the refresh rate as possible.
//   * uses rational integer math to support 59.94 and 119.88
//   * syncs periodically with hardware 60hz vsync to prevent drift
//
// backPacer thread (called vsyncThread in moonlight-qt)
//   * wakes up on each signal from vsync thread, uses same deadline Qpc as emulator thread
//   * maintains pacing queue
//   * moves 1 frame to render queue
//   * tries to accurately time signal to main thread
//   * signals main thread to start rendering
//
// renderOnMainThread
//   * called from main render loop after waiting on GetFrameLatencyWaitableObject and signal from backPacer
//   * calls frontPacer() (renderFrame in moonlight-qt)
//   * maintains render queue
//
// Other differences from moonlight-qt:
// SwapChain BufferCount = 3
// GetFrameLatencyWaitableObject() is used to wait before each frame
// SetMaximumFrameLatency(1) (TODO: with dynamic adjustment to 2 if necessary?)

// Limit the number of queued frames to prevent excessive memory consumption
// if the V-Sync source or renderer is blocked for a while. It's important
// that the sum of all queued frames between both pacing and rendering queues
// must not exceed the number buffer pool size to avoid running the decoder
// out of available decoding surfaces.
#define MAX_QUEUED_FRAMES 4

// We may be woken up slightly late so don't go all the way
// up to the next V-sync since we may accidentally step into
// the next V-sync period. It also takes some amount of time
// to do the render itself, so we can't render right before
// V-sync happens.
#define TIMER_SLACK_MS 2

using steady_clock = std::chrono::steady_clock;
using namespace moonlight_xbox_dx;

Pacer::Pacer(const std::shared_ptr<DX::DeviceResources> &res) :
    m_DeviceResources(res),
    m_BackPacerThread(),
    m_VsyncSeq(0),
    m_VsyncNextDeadlineQpc(0),
    m_Stopping(false),
    m_MaxVideoFps(0),
    m_RefreshRate(0.0),
    m_FrameDropTarget(0) {
}

Pacer::~Pacer() {
	m_Stopping.store(true, std::memory_order_release);
	m_RenderQueueNotEmpty.notify_all();

	// Stop the vsync threads
	m_VsyncSignalled.notify_all();
	m_PacingQueueNotEmpty.notify_all();
	if (m_BackPacerThread.joinable()) {
		m_BackPacerThread.join();
	}
	if (m_VsyncEmulator.joinable()) {
		m_VsyncEmulator.join();
	}

	// Drain and free any remaining frames in both queues
	{
		std::unique_lock<std::mutex> lock(m_FrameQueueLock);

		while (!m_RenderQueue.empty()) {
			AVFrame *f = m_RenderQueue.front();
			m_RenderQueue.pop_front();
			lock.unlock();
			av_frame_free(&f);
			lock.lock();
		}
		while (!m_PacingQueue.empty()) {
			AVFrame *f = m_PacingQueue.front();
			m_PacingQueue.pop_front();
			lock.unlock();
			av_frame_free(&f);
			lock.lock();
		}
	}
}

void Pacer::initialize(int maxVideoFps, double refreshRate) {
	m_MaxVideoFps = maxVideoFps;
	m_RefreshRate = refreshRate;

	Utils::Logf("Frame pacing: target %.2f Hz with %d FPS stream",
	            m_RefreshRate, m_MaxVideoFps);

	// Start the timing emulator thread
	if (!m_VsyncEmulator.joinable()) {
		m_VsyncEmulator = std::thread(&Pacer::vsyncEmulator, this);
	}

	// Start the dedicated vblank pacing thread
	if (!m_BackPacerThread.joinable()) {
		m_BackPacerThread = std::thread(&Pacer::backPacer, this);
	}
}

// Vsync Emulator

// Sleep until approximately targetQpc, then busy-wait to land precisely.
// sleepSlackUs: how early (in microseconds) to stop sleeping and start spinning
// tightSpinUs: within this final window, use a tight YieldProcessor loop
static inline void SleepUntilQpc(int64_t targetQpc,
                                 int64_t sleepSlackUs = 1500,
                                 int64_t tightSpinUs = 150) {
	const int64_t f = QpcFreq();
	const int64_t sleepSlackQpc = UsToQpc(sleepSlackUs);
	const int64_t tightSpinQpc = UsToQpc(tightSpinUs);

	for (;;) {
		const int64_t now = QpcNow();
		int64_t remaining = targetQpc - now;
		if (remaining <= 0) break;

		if (remaining > sleepSlackQpc) {
			int64_t toSleepQpc = remaining - sleepSlackQpc;
			DWORD ms = (DWORD)((toSleepQpc * 1000) / f);
			if (ms == 0) ms = 1;
			Sleep(ms);
			continue;
		}

		if (remaining > tightSpinQpc) {
			YieldProcessor();
			continue;
		}

		do {
			YieldProcessor();
		} while ((targetQpc - QpcNow()) > 0);
		break;
	}
}

void Pacer::vsyncEmulator() {
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST)) {
		Utils::Logf("Failed to set vsyncEmulator priority: %d\n", GetLastError());
	}

	QpcRationalPeriod period;
	uint32_t hzNum = 60;
	uint32_t hzDen = 1;

	switch ((int)std::round(m_RefreshRate * 100)) {
	case 5994:
		hzNum = 60000;
		hzDen = 1001;
		break;
	case 6000:
		hzNum = 60;
		hzDen = 1;
		break;
	case 11988:
		hzNum = 120000;
		hzDen = 1001;
		break;
	case 12000:
		hzNum = 120;
		hzDen = 1;
		break;
	}

	Utils::Logf("Pacer: emulating vsync for %.2fhz using integer rate %lu/%lu\n",
	            m_RefreshRate, hzNum, hzDen);

	static constexpr int64_t remeasureEveryFrames = 3600; // 30s at 120hz
	uint64_t frames = 0;
	int64_t baseQpc = 0;
	int64_t lastVsync = 0;

	while (!stopping()) {
		int64_t curDeadlineQpc = period.nextDeadlineQpc;

		if (baseQpc == 0 || frames % remeasureEveryFrames == 0) {
			// Align to a real vblank edge at startup and at regular intervals
			m_DeviceResources->GetDXGIOutput()->WaitForVBlank();
			baseQpc = QpcNow();
			period.initFromHz(hzNum, hzDen, baseQpc);

			curDeadlineQpc = baseQpc;
		} else {
			// Emulate the vsync interval
			SleepUntilQpc(period.nextDeadlineQpc);
		}

		// Compute next period
		period.step();
		++frames;

		// signal vsync thread, passing it the next deadline
		{
			std::lock_guard<std::mutex> lock(m_VsyncMutex);
			++m_VsyncSeq;
			m_VsyncNextDeadlineQpc = period.nextDeadlineQpc;
		}
		m_VsyncSignalled.notify_one();

		ImGuiPlots::instance().observeFloat(PLOT_VSYNC_INTERVAL, (float)QpcToMs(period.nextDeadlineQpc - curDeadlineQpc));
	}
}

void Pacer::backPacer() {
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL)) {
		Utils::Logf("Failed to set vsyncThread priority: %d\n", GetLastError());
	}

	uint64_t seenSeq = 0;

	for (;;) {
		int64_t nextDeadlineQpc = 0;

		// Wait for either stop or a new vsync tick
		{
			std::unique_lock<std::mutex> lock(m_VsyncMutex);
			m_VsyncSignalled.wait(lock, [this, &seenSeq] {
				return stopping() || (m_VsyncSeq != seenSeq);
			});

			if (stopping()) break;

			seenSeq = m_VsyncSeq;
			nextDeadlineQpc = m_VsyncNextDeadlineQpc;
		}

		handleVsync(nextDeadlineQpc);

		LARGE_INTEGER t0;
		static int64_t lastT0 = 0;
		QueryPerformanceCounter(&t0);
		FQLog("vsyncThread interval %.3f ms\n", QpcToMs(t0.QuadPart - lastT0));
		lastT0 = t0.QuadPart;
	}
}

// XXX rename this
void Pacer::handleVsync(int64_t nextDeadlineQpc) {
	// Compute remaining time until the next edge, then subtract slack.
	const int64_t now = QpcNow();
	int64_t remainingQpc = nextDeadlineQpc - now;
	if (remainingQpc < 0) remainingQpc = 0;

	const int64_t slackQpc = UsToQpc((int64_t)TIMER_SLACK_MS * 1000);
	int64_t waitQpc = (remainingQpc > slackQpc) ? (remainingQpc - slackQpc) : 0;
	const auto waitUs = std::chrono::microseconds(QpcToUs(waitQpc));

	std::unique_lock<std::mutex> lock(m_FrameQueueLock);

	// If the queue length history entries are large, be strict
	// about dropping excess frames.
	int frameDropTarget = 1;

	// If we may get more frames per second than we can display, use
	// frame history to drop frames only if consistently above the
	// one queued frame mark.
	// XXX this should match for 119.88 now
	if (m_MaxVideoFps >= m_RefreshRate) {
		for (int queueHistoryEntry : m_PacingQueueHistory) {
			if (queueHistoryEntry <= 1) {
				// Be lenient as long as the queue length
				// resolves before the end of frame history
				frameDropTarget = 3;
				break;
			}
		}

		// Keep a rolling 500 ms window of pacing queue history
		if (m_PacingQueueHistory.size() == m_RefreshRate / 2) {
			m_PacingQueueHistory.pop_front();
		}

		m_PacingQueueHistory.push_back((int)m_PacingQueue.size());
	}

	// Catch up if we're several frames ahead
	int dropCount = 0;
	while (m_PacingQueue.size() > frameDropTarget) {
		++dropCount;
		AVFrame *frame = std::move(m_PacingQueue.front());
		m_PacingQueue.pop_front();

		FQLog("! pacing queue overflow: %d > frame drop target %d, dropping oldest frame (pts %.3f ms)\n",
		      m_PacingQueue.size() + 1, frameDropTarget, frame->pts / 90.0);
		m_DeviceResources->GetStats()->SubmitDroppedFrame(1);

		// Drop the lock while we call av_frame_free()
		lock.unlock();
		av_frame_free(&frame);
		lock.lock();
	}

	// Don't graph 0 here, because it's already being done in dropFrameForEnqueue
	if (dropCount > 0) {
		ImGuiPlots::instance().observeFloat(PLOT_DROPPED_PACER_BACK, (float)dropCount);
	}

	if (m_PacingQueue.empty()) {
		const int64_t t0 = QpcNow();
		m_PacingQueueNotEmpty.wait_for(lock, waitUs, [this] {
			return stopping() || !m_PacingQueue.empty();
		});
		const int64_t t1 = QpcNow();
		FQLog("handleVsync waited for m_PacingQueueNotEmpty for %.3f ms\n", QpcToMs(t1 - t0));

		if (stopping() || m_PacingQueue.empty()) {
			return;
		}
	} else {
		FQLog("handleVsync handling pacingQueue of size %d\n", m_PacingQueue.size());
	}

	// Place the first frame on the render queue
	AVFrame *frame = std::move(m_PacingQueue.front());
	m_PacingQueue.pop_front();
	lock.unlock();

	// try to time this as close to deadline as possible
	remainingQpc = nextDeadlineQpc - QpcNow();
	if (remainingQpc > 150) {
		SleepUntilQpc(nextDeadlineQpc);
	}

	enqueueFrameForRenderingAndUnlock(frame);
}

// Main thread

void Pacer::waitForFrame() {
	// Wait for the renderer to be ready for the next frame
	LARGE_INTEGER t0, t1;
	QueryPerformanceCounter(&t0);

	HANDLE flw = m_DeviceResources->GetFrameLatencyWaitable();
	WaitForSingleObjectEx(flw, 1000, true);

	QueryPerformanceCounter(&t1);
	FQLog("waitForFrame part 1 waited %.3f ms\n", QpcToMs(t1.QuadPart - t0.QuadPart));

	std::unique_lock<std::mutex> lock(m_FrameQueueLock);
	m_RenderQueueNotEmpty.wait(lock, [this] {
		return stopping() || !m_RenderQueue.empty();
	});

	int64_t t2 = QpcNow();
	FQLog("waitForFrame part 2 waited %.3f ms\n", QpcToMs(t2 - t1.QuadPart));
}

bool Pacer::renderOnMainThread(std::shared_ptr<VideoRenderer> &sceneRenderer) {
	AVFrame *frame = nullptr;

	{
		std::scoped_lock<std::mutex> lock(m_FrameQueueLock);
		if (!m_RenderQueue.empty()) {
			frame = std::move(m_RenderQueue.front());
			m_RenderQueue.pop_front();
		}
	}

	if (!frame) {
		return false; // no frame, don't Present()
	}

	frontPacer(sceneRenderer, frame);
	return true; // ok to Present()
}

void Pacer::enqueueFrameForRenderingAndUnlock(AVFrame *frame) {
	{
		std::scoped_lock<std::mutex> lock(m_FrameQueueLock);
		dropFrameForEnqueue(m_RenderQueue, PLOT_DROPPED_PACER_FRONT);
		m_RenderQueue.push_back(frame);
	}

	// notify render loop of new frame
	FQLog("enqueueFrame notifying m_RenderQueueNotEmpty\n");
	m_RenderQueueNotEmpty.notify_one();
}

void Pacer::frontPacer(std::shared_ptr<VideoRenderer> &sceneRenderer, AVFrame *frame) {
	// Count time spent in Pacer's queues
	LARGE_INTEGER beforeRender, afterRender;
	QueryPerformanceCounter(&beforeRender);

	// Render it
	sceneRenderer->Render(frame);
	QueryPerformanceCounter(&afterRender);

	if (frame->opaque_ref) {
		auto *data = reinterpret_cast<MLFrameData *>(frame->opaque_ref->data);

		m_DeviceResources->GetStats()->SubmitPacerTime(
		    beforeRender.QuadPart - data->decodeEndQpc,
		    afterRender.QuadPart - beforeRender.QuadPart);
	}

	av_frame_free(&frame);

	// Drop frames if we have too many queued up for a while
	std::unique_lock<std::mutex> lock(m_FrameQueueLock);

	int frameDropTarget = m_FrameDropTarget;
	for (int queueHistoryEntry : m_RenderQueueHistory) {
		if (queueHistoryEntry == 0) {
			// Be lenient as long as the queue length
			// resolves before the end of frame history
			frameDropTarget += 2;
			break;
		}
	}

	// Keep a rolling 500 ms window of render queue history
	if (m_RenderQueueHistory.size() == m_MaxVideoFps / 2) {
		m_RenderQueueHistory.pop_front();
	}

	m_RenderQueueHistory.push_back((int)m_RenderQueue.size());

	// Catch up if we're several frames ahead
	int dropCount = 0;
	while (m_RenderQueue.size() > frameDropTarget) {
		++dropCount;
		AVFrame *frame = std::move(m_RenderQueue.front());
		m_RenderQueue.pop_front();

		FQLog("! render queue overflow: %d > frame drop target %d, dropping oldest frame (pts %.3f ms)\n",
		      m_RenderQueue.size() + 1, frameDropTarget, frame->pts / 90.0);
		m_DeviceResources->GetStats()->SubmitDroppedFrame(1);

		// Drop the lock while we call av_frame_free()
		lock.unlock();
		av_frame_free(&frame);
		lock.lock();
	}

	lock.unlock();

	// Don't graph 0 here, because it's already being done in dropFrameForEnqueue
	if (dropCount > 0) {
		ImGuiPlots::instance().observeFloat(PLOT_DROPPED_PACER_FRONT, (float)dropCount);
	}
}

void Pacer::dropFrameForEnqueue(std::deque<AVFrame *> &queue, int plotId) {
	int dropCount = 0;
	if (queue.size() >= MAX_QUEUED_FRAMES) {
		AVFrame *frame = std::move(queue.front());
		queue.pop_front();

		FQLog("! queue full (%d), dropping oldest frame (pts %.3f ms)\n",
		      queue.size() + 1, frame->pts / 90.0);
		m_DeviceResources->GetStats()->SubmitDroppedFrame(1);

		av_frame_free(&frame);
		dropCount = 1;
	}

	ImGuiPlots::instance().observeFloat(plotId, (float)dropCount);
}

void Pacer::submitFrame(AVFrame *frame) {
	// Make sure initialize() has been called
	assert(m_MaxVideoFps != 0);

	// Queue the frame and possibly wake up the render thread
	{
		std::scoped_lock<std::mutex> lock(m_FrameQueueLock);

		dropFrameForEnqueue(m_PacingQueue, PLOT_DROPPED_PACER_BACK);
		m_PacingQueue.push_back(frame);
	}

	// notify pacing loop of new frame
	FQLog("submitFrame notifying m_PacingQueueNotEmpty\n");
	m_PacingQueueNotEmpty.notify_one();
}

int Pacer::getFrameDropTarget() {
	return m_FrameDropTarget;
}

int Pacer::modifyFrameDropTarget(bool increase) {
	int current = m_FrameDropTarget;
	if (increase) {
		if (current >= 5) {
			return current;
		}
		return ++m_FrameDropTarget;
	} else {
		if (current == 0) {
			return current;
		}
		return --m_FrameDropTarget;
	}
}
