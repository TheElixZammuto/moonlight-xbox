#pragma once

#include "VideoRenderer.h"
#include <atomic>
#include <deque>

extern "C"
{
#include <libavcodec/avcodec.h>
}

using namespace moonlight_xbox_dx;

class Pacer
{
public:
    Pacer(const std::shared_ptr<DX::DeviceResources>& res);

    ~Pacer();

    void submitFrame(AVFrame* frame);

    void initialize(int maxVideoFps, double refreshRate);

    void waitForFrame();

    bool renderOnMainThread(std::shared_ptr<VideoRenderer>& sceneRenderer);

private:
    void renderFrame(std::shared_ptr<VideoRenderer>& sceneRenderer, AVFrame* frame);

    void dropFrameForEnqueue(std::deque<AVFrame*>& queue);

    std::shared_ptr<DX::DeviceResources> m_DeviceResources;
    std::deque<AVFrame*> m_RenderQueue;
    std::deque<int> m_RenderQueueHistory;
    std::condition_variable m_RenderQueueNotEmpty;
    std::mutex m_FrameQueueLock;
    std::atomic<bool> m_Stopping{false};
    int m_MaxVideoFps;
    double m_DisplayFps;
};
