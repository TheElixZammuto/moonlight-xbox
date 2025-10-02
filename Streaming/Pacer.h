#pragma once

#include <atomic>
#include <deque>
#include <thread>
#include "Utils.hpp"
#include "VideoRenderer.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

class Pacer {
  public:
	Pacer(const std::shared_ptr<DX::DeviceResources> &res);

	~Pacer();

	void submitFrame(AVFrame *frame);

	void initialize(int maxVideoFps, double refreshRate);

	void waitForFrame();

	bool renderOnMainThread(std::shared_ptr<moonlight_xbox_dx::VideoRenderer> &sceneRenderer);

	void waitUntilPresentTarget();

	int getFrameDropTarget();

	int modifyFrameDropTarget(bool increase);

  private:
	int64_t measureVsyncQpc(int intervals);

	void vsyncEmulator();

	void vsyncHardware();

	void backPacer();

	void handleVsync(int64_t nextDeadlineQpc);

	void enqueueFrameForRenderingAndUnlock(AVFrame *frame);

	void frontPacer(std::shared_ptr<moonlight_xbox_dx::VideoRenderer> &sceneRenderer, AVFrame *frame);

	void dropFrameForEnqueue(std::deque<AVFrame *> &queue, int plotId);

	bool stopping() const noexcept {
		return m_Stopping.load(std::memory_order_acquire);
	}

	std::shared_ptr<DX::DeviceResources> m_DeviceResources;
	std::thread m_VsyncEmulator;
	std::thread m_BackPacerThread;
	std::mutex m_VsyncMutex;
	std::condition_variable m_VsyncSignalled;
	uint64_t m_VsyncSeq;
	int64_t m_VsyncNextDeadlineQpc;
	std::deque<AVFrame *> m_RenderQueue;
	std::deque<AVFrame *> m_PacingQueue;
	std::deque<int> m_RenderQueueHistory;
	std::deque<int> m_PacingQueueHistory;
	std::mutex m_FrameQueueLock;
	std::condition_variable m_RenderQueueNotEmpty;
	std::condition_variable m_PacingQueueNotEmpty;
	int64_t m_PresentTargetQpc;
	std::atomic<bool> m_Stopping{false};
	int m_StreamFps;
	double m_RefreshRate;
	std::atomic_int m_FrameDropTarget;
};
