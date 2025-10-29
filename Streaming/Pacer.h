#pragma once

#include <atomic>
#include <deque>
#include <thread>
#include "PacerRational.h"
#include "Utils.hpp"
#include "VideoRenderer.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

class Pacer {
  public:
	// Singleton accessor
	static Pacer &instance();

	void deinit();
	void init(const std::shared_ptr<DX::DeviceResources> &res, int maxVideoFps, double refreshRate);
	void waitForFrame();
	bool renderOnMainThread(std::shared_ptr<moonlight_xbox_dx::VideoRenderer> &sceneRenderer);
	void waitUntilPresentTarget();
	void afterPresent(int64_t presentTimeQpc);
	void submitFrame(AVFrame *frame);

  private:
	Pacer();
	Pacer(const Pacer &) = delete;
	Pacer &operator=(const Pacer &) = delete;

	inline bool stopping() const noexcept {
		return m_Stopping.load(std::memory_order_acquire);
	}

	inline bool running() const noexcept {
		return m_Running.load(std::memory_order_acquire);
	}

	void vsyncEmulator();
	void vsyncHardware();
	void backPacer();
	void handleVsync(int64_t nextDeadlineQpc);
	void enqueueFrameForRenderingAndUnlock(AVFrame *frame);
	void frontPacer(std::shared_ptr<moonlight_xbox_dx::VideoRenderer> &sceneRenderer, AVFrame *frame);
	void dropFrameForEnqueue(std::deque<AVFrame *> &queue);
	int64_t waitForVBlank();

	std::shared_ptr<DX::DeviceResources> m_DeviceResources;
	std::thread m_VsyncThread;
	std::thread m_BackPacerThread;
	std::deque<AVFrame *> m_RenderQueue;
	std::deque<AVFrame *> m_PacingQueue;
	std::deque<int> m_RenderQueueHistory;
	std::deque<int> m_PacingQueueHistory;
	std::mutex m_FrameQueueLock;
	std::condition_variable m_RenderQueueNotEmpty;
	std::condition_variable m_PacingQueueNotEmpty;
	std::atomic<bool> m_Running{false};
	std::atomic<bool> m_Stopping{false};
	int m_StreamFps;
	double m_RefreshRate;

	QpcRationalPeriod m_VsyncPeriod;
	int64_t m_VsyncNextDeadlineQpc;
	int64_t m_PresentTargetQpc;
	int64_t m_PresentVsyncQpc;
	std::mutex m_VsyncMutex;
	std::condition_variable m_VsyncSignalled;
	uint64_t m_VsyncSeq;
	double m_AvgDriftMs;
};
