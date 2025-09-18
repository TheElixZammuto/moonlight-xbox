#pragma once

#include "VideoRenderer.h"
#include "Utils.hpp"
#include <atomic>
#include <deque>
#include <thread>

extern "C"
{
#include <libavcodec/avcodec.h>
}

class Pacer
{
public:
    Pacer(const std::shared_ptr<DX::DeviceResources>& res);

    ~Pacer();

    void submitFrame(AVFrame* frame);

    void initialize(int maxVideoFps, double refreshRate);

    void waitForFrame();

    bool renderOnMainThread(std::shared_ptr<moonlight_xbox_dx::VideoRenderer>& sceneRenderer);

    int modifyFrameDropTarget(bool increase);

private:
    void vsyncThread();

    void waitForVsync();

    void handleVsync(double timeUntilNextVsyncMillis);

    void enqueueFrameForRenderingAndUnlock(AVFrame* frame);

    void renderFrame(std::shared_ptr<moonlight_xbox_dx::VideoRenderer>& sceneRenderer, AVFrame* frame);

    void dropFrameForEnqueue(std::deque<AVFrame*>& queue, int plotId);

    std::shared_ptr<DX::DeviceResources> m_DeviceResources;
    std::thread m_VsyncThread;
    std::deque<AVFrame*> m_RenderQueue;
    std::deque<AVFrame*> m_PacingQueue;
    std::deque<int> m_RenderQueueHistory;
    std::deque<int> m_PacingQueueHistory;
    std::mutex m_FrameQueueLock;
    std::condition_variable m_RenderQueueNotEmpty;
    std::condition_variable m_PacingQueueNotEmpty;
    std::atomic<bool> m_Stopping{false};
    int m_MaxVideoFps;
    double m_DisplayFps;
    std::atomic_int m_FrameDropTarget;
};
