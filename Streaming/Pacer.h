#pragma once

#include <atomic>
#include <deque>
#include <set>
#include <thread>
#include <utility>
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
	void waitForFrame(double timeoutMs);
	bool renderOnMainThread(std::shared_ptr<moonlight_xbox_dx::VideoRenderer> &sceneRenderer);
	void waitBeforePresent();
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

	void vsyncHardware();
	void updateFrameStats();
	int64_t getNextVBlankQpc(int64_t *now, int64_t *interval);

	std::shared_ptr<DX::DeviceResources> m_DeviceResources;
	std::thread m_VsyncThread;
	std::atomic<bool> m_Running{false};
	std::atomic<bool> m_Stopping{false};
	int m_StreamFps;
	double m_RefreshRate;
	int64_t m_BeginFrameQpc = 0;

	static constexpr int VSYNC_HISTORY_SIZE = 512;
	std::mutex m_FrameStatsLock;
	UINT m_LastSyncRefreshCount;
	int64_t m_LastSyncQpc;
	int64_t m_VsyncIntervalQpc;
	std::array<int64_t, VSYNC_HISTORY_SIZE> m_vhistory{};
	int m_vhcount = 0;
	int m_vhidx = 0;
	int64_t m_vhsum = 0;
	std::atomic<int64_t> m_LastSyncTarget{0};
	double m_ewmaVsyncDriftQpc = 1;
};
