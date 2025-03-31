#include "pch.h"
#include "Stats.h"
#include "Utils.hpp"
#include "../Streaming/FFMpegDecoder.h"

extern "C" {
	#include <Limelight.h>
}

using namespace moonlight_xbox_dx;

Stats::Stats()
{
	ZeroMemory(&m_ActiveWndVideoStats, sizeof(VIDEO_STATS));
	ZeroMemory(&m_LastWndVideoStats, sizeof(VIDEO_STATS));
	ZeroMemory(&m_GlobalVideoStats, sizeof(VIDEO_STATS));
}

bool Stats::ShouldUpdateDisplay(DX::StepTimer const& timer, bool isVisible, char* output, size_t length)
{
	bool shouldUpdate = false;

	// Process stats once per second
	if (timer.GetTotalSeconds() - m_ActiveWndVideoStats.measurementStartTimestamp >= 1.0) {
		std::lock_guard<std::mutex> lock(m_mutex);

		if (isVisible) {
			// Display using data from the last 2 window periods
			VIDEO_STATS lastTwoWndStats = {};
			addVideoStats(timer, m_LastWndVideoStats, lastTwoWndStats);
			addVideoStats(timer, m_ActiveWndVideoStats, lastTwoWndStats);

			formatVideoStats(timer, lastTwoWndStats, output, length);
			shouldUpdate = true;
		}

		// Accumulate these values into the global stats
		addVideoStats(timer, m_ActiveWndVideoStats, m_GlobalVideoStats);

		// Move this window into the last window slot and clear it for next window
		memcpy(&m_LastWndVideoStats, &m_ActiveWndVideoStats, sizeof(VIDEO_STATS));
		ZeroMemory(&m_ActiveWndVideoStats, sizeof(VIDEO_STATS));
		m_ActiveWndVideoStats.measurementStartTimestamp = timer.GetTotalSeconds();
	}

	return shouldUpdate;
}

/// Hooks for stat producers, where possible these are combined into one call

// 1. The size in bytes of one video frame, we use this to also increment frame counters.
// 2. Time in milliseconds from first packet of a frame until fully reassembled frame is ready for decoding
//    Includes time spent in FEC reassembly
// 3. Host processing latency (encode time)
// 4. network packet loss (caller reports frame sequence number holes)
void Stats::SubmitVideoBytesAndReassemblyTime(uint32_t length, uint32_t reassemblyMs, uint16_t frameHPL, uint32_t droppedFrames) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_bwTracker.AddBytes(length);
	m_ActiveWndVideoStats.receivedFrames++;
	m_ActiveWndVideoStats.totalFrames++;
	m_ActiveWndVideoStats.totalReassemblyTime += reassemblyMs;

	// Host processing latency
	if (frameHPL != 0) {
		if (m_ActiveWndVideoStats.minHostProcessingLatency != 0) {
			m_ActiveWndVideoStats.minHostProcessingLatency = std::min(m_ActiveWndVideoStats.minHostProcessingLatency, frameHPL);
		}
		else {
			m_ActiveWndVideoStats.minHostProcessingLatency = frameHPL;
		}
		m_ActiveWndVideoStats.framesWithHostProcessingLatency += 1;
	}
	m_ActiveWndVideoStats.maxHostProcessingLatency = std::max(m_ActiveWndVideoStats.maxHostProcessingLatency, frameHPL);
	m_ActiveWndVideoStats.totalHostProcessingLatency += frameHPL;

	// Network packet loss
	if (droppedFrames > 0) {
		m_ActiveWndVideoStats.networkDroppedFrames += droppedFrames;
		m_ActiveWndVideoStats.totalFrames += droppedFrames;
	}
}

// Time in milliseconds we spent decoding one frame, it is added up to later be divided by decodedFrames
void Stats::SubmitDecodeMs(double decodeMs) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_ActiveWndVideoStats.totalDecodeTime += decodeMs;
	m_ActiveWndVideoStats.decodedFrames++;
}

// Time in milliseconds for all rendering, including Update(), Render(), and Present().
// If vsync is enabled, this will include time waiting for vsync in Present().
// Also increments the rendered frame count.
void Stats::SubmitRenderMs(double renderMs) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_ActiveWndVideoStats.totalRenderTime += renderMs;
	m_ActiveWndVideoStats.renderedFrames++;
}

void Stats::SubmitDXGIFrameStatistics(DXGI_FRAME_STATISTICS *frameStats) {
	std::lock_guard<std::mutex> lock(m_mutex);

	// frameStats->PresentCount;
	// frameStats->PresentRefreshCount;
	// frameStats->SyncRefreshCount;
	// frameStats->SyncQPCTime;

	// The values in PresentRefreshCount, SyncRefreshCount, and SyncQPCTime members of DXGI_FRAME_STATISTICS have the following characteristics:
	// PresentRefreshCount is equal to SyncRefreshCount when the app presents on every vsync.
	// SyncRefreshCount is obtained on the vsync interval when the present was submitted.
	// SyncQPCTime is approximately the time associated with the vsync interval.

	// Track glitches, from https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/TechniqueDemos/D3D12MemoryManagement/src/Framework.cpp
	if (frameStats->PresentCount > m_PreviousPresentCount) {
		if (m_PreviousRefreshCount > 0 &&
			(frameStats->PresentRefreshCount - m_PreviousRefreshCount) > (frameStats->PresentCount - m_PreviousPresentCount))
		{
			++m_GlitchCount;
			m_ActiveWndVideoStats.dxGlitchCount++;
		}
	}

	if (m_PreviousRefreshCount > 0) {
		m_ActiveWndVideoStats.dxRefreshCountDiff = frameStats->PresentRefreshCount - m_PreviousRefreshCount;
		m_ActiveWndVideoStats.dxPresentCountDiff = frameStats->PresentCount - m_PreviousPresentCount;
	}

	m_PreviousRefreshCount = frameStats->SyncRefreshCount;
	m_PreviousPresentCount = frameStats->PresentCount;

	// TODO: research this method of vsync frame pacing

	// Microsoft covers this topic at:
	// https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model#frame-synchronization-of-dxgi-flip-model-apps
	// https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model#avoiding-detecting-and-recovering-from-glitches

	// Some promising code around calculating estimated vsync timings and glitch recovery is in
	// https://github.com/Emill/n64-fast3d-engine/blob/master/gfx_dxgi.cpp
}

/// private methods

void Stats::addVideoStats(DX::StepTimer const& timer, VIDEO_STATS& src, VIDEO_STATS& dst) {
	dst.receivedFrames += src.receivedFrames;
	dst.decodedFrames += src.decodedFrames;
	dst.renderedFrames += src.renderedFrames;
	dst.totalFrames += src.totalFrames;
	dst.networkDroppedFrames += src.networkDroppedFrames;
	dst.totalReassemblyTime += src.totalReassemblyTime;
	dst.totalDecodeTime += src.totalDecodeTime;
	dst.totalRenderTime += src.totalRenderTime;

	if (dst.minHostProcessingLatency == 0) {
		dst.minHostProcessingLatency = src.minHostProcessingLatency;
	}
	else if (src.minHostProcessingLatency != 0) {
		dst.minHostProcessingLatency = std::min(dst.minHostProcessingLatency, src.minHostProcessingLatency);
	}
	dst.maxHostProcessingLatency = std::max(dst.maxHostProcessingLatency, src.maxHostProcessingLatency);
	dst.totalHostProcessingLatency += src.totalHostProcessingLatency;
	dst.framesWithHostProcessingLatency += src.framesWithHostProcessingLatency;

	if (!LiGetEstimatedRttInfo(&dst.lastRtt, &dst.lastRttVariance)) {
		dst.lastRtt = 0;
		dst.lastRttVariance = 0;
	}
	else {
		// Our logic to determine if RTT is valid depends on us never
		// getting an RTT of 0. ENet currently ensures RTTs are >= 1.
		assert(dst.lastRtt > 0);
	}

	dst.dxRefreshCountDiff = src.dxRefreshCountDiff; // these only store the most recent value
	dst.dxPresentCountDiff = src.dxPresentCountDiff;
	dst.dxGlitchCount += src.dxGlitchCount;

	double now = timer.GetTotalSeconds();

	// Initialize the measurement start point if this is the first video stat window
	if (!dst.measurementStartTimestamp) {
		dst.measurementStartTimestamp = src.measurementStartTimestamp;
	}

	// The following code assumes the global measure was already started first
	assert(dst.measurementStartTimestamp <= src.measurementStartTimestamp);

	dst.totalFps = (double)dst.totalFrames / (now - dst.measurementStartTimestamp);
	dst.receivedFps = (double)dst.receivedFrames / (now - dst.measurementStartTimestamp);
	dst.decodedFps = (double)dst.decodedFrames / (now - dst.measurementStartTimestamp);
	dst.renderedFps = (double)dst.renderedFrames / (now - dst.measurementStartTimestamp);
}

void Stats::formatVideoStats(DX::StepTimer const& timer, VIDEO_STATS& stats, char* output, size_t length) {
	FFMpegDecoder* ffmpeg = FFMpegDecoder::getInstance();
	int offset = 0;
	const char* codecString;
	int ret = -1;

	// Start with an empty string
	output[offset] = 0;

	switch (ffmpeg->videoFormat)
	{
	case VIDEO_FORMAT_H264:
		codecString = "H.264";
		break;

	case VIDEO_FORMAT_H264_HIGH8_444:
		codecString = "H.264 4:4:4";
		break;

	case VIDEO_FORMAT_H265:
		codecString = "HEVC";
		break;

	case VIDEO_FORMAT_H265_REXT8_444:
		codecString = "HEVC 4:4:4";
		break;

	case VIDEO_FORMAT_H265_MAIN10:
		if (LiGetCurrentHostDisplayHdrMode()) {
			codecString = "HEVC 10-bit HDR";
		}
		else {
			codecString = "HEVC 10-bit SDR";
		}
		break;

	case VIDEO_FORMAT_H265_REXT10_444:
		if (LiGetCurrentHostDisplayHdrMode()) {
			codecString = "HEVC 10-bit HDR 4:4:4";
		}
		else {
			codecString = "HEVC 10-bit SDR 4:4:4";
		}
		break;

	case VIDEO_FORMAT_AV1_MAIN8:
		codecString = "AV1";
		break;

	case VIDEO_FORMAT_AV1_HIGH8_444:
		codecString = "AV1 4:4:4";
		break;

	case VIDEO_FORMAT_AV1_MAIN10:
		if (LiGetCurrentHostDisplayHdrMode()) {
			codecString = "AV1 10-bit HDR";
		}
		else {
			codecString = "AV1 10-bit SDR";
		}
		break;

	case VIDEO_FORMAT_AV1_HIGH10_444:
		if (LiGetCurrentHostDisplayHdrMode()) {
			codecString = "AV1 10-bit HDR 4:4:4";
		}
		else {
			codecString = "AV1 10-bit SDR 4:4:4";
		}
		break;

	default:
		codecString = "UNKNOWN";
		break;
	}

	if (stats.receivedFps > 0) {
		if (ffmpeg != nullptr) {
			ret = snprintf(&output[offset],
						   length - offset,
						   "Video stream: %dx%d %.2f FPS (Codec: %s)\n",
						   ffmpeg->width,
						   ffmpeg->height,
						   stats.totalFps,
						   codecString);
			if (ret < 0 || ret >= length - offset) {
				Utils::Log("Error: stringifyVideoStats length overflow\n");
				return;
			}

			offset += ret;
		}

		double avgVideoMbps = m_bwTracker.GetAverageMbps();
		double peakVideoMbps = m_bwTracker.GetPeakMbps();

		ret = snprintf(&output[offset],
					   length - offset,
					   "Bitrate: %.1f Mbps, Peak (%us): %.1f\n"
					   "Incoming frame rate from network: %.2f FPS\n"
					   "Decoding frame rate: %.2f FPS\n"
					   "Rendering frame rate: %.2f FPS\n",
					   avgVideoMbps,
					   m_bwTracker.GetWindowSeconds(),
					   peakVideoMbps,
					   stats.receivedFps,
					   stats.decodedFps,
					   stats.renderedFps);
		if (ret < 0 || ret >= length - offset) {
			Utils::Log("Error: stringifyVideoStats length overflow\n");
			return;
		}

		offset += ret;
	}

	if (stats.framesWithHostProcessingLatency > 0) {
		ret = snprintf(&output[offset],
					   length - offset,
					   "Host processing latency min/max/average: %.1f/%.1f/%.1f ms\n",
					   (double)stats.minHostProcessingLatency / 10,
					   (double)stats.maxHostProcessingLatency / 10,
					   (double)stats.totalHostProcessingLatency / 10 / stats.framesWithHostProcessingLatency);
		if (ret < 0 || ret >= length - offset) {
			Utils::Log("Error: stringifyVideoStats length overflow\n");
			return;
		}

		offset += ret;
	}

	if (stats.renderedFrames != 0) {
		char rttString[32];

		if (stats.lastRtt != 0) {
			snprintf(rttString, sizeof(rttString), "%u ms (variance: %u ms)", stats.lastRtt, stats.lastRttVariance);
		}
		else {
			snprintf(rttString, sizeof(rttString), "N/A");
		}

		ret = snprintf(&output[offset],
					   length - offset,
					   "Frames dropped by your network connection: %.2f%%\n"
					   "Average network latency: %s\n"
					   "Average reassembly/decoding time: %.2f/%.2f ms\n"
					   "Average frametime: %.2f ms (vsync %s)\n",
					   (double)stats.networkDroppedFrames / stats.totalFrames * 100,
					   rttString,
					   (double)stats.totalReassemblyTime / stats.decodedFrames,
					   (double)stats.totalDecodeTime / stats.decodedFrames,
					   (double)stats.totalRenderTime / stats.renderedFrames,
					   (m_displayStatus == VSYNC_ON) ? "on" : "OFF");
		if (ret < 0 || ret >= length - offset) {
			Utils::Log("Error: stringifyVideoStats length overflow\n");
			return;
		}

		offset += ret;
	}

#if defined(_DEBUG)
	// Extra info only of interest to devs and not yet used for anything
	ret = snprintf(&output[offset],
					length - offset,
					"dxRefreshCountDiff %d dxPresentCountDiff %d dxGlitchCount %d\n",
					stats.dxRefreshCountDiff, stats.dxPresentCountDiff, stats.dxGlitchCount);
	if (ret < 0 || ret >= length - offset) {
		Utils::Log("Error: stringifyVideoStats length overflow\n");
		return;
	}

	offset += ret;
#endif
}
