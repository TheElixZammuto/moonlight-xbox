#pragma once

#include "pch.h"
#include <mutex>
#include <string>
#include "../Common/StepTimer.h"
#include "../Utils/FloatBuffer.h"

#include "BandwidthTracker.h"
#include "../Utils/SlidingWindowCounter.h"

extern "C" {
	#include "Limelight.h"
}

typedef enum {
	VSYNC_ON      = 1,        // vsync is enabled
	VSYNC_OFF     = (1 << 1), // vsync is disabled
	VRR_SUPPORTED = (1 << 2), // console is in VRR mode but it's not being used
	VRR_ON        = (1 << 3)  // we're using ALLOW_TEARING Present mode in fullscreen mode (not yet possible)
} SyncMode;

typedef struct _VIDEO_STATS {
	uint32_t receivedFrames;
	uint32_t decodedFrames;
	uint32_t renderedFrames;
	uint32_t totalFrames;
	uint32_t networkDroppedFrames;
	uint32_t pacerDroppedFrames;
	uint32_t hitDeadlines;
	uint32_t missedDeadlines;
	uint16_t minHostProcessingLatency;
	uint16_t maxHostProcessingLatency;
	uint32_t totalHostProcessingLatency;
	uint32_t framesWithHostProcessingLatency;
	uint32_t totalReassemblyTimeUs;
	double totalDecodeTimeMs;
	double totalGpuDecodeTimeMs;    // measured GPU decode time (PyroWave only)
	uint32_t gpuTimedFrames;        // frames contributing to totalGpuDecodeTimeMs
	uint64_t totalPacerTimeUs;
	uint64_t totalPreWaitTimeUs;
	uint64_t totalRenderTimeUs;
	uint64_t totalPresentTimeUs;
	double totalPresentDisplayMs;
	uint32_t countPresentDisplayMs;
	uint32_t lastRtt;
	uint32_t lastRttVariance;
	double totalFps;
	double receivedFps;
	double decodedFps;
	double renderedFps;
	double measurementStartTimestamp;

	// RTP packet stats
	uint64_t expectedVideoPackets;
	uint64_t expectedFecPackets;
	uint64_t recoveredVideoPackets;
	uint64_t unrecoveredVideoPackets;
	uint64_t receivedVideoPackets;
	double videoPps;
} VIDEO_STATS, *PVIDEO_STATS;

namespace moonlight_xbox_dx
{
	class Stats
	{
	public:
		// Singleton
		static Stats& instance();
		void Reset();

		bool ShouldUpdateDisplay(DX::StepTimer const& timer, bool isVisible, char* output, size_t length);

		// submitters for various types of data
		void SubmitVideoBytesAndReassemblyTime(uint32_t length, PDECODE_UNIT decodeUnit, uint32_t droppedFrames);
		void SubmitDecodeMs(double decodeMs);
		void SubmitGpuDecodeMs(double gpuMs);
		void SubmitDroppedFrame(int count);
		void SubmitAvgQueueSize(float avgQueueSize);
		void SubmitPacerTime(int64_t pacerTimeQpc);
		void SubmitPresentDisplayMs(double presentDisplayMs);
		void SubmitRenderStats(double preWaitTimeMs, double renderTimeMs, double presentTimeMs, bool hitDeadline);

	private:
		Stats();
		Stats(const Stats&) = delete;
		Stats& operator=(const Stats&) = delete;

		void addVideoStats(DX::StepTimer const& timer, VIDEO_STATS& src, VIDEO_STATS& dst);
		void formatVideoStats(DX::StepTimer const& timer, VIDEO_STATS& stats, char* output, size_t length);
		void sampleRtpStats(); // m_mutex must be held

		std::mutex                           m_mutex;

		// Moonlight stats overlay
		VIDEO_STATS                          m_ActiveWndVideoStats;
		VIDEO_STATS                          m_LastWndVideoStats;
		VIDEO_STATS                          m_GlobalVideoStats;
		RTP_VIDEO_STATS                      m_LastRtpVideoStats = {};
		BandwidthTracker                     m_bwTracker;
		float                                m_avgQueueSize;
		double                               m_avgMbpsSmoothed;
		SlidingWindowCounter                 m_lostPackets{60, 1000};
		SlidingWindowCounter                 m_recoveredPackets{60, 1000};
	};
}
