#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <deque>
#include <iterator>
#include <optional>
#include <set>
#include <thread>
#include <utility>
#include "..\Common\DirectXHelper.h"
#include "FrameCadence.h"
#include "Utils.hpp"
#include "VideoRenderer.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace DX {
	class GpuPerformanceTimer;
}

struct PresentTracker {
	struct Entry {
		UINT presentID;
		int64_t qpcBeforePresent;
	};

	explicit PresentTracker(std::size_t maxPending = 8)
	    : maxPending_(maxPending) {
	}

	// Call after a successful Present and GetLastPresentCount.
	void Record(UINT presentID, int64_t qpcBeforePresent) {
		pending_.push_back({presentID, qpcBeforePresent});
		while (pending_.size() > maxPending_)
			pending_.pop_front();
	}

	std::optional<Entry> Consume(const DXGI_FRAME_STATISTICS &stats) {
		const auto it = std::find_if(
		    pending_.begin(), pending_.end(),
		    [&](const Entry &entry) {
			    return entry.presentID == stats.PresentCount;
		    });

		if (it == pending_.end())
			return std::nullopt;

		const Entry matched = *it;

		// Older entries are no longer needed
		pending_.erase(pending_.begin(), std::next(it));
		return matched;
	}

	void Reset() {
		pending_.clear();
	}

  private:
	std::size_t maxPending_;
	std::deque<Entry> pending_;
};

class Pacer {
  public:
	// Singleton accessor
	static Pacer &instance();

	void deinit();
	void init(const std::shared_ptr<DX::DeviceResources> &res, int maxVideoFps, double refreshRate, bool framePacingImmediate);
	bool getPacingImmediate();
	void setPacingImmediate(bool framePacingImmediate);
	void waitForFrame(double timeoutMs);
	bool renderOnMainThread(std::shared_ptr<moonlight_xbox_dx::VideoRenderer> &sceneRenderer);
	bool waitBeforePresent(int64_t deadline);
	int64_t getCurrentFramePts();
	int64_t getNextVBlankQpc(int64_t *now);
	void submitFrame(AVFrame *frame);
	void recordPresent(UINT presentID, int64_t qpcBeforePresent);

	DX::GpuPerformanceTimer *GetGpuPerformanceTimer() const noexcept {
		return m_GpuPerformanceTimer.get();
	}
	void StartGpuTimerForFrame() {
		if (m_GpuPerformanceTimer) m_GpuPerformanceTimer->StartTimerForFrame();
	}
	void EndGpuTimerForFrame() {
		if (m_GpuPerformanceTimer) m_GpuPerformanceTimer->EndTimerForFrame();
	}

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

	bool renderModeImmediate(std::shared_ptr<moonlight_xbox_dx::VideoRenderer> &sceneRenderer);
	bool renderModeDisplayLocked(std::shared_ptr<moonlight_xbox_dx::VideoRenderer> &sceneRenderer);
	void vsyncHardware();
	void updateFrameStats();

	std::shared_ptr<DX::DeviceResources> m_DeviceResources;
	std::thread m_VsyncThread;
	std::atomic<bool> m_Running{false};
	std::atomic<bool> m_Stopping{false};
	int m_StreamFps;
	double m_RefreshRate;
	std::atomic<bool> m_FramePacingImmediate;
	std::unique_ptr<DX::GpuPerformanceTimer> m_GpuPerformanceTimer;

	FrameCadence m_FrameCadence;
	AVFrame *m_CurrentFrame = nullptr;

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
	PresentTracker m_PresentTracker{};
};
