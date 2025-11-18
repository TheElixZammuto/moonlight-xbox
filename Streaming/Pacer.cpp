// clang-format off
#include "pch.h"
// clang-format on
#include "Pacer.h"
#include <chrono>
#include <thread>
#include <windows.h>
#include "../Plot/ImGuiPlots.h"
#include "FFmpegDecoder.h"
#include "Utils.hpp"

// Frame Pacing operation
//
// This code is derived from moonlight-qt's pacer.cpp, and has substantial improvements to deal with Xbox-specific
// challenges.
//
// 4 threads use this class:
//
// Decoder thread (run from moonlight-common-c because DIRECT_SUBMIT)
//   * submits AVFrame frames to pacing queue via submitFrame()
//   * max frames in pacing queue = 3, will drop oldest frame if it's full (dropped by back pacing)
//
// vsyncEmulator thread:
//   * Creates our own 120hz vsync signal since Xbox UWP only supports 60hz.
//   * attempts to send m_VsyncSignalled as accurately locked to the refresh rate as possible.
//   * uses rational integer math to support 59.94 and 119.88
//   * alternates 1 frame synced to hardware vblank, a 1 frame on timer
//   * since 120hz requires running without vsync, this thread also manages a TearController that schedules
//     Present() calls in an attempt to reduce/hide/move the location of the screen tearing line. It sort of
//     works, but mostly we're still at the mercy of the DWM Compositor. Xbox Series consoles deal with
//     tearing better than Xbox One X.
//
//  vsyncHardware thread:
//   * Used when streaming at 30 or 60fps. Runs with vsync so there is no screen tearing.
//
// backPacer thread (called vsyncThread in moonlight-qt)
//   * wakes up on each signal from the vsync thread, uses same deadline Qpc as emulator thread
//   * maintains pacing queue
//   * moves 1 frame to render queue
//   * passes along current vblank timestamps to the render thread
//   * signals main thread to start rendering
//
// renderOnMainThread
//   * called from main render loop after waiting on GetFrameLatencyWaitableObject and signal from backPacer
//   * calls frontPacer() (renderFrame in moonlight-qt)
//   * Using vblank timestamps passed down from vsync thread, WaitUntilPresentTarget() schedules precise
//     Present() calls, and is somewhat able to control screen tearing location. This also enables butter
//     smooth frame pacing.
//   * Calls AfterPresent() to capture stats and drift timing information, passed to TearController
//
// Other notes and differences from moonlight-qt:
// SwapChain BufferCount = 3
// GetFrameLatencyWaitableObject() is used to wait before each frame
// SetMaximumFrameLatency(1)
// Calls to FQLog() and functions called within FQLog() are no-op unless you define FRAME_QUEUE_VERBOSE in pch.h
// and build in Debug mode.

// Limit the number of queued frames to prevent excessive memory consumption
// if the V-Sync source or renderer is blocked for a while. It's important
// that the sum of all queued frames between both pacing and rendering queues
// must not exceed the number buffer pool size to avoid running the decoder
// out of available decoding surfaces.
#define MAX_QUEUED_FRAMES 3

// We may be woken up slightly late so don't go all the way
// up to the next V-sync since we may accidentally step into
// the next V-sync period. It also takes some amount of time
// to do the render itself, so we can't render right before
// V-sync happens.
#define TIMER_SLACK_MS 2

using steady_clock = std::chrono::steady_clock;
using namespace moonlight_xbox_dx;

Pacer &Pacer::instance() {
	static Pacer inst;
	return inst;
}

Pacer::Pacer()
    : m_Running(false),
      m_DeviceResources(),
      m_VsyncThread(),
      m_BackPacerThread(),
      m_VsyncSeq(0),
      m_VsyncNextDeadlineQpc(0),
      m_Stopping(false),
      m_StreamFps(0),
      m_RefreshRate(0.0),
      m_AvgDriftMs(1.2) {
}

void Pacer::deinit() {
	m_Running.store(false, std::memory_order_release);
	m_Stopping.store(true, std::memory_order_release);
	m_RenderQueueNotEmpty.notify_all();

	// Stop the vsync threads
	m_VsyncSignalled.notify_all();
	m_PacingQueueNotEmpty.notify_all();
	if (m_BackPacerThread.joinable()) {
		m_BackPacerThread.join();
	}
	if (m_VsyncThread.joinable()) {
		m_VsyncThread.join();
	}

	m_DeviceResources = nullptr;

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

	Utils::Logf("Pacer: deinit\n");
}

void Pacer::init(const std::shared_ptr<DX::DeviceResources> &res, int streamFps, double refreshRate) {
	m_Stopping.store(false, std::memory_order_release);
	m_DeviceResources = res;
	m_StreamFps = streamFps;
	m_RefreshRate = refreshRate;

	Utils::Logf("Pacer: init target %.2f Hz with %d FPS stream\n",
	            m_RefreshRate, m_StreamFps);

	// If running on Xbox at 120hz, emulate the vsync interval
	if (m_DeviceResources->isXbox() && m_StreamFps == 120) {
		// Start the timing emulator thread
		if (!m_VsyncThread.joinable()) {
			m_VsyncThread = std::thread(&Pacer::vsyncEmulator, this);
		}
	} else {
		// Much simpler hardware vsync thread
		if (!m_VsyncThread.joinable()) {
			m_VsyncThread = std::thread(&Pacer::vsyncHardware, this);
		}
	}

	// Start the dedicated vblank pacing thread
	if (!m_BackPacerThread.joinable()) {
		m_BackPacerThread = std::thread(&Pacer::backPacer, this);
	}

	m_Running.store(true, std::memory_order_release);
}

// Vsync Emulator

void Pacer::vsyncEmulator() {
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST)) {
		Utils::Logf("Failed to set vsyncEmulator priority: %d\n", GetLastError());
	}

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

	int64_t vsyncQpc = waitForVBlank();
	m_VsyncPeriod.initFromHz(hzNum, hzDen, vsyncQpc, QpcFreq());

	Utils::Logf("Pacer: using emulated integer vsync timer %lu/%lu based on system refresh rate %.2f Hz\n",
	            hzNum, hzDen, m_RefreshRate);

	int64_t baseQpc = 0;
	int64_t lastTick = 0;
	uint64_t lastVblankSeq = 0;

	const bool is120 = m_RefreshRate > 60.0;
	bool tick = true; // tick = hardware vsync, tock = emulated vsync (120hz)
	                  // tick = hardware vsync only (60hz)

	uint64_t emuClockTTL = 2; // resync with hardware every other frame. This can be any even number.
	uint64_t ttl = 0;

	while (!stopping()) {
		if (tick) {
#ifdef FRAME_QUEUE_VERBOSE
			int64_t pre = QpcNow();
#endif
			// Align to a real vblank edge
			baseQpc = waitForVBlank();

			m_VsyncPeriod.initFromHz(hzNum, hzDen, baseQpc, QpcFreq());
			FQLog("resync to hardware vsync, waited %.3fms\n", QpcToMs(baseQpc - pre));
			lastVblankSeq = m_VsyncSeq;

			if (is120) {
				// when running at 120hz, next vsync is emulated
				tick = false;
				ttl = emuClockTTL - 1;
			}
		} else {
#ifdef FRAME_QUEUE_VERBOSE
			int64_t pre = QpcNow();
#endif
			// Emulate the vsync interval
			SleepUntilQpc(m_VsyncPeriod.nextDeadlineQpc);
			FQLog("waited %.3fms for emulated vsync\n", QpcToMs(QpcNow() - pre));

			if (--ttl == 0) {
				// time for a hardware vsync
				tick = true;
			}
		}

		// Compute next period
		m_VsyncPeriod.step();

		// signal vsync thread, passing it the next deadline
		{
			std::lock_guard<std::mutex> lock(m_VsyncMutex);
			++m_VsyncSeq;
			m_VsyncNextDeadlineQpc = m_VsyncPeriod.nextDeadlineQpc;
		}
		m_VsyncSignalled.notify_one();

		int64_t nowTick = QpcNow();
		if (lastTick > 0) {
			FQLog("vsync interval %.3f ms\n", QpcToMs(nowTick - lastTick));
			ImGuiPlots::instance().observeFloat(PLOT_VSYNC_INTERVAL, (float)QpcToMs(nowTick - lastTick));
		}
		lastTick = nowTick;
	}

	Utils::Logf("vsyncEmulator thread stopped\n");
}

void Pacer::vsyncHardware() {
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL)) {
		Utils::Logf("Failed to set vsyncHardware priority: %d\n", GetLastError());
	}

	// The hardware vsync path can be called for the following cases:
	// system 120hz, stream 60 or 30fps
	// system 60hz, stream 60 or 30fps
	// Same but with 119.88hz/59.94hz

	uint32_t hzNum = 60;
	uint32_t hzDen = 1;

	// Detect if we need special NTSC handling
	switch ((int)std::round(m_RefreshRate * 100)) {
	case 5994:
	case 11988:
		hzNum = 60000;
		hzDen = 1001;
		break;
	case 6000:
	case 12000:
		hzNum = 60;
		hzDen = 1;
		break;
	default:
		hzNum = m_StreamFps;
		hzDen = 1;
	}

	if (m_StreamFps == 30) {
		hzNum /= 2;
	}

	int64_t lastT0 = 0;

	// no tearing to worry about, we will just use it for timing
	int64_t vsyncQpc = waitForVBlank();
	m_VsyncPeriod.initFromHz(hzNum, hzDen, vsyncQpc, QpcFreq());

	Utils::Logf("Pacer: using integer vsync timer %lu/%lu based on system refresh rate %.2f Hz\n",
	            hzNum, hzDen, m_RefreshRate);

	bool skipNextVblank = false;

	while (!stopping()) {
		// Align to a real vblank
		int64_t now = waitForVBlank();

		// If we're streaming 30fps, wait 2 vblanks per frame
		if (m_StreamFps == 30 && skipNextVblank) {
			skipNextVblank = false;
			continue;
		}

		m_VsyncPeriod.initFromHz(hzNum, hzDen, now, QpcFreq());

		// Compute next period
		m_VsyncPeriod.step();

		// signal vsync thread, passing it the next deadline
		{
			std::lock_guard<std::mutex> lock(m_VsyncMutex);
			++m_VsyncSeq;
			m_VsyncNextDeadlineQpc = m_VsyncPeriod.nextDeadlineQpc;
		}
		m_VsyncSignalled.notify_one();

		if (lastT0 > 0) {
			ImGuiPlots::instance().observeFloat(PLOT_VSYNC_INTERVAL, (float)QpcToMs(now - lastT0));
		}
		lastT0 = now;

		if (m_StreamFps == 30 && !skipNextVblank) {
			skipNextVblank = true;
		}
	}

	Utils::Logf("vsyncHardware thread stopped\n");
}

void Pacer::backPacer() {
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL)) {
		Utils::Logf("Failed to set backPacer priority: %d\n", GetLastError());
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

#ifdef FRAME_QUEUE_VERBOSE
		int64_t now = QpcNow();
		static int64_t last = 0;
		FQLog("backPacer interval %.3f ms\n", QpcToMs(now - last));
		last = now;
#endif
	}

	Utils::Logf("backPacer thread stopped\n");
}

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
	if (m_StreamFps >= std::round(m_RefreshRate)) {
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
		AVFrame *frame = m_PacingQueue.front();
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
		ImGuiPlots::instance().observeFloat(PLOT_DROPPED_PACER, (float)dropCount);
	}

	if (m_PacingQueue.empty()) {
#ifdef FRAME_QUEUE_VERBOSE
		const int64_t t0 = QpcNow();
#endif
		m_PacingQueueNotEmpty.wait_for(lock, waitUs, [this] {
			return stopping() || !m_PacingQueue.empty();
		});
		FQLog("handleVsync waited for m_PacingQueueNotEmpty for %.3f ms\n", QpcToMs(QpcNow() - t0));

		if (stopping() || m_PacingQueue.empty()) {
			return;
		}
	} else {
		FQLog("handleVsync handling pacingQueue of size %d\n", m_PacingQueue.size());
	}

	// Place the first frame on the render queue
	AVFrame *frame = m_PacingQueue.front();
	m_PacingQueue.pop_front();

	// Pass along the target timestamp with the frame metadata
	if (frame->opaque_ref) {
		auto *data = reinterpret_cast<MLFrameData *>(frame->opaque_ref->data);
		data->presentVsyncQpc = nextDeadlineQpc;

		// Adjust Present() timing, goal is to present as close to nextDeadline without going over
		static const int64_t safetyQpc = MsToQpc(0.3);
		data->presentTargetQpc = nextDeadlineQpc - MsToQpc(m_AvgDriftMs) - safetyQpc;
		if (data->presentTargetQpc > nextDeadlineQpc) {
			data->presentTargetQpc = nextDeadlineQpc - safetyQpc;
		}
	}

	lock.unlock();

	// Render frame immediately, it will wait to be presented at presentTargetQpc
	enqueueFrameForRenderingAndUnlock(frame);
}

void Pacer::enqueueFrameForRenderingAndUnlock(AVFrame *frame) {
	{
		std::scoped_lock<std::mutex> lock(m_FrameQueueLock);
		dropFrameForEnqueue(m_RenderQueue);
		m_RenderQueue.push_back(frame);
	}

	// notify render loop of new frame
	m_RenderQueueNotEmpty.notify_one();
}

// Main render thread

void Pacer::waitForFrame() {
	if (!running()) return;

	// Wait for the renderer to be ready for the next frame
#ifdef FRAME_QUEUE_VERBOSE
	int64_t t0 = QpcNow();
#endif

	HANDLE flw = m_DeviceResources->GetFrameLatencyWaitable();
	if (WaitForSingleObjectEx(flw, 1000, true) == WAIT_TIMEOUT) {
		Utils::Logf("Pacer::waitForFrame(): warning: waited over 1000ms for FrameLatencyWaitable\n");
	}

	FQLog("waitForFrame(): FrameLatencyWaitable waited %.3f ms\n", QpcToMs(QpcNow() - t0));

	std::unique_lock<std::mutex> lock(m_FrameQueueLock);
	m_RenderQueueNotEmpty.wait(lock, [this] {
		return stopping() || !m_RenderQueue.empty();
	});
}

// called by render thread
bool Pacer::renderOnMainThread(std::shared_ptr<VideoRenderer> &sceneRenderer) {
	if (!running()) return false;

	AVFrame *frame = nullptr;

	{
		std::scoped_lock<std::mutex> lock(m_FrameQueueLock);
		if (!m_RenderQueue.empty()) {
			frame = m_RenderQueue.front();
			m_RenderQueue.pop_front();
		}
	}

	if (!frame) {
		return false; // no frame, don't Present()
	}

	frontPacer(sceneRenderer, frame);
	return true; // ok to Present()
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

		// Cache the target present time for this frame
		m_PresentTargetQpc = data->presentTargetQpc;
		m_PresentVsyncQpc = data->presentVsyncQpc;
	} else {
		m_PresentTargetQpc = 0;
	}

	av_frame_free(&frame);

	// Drop frames if we have too many queued up for a while
	std::unique_lock<std::mutex> lock(m_FrameQueueLock);

	int frameDropTarget = 0;
	for (int queueHistoryEntry : m_RenderQueueHistory) {
		if (queueHistoryEntry == 0) {
			// Be lenient as long as the queue length
			// resolves before the end of frame history
			frameDropTarget = 2;
			break;
		}
	}

	// Keep a rolling 500 ms window of render queue history
	if (m_RenderQueueHistory.size() == m_StreamFps / 2) {
		m_RenderQueueHistory.pop_front();
	}

	m_RenderQueueHistory.push_back((int)m_RenderQueue.size());

	// Catch up if we're several frames ahead
	int dropCount = 0;
	while (m_RenderQueue.size() > frameDropTarget) {
		++dropCount;
		AVFrame *frame = m_RenderQueue.front();
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
		ImGuiPlots::instance().observeFloat(PLOT_DROPPED_PACER, (float)dropCount);
	}
}

// called by render thread
void Pacer::waitUntilPresentTarget() {
	if (!running()) return;

	FQLog("waitUntilPresentTarget(): waiting for %.3fms\n", QpcToMs(m_PresentTargetQpc - QpcNow()));

	SleepUntilQpc(m_PresentTargetQpc);
}

// called by render thread
void Pacer::afterPresent(int64_t presentTimeQpc) {
	if (!running()) return;

	// Called immediately after Present(), check our timings and adjust future timings
	const int64_t driftQpc = presentTimeQpc - m_PresentVsyncQpc;
	double driftMs = QpcToMs(driftQpc);

	{
		std::scoped_lock<std::mutex> lock(m_FrameQueueLock);
		// weighted moving average of drift
		const double alpha = 0.05f;
		m_AvgDriftMs = (driftMs * alpha) + (m_AvgDriftMs * (1.0 - alpha));
	}

	// technically present-to-slightly-before-display
	FQLog("Present-to-present drift %.3fms\n", driftMs);

	m_DeviceResources->GetStats()->SubmitPresentPacing(driftMs);
	ImGuiPlots::instance().observeFloat(PLOT_PRESENT_PACING, (float)driftMs);
}

// end main thread

void Pacer::dropFrameForEnqueue(std::deque<AVFrame *> &queue) {
	int dropCount = 0;
	if (queue.size() >= MAX_QUEUED_FRAMES) {
		AVFrame *frame = queue.front();
		queue.pop_front();
		FQLog("! queue full (%d), dropping oldest frame (pts %.3f ms)\n",
		      queue.size() + 1, frame->pts / 90.0);
		m_DeviceResources->GetStats()->SubmitDroppedFrame(1);
		av_frame_free(&frame);
		dropCount = 1;
	}

	const bool isPacingQueue = (&queue == &m_PacingQueue);

	// when this is zero, only count it for the pacing queue
	if (dropCount > 0 || isPacingQueue) {
		ImGuiPlots::instance().observeFloat(PLOT_DROPPED_PACER, (float)dropCount);
	}
}

void Pacer::submitFrame(AVFrame *frame) {
	// Queue the frame and possibly wake up the render thread
	{
		std::scoped_lock<std::mutex> lock(m_FrameQueueLock);
		dropFrameForEnqueue(m_PacingQueue);
		m_PacingQueue.push_back(frame);
	}

	// notify pacing loop of new frame
	FQLog("submitFrame notifying m_PacingQueueNotEmpty\n");
	m_PacingQueueNotEmpty.notify_one();
}

// Misc helper functions

inline int64_t Pacer::waitForVBlank() {
	m_DeviceResources->GetDXGIOutput()->WaitForVBlank();
	return QpcNow();
}
