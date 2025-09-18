#include "pch.h"
#include "Pacer.h"
#include "FFmpegDecoder.h"
#include "../Plot/ImGuiPlots.h"
#include "Utils.hpp"
#include <windows.h>
#include <chrono>
#include <thread>

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
#define TIMER_SLACK_MS 3

using steady_clock = std::chrono::steady_clock;
using namespace moonlight_xbox_dx;

Pacer::Pacer(const std::shared_ptr<DX::DeviceResources>& res) :
    m_DeviceResources(res),
    m_VsyncThread(),
    m_Stopping(false),
    m_MaxVideoFps(0),
    m_DisplayFps(0.0),
    m_FrameDropTarget(0)
{

}

Pacer::~Pacer()
{
    m_Stopping = true;
    m_RenderQueueNotEmpty.notify_all();

    // Stop the V-sync thread
    m_PacingQueueNotEmpty.notify_all();
    if (m_VsyncThread.joinable()) {
        m_VsyncThread.join();
    }

    // Drain and free any remaining frames in both queues
    {
        std::unique_lock<std::mutex> lock(m_FrameQueueLock);

        while (!m_RenderQueue.empty()) {
            AVFrame* f = m_RenderQueue.front();
            m_RenderQueue.pop_front();
            lock.unlock();
            av_frame_free(&f);
            lock.lock();
        }
        while (!m_PacingQueue.empty()) {
            AVFrame* f = m_PacingQueue.front();
            m_PacingQueue.pop_front();
            lock.unlock();
            av_frame_free(&f);
            lock.lock();
        }
    }
}

void Pacer::initialize(int maxVideoFps, double refreshRate)
{
    m_MaxVideoFps = maxVideoFps;
    m_DisplayFps = refreshRate;

    Utils::Logf("Frame pacing: target %.2f Hz with %d FPS stream",
                m_DisplayFps, m_MaxVideoFps);

    // Start the dedicated vblank pacing thread once
    if (!m_VsyncThread.joinable()) {
        m_VsyncThread = std::thread(&Pacer::vsyncThread, this);
    }
}

// Vsync thread

void Pacer::vsyncThread()
{
    if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL)) {
        Utils::Logf("Failed to set vsyncThread priority: %d\n", GetLastError());
    }

    while (!m_Stopping) {
        // See comments in waitForVsync()

        LARGE_INTEGER beforeWait, afterWait;
        // QueryPerformanceCounter(&beforeWait);
        // waitForVsync();
        // QueryPerformanceCounter(&afterWait);
        // FQLog("waitForVsync() waited %.3f ms\n", QpcToMs(afterWait.QuadPart - beforeWait.QuadPart));

        // if (m_Stopping) {
        //     break;
        // }

        QueryPerformanceCounter(&beforeWait);
        handleVsync(1000.0 / m_DisplayFps);
        QueryPerformanceCounter(&afterWait);
        FQLog("handleVsync() waited %.3f ms\n", QpcToMs(afterWait.QuadPart - beforeWait.QuadPart));
    }
}

void Pacer::waitForVsync()
{
    // XXX for some reason, both of these vsync methods seem to run at 60hz even when Xbox is running at 120hz
    // We can probably just run without them, but perfect smoothness probably requires some vsync support

    // might work but is tied to a single display
    //m_DeviceResources->GetDXGIOutput()->WaitForVBlank();

    //HANDLE flw = m_DeviceResources->GetFrameLatencyWaitable();
    //WaitForSingleObject(flw, 2000 /*ms timeout*/);
}

// Called in an arbitrary thread by the IVsyncSource on V-sync
// or an event synchronized with V-sync
void Pacer::handleVsync(double timeUntilNextVsyncMillis)
{
    // Make sure initialize() has been called
    assert(m_MaxVideoFps != 0);

    std::unique_lock<std::mutex> lock(m_FrameQueueLock);

    // If the queue length history entries are large, be strict
    // about dropping excess frames.
    int frameDropTarget = 1;

    // If we may get more frames per second than we can display, use
    // frame history to drop frames only if consistently above the
    // one queued frame mark.
    if (m_MaxVideoFps >= m_DisplayFps) {
        for (int queueHistoryEntry : m_PacingQueueHistory) {
            if (queueHistoryEntry <= 1) {
                // Be lenient as long as the queue length
                // resolves before the end of frame history
                frameDropTarget = 3;
                break;
            }
        }

        // Keep a rolling 500 ms window of pacing queue history
        if (m_PacingQueueHistory.size() == m_DisplayFps / 2) {
            m_PacingQueueHistory.pop_front();
        }

        m_PacingQueueHistory.push_back((int)m_PacingQueue.size());
    }

    // Catch up if we're several frames ahead
    int dropCount = 0;
    while (m_PacingQueue.size() > frameDropTarget) {
        ++dropCount;
        AVFrame* frame = std::move(m_PacingQueue.front());
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
        // Sleep up to the next vblank interval or until a frame arrives
        static constexpr auto kSlack = std::chrono::milliseconds(TIMER_SLACK_MS);
        const auto period_us = std::chrono::microseconds(static_cast<int64_t>(timeUntilNextVsyncMillis * 1000.0 + 0.5));
        const auto remaining = (period_us > kSlack)
            ? (period_us - kSlack)
            : std::chrono::microseconds(0);

        m_PacingQueueNotEmpty.wait_for(lock, remaining, [this]{
            return m_Stopping || !m_PacingQueue.empty();
        });

        if (m_Stopping || m_PacingQueue.empty()) {
            return;
        }
    }

    // Place the first frame on the render queue
    AVFrame* frame = std::move(m_PacingQueue.front());
    m_PacingQueue.pop_front();
    lock.unlock();

    enqueueFrameForRenderingAndUnlock(frame);
}

// Main thread

void Pacer::waitForFrame()
{
    std::unique_lock<std::mutex> lock(m_FrameQueueLock);
    m_RenderQueueNotEmpty.wait(lock, [this]{
        return m_Stopping || !m_RenderQueue.empty();
    });
}

bool Pacer::renderOnMainThread(std::shared_ptr<VideoRenderer>& sceneRenderer)
{
    AVFrame* frame = nullptr;

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

    renderFrame(sceneRenderer, frame);
    return true; // ok to Present()
}

void Pacer::enqueueFrameForRenderingAndUnlock(AVFrame *frame)
{
    {
        std::scoped_lock<std::mutex> lock(m_FrameQueueLock);
        dropFrameForEnqueue(m_RenderQueue, PLOT_DROPPED_PACER_FRONT);
        m_RenderQueue.push_back(frame);

        size_t queueSize = m_RenderQueue.size();
        ImGuiPlots::instance().observeFloat(PLOT_QUEUED_FRAMES, (float)queueSize);
    }

    // notify render loop of new frame
    m_RenderQueueNotEmpty.notify_one();
}

void Pacer::renderFrame(std::shared_ptr<VideoRenderer>& sceneRenderer, AVFrame* frame)
{
    // Count time spent in Pacer's queues
    LARGE_INTEGER beforeRender, afterRender;
    QueryPerformanceCounter(&beforeRender);

    // Render it
    sceneRenderer->Render(frame);
    QueryPerformanceCounter(&afterRender);

    if (frame->opaque_ref) {
        auto* data = reinterpret_cast<MLFrameData*>(frame->opaque_ref->data);

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
        AVFrame* frame = std::move(m_RenderQueue.front());
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

void Pacer::dropFrameForEnqueue(std::deque<AVFrame*>& queue, int plotId)
{
    int dropCount = 0;
    if (queue.size() >= MAX_QUEUED_FRAMES) {
        AVFrame* frame = std::move(queue.front());
        queue.pop_front();

        FQLog("! pacing queue %d full (%d), dropping oldest frame (pts %.3f ms)\n",
            plotId, queue.size() + 1, frame->pts / 90.0);
		m_DeviceResources->GetStats()->SubmitDroppedFrame(1);

        av_frame_free(&frame);
        dropCount = 1;
    }

    ImGuiPlots::instance().observeFloat(plotId, (float)dropCount);
}

void Pacer::submitFrame(AVFrame* frame)
{
    // Make sure initialize() has been called
    assert(m_MaxVideoFps != 0);

    // Queue the frame and possibly wake up the render thread
    {
        std::scoped_lock<std::mutex> lock(m_FrameQueueLock);

        dropFrameForEnqueue(m_PacingQueue, PLOT_DROPPED_PACER_BACK);
        m_PacingQueue.push_back(frame);

        size_t queueSize = m_RenderQueue.size();
        ImGuiPlots::instance().observeFloat(PLOT_QUEUED_FRAMES, (float)queueSize);
    }

    // notify pacing loop of new frame
    m_PacingQueueNotEmpty.notify_one();
}

int Pacer::modifyFrameDropTarget(bool increase) {
    int current = m_FrameDropTarget;
    if (increase) {
        if (current >= 5) {
            return current;
        }
        return ++m_FrameDropTarget;
    }
    else {
        if (current == 0) {
            return current;
        }
        return --m_FrameDropTarget;
    }
}
