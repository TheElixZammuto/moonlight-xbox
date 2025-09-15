#include "pch.h"
#include "Pacer.h"
#include "FFmpegDecoder.h"
#include "../Plot/ImGuiPlots.h"
#include "Utils.hpp"

// Limit the number of queued frames to prevent excessive memory consumption
// if the V-Sync source or renderer is blocked for a while. It's important
// that the sum of all queued frames between both pacing and rendering queues
// must not exceed the number buffer pool size to avoid running the decoder
// out of available decoding surfaces.
#define MAX_QUEUED_FRAMES 4

Pacer::Pacer(const std::shared_ptr<DX::DeviceResources>& res) :
    m_DeviceResources(res),
    m_Stopping(false),
    m_MaxVideoFps(0),
    m_DisplayFps(0.0f),
    m_FrameDropTarget(1)
{

}

Pacer::~Pacer()
{
    m_Stopping = true;
    m_RenderQueueNotEmpty.notify_all();

    // Delete any remaining unconsumed frames
    while (!m_RenderQueue.empty()) {
        AVFrame* frame = std::move(m_RenderQueue.front());
        m_RenderQueue.pop_front();
        av_frame_free(&frame);
    }
}

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

void Pacer::initialize(int maxVideoFps, double refreshRate)
{
    m_MaxVideoFps = maxVideoFps;
    m_DisplayFps = refreshRate;

    Utils::Logf("Frame pacing: target %.2f Hz with %d FPS stream",
                m_DisplayFps, m_MaxVideoFps);
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
        MLFrameData *data = (MLFrameData *)frame->opaque_ref->data;

        m_DeviceResources->GetStats()->SubmitPacerTime(
            beforeRender.QuadPart - data->decodeEndQpc,
            afterRender.QuadPart - beforeRender.QuadPart);
    }

    av_frame_free(&frame);

    // Drop frames if we have too many queued up for a while
    m_FrameQueueLock.lock();

    // these were originally 0 and 2
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

		FQLog("!! pacer queue overflow: %d > frame drop target %d, dropping oldest frame (pts %.3f ms)\n",
            m_RenderQueue.size() + 1, frameDropTarget, (frame->pts / 90000.0) * 1000);
		m_DeviceResources->GetStats()->SubmitDroppedFrame(1);

        // Drop the lock while we call av_frame_free()
        m_FrameQueueLock.unlock();
        av_frame_free(&frame);
        m_FrameQueueLock.lock();
    }

    m_FrameQueueLock.unlock();

    // Don't graph 0 here, because it's already being done in dropFrameForEnqueue
    if (dropCount > 0) {
        ImGuiPlots::instance().observeFloat(PLOT_DROPPED_PACER, (float)dropCount);
    }
}

void Pacer::dropFrameForEnqueue(std::deque<AVFrame*>& queue)
{
    assert(queue.size() <= MAX_QUEUED_FRAMES);

    int dropCount = 0;
    if (queue.size() == MAX_QUEUED_FRAMES) {
        AVFrame* frame = std::move(queue.front());
        queue.pop_front();

        FQLog("!! pacer queue full (%d), dropping oldest frame (pts %.3f ms)\n",
            queue.size() + 1, (frame->pts / 90000.0) * 1000);
		m_DeviceResources->GetStats()->SubmitDroppedFrame(1);

        av_frame_free(&frame);
        dropCount = 1;
    }

    ImGuiPlots::instance().observeFloat(PLOT_DROPPED_PACER, (float)dropCount);
}

void Pacer::submitFrame(AVFrame* frame)
{
    // Make sure initialize() has been called
    assert(m_MaxVideoFps != 0);

    // Queue the frame and possibly wake up the render thread
    {
        std::scoped_lock<std::mutex> lock(m_FrameQueueLock);
        dropFrameForEnqueue(m_RenderQueue);
        m_RenderQueue.push_back(frame);

        size_t queueSize = m_RenderQueue.size();
        ImGuiPlots::instance().observeFloat(PLOT_QUEUED_FRAMES, (float)queueSize);
    }

    // notify render loop of new frame
    m_RenderQueueNotEmpty.notify_one();
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
