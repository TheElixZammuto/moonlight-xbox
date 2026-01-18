#pragma once

#include <atomic>
#include <mutex>
#include <queue>
#include "../Common/StepTimer.h"
#include "Pacer.h"
#include "Utils.hpp"
#include "VideoRenderer.h"

extern "C" {
#include <Limelight.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libswscale/swscale.h>
}

#define MAX_BUFFER 1024 * 1024

typedef struct MLFrameData {
	int64_t decodeEndQpc;     // when we finished decoding
	int64_t presentTargetQpc; // timestamp when frame should be presented (slightly earlier than vsync)
	int64_t presentVsyncQpc;  // hard vsync deadline
} MLFrameData;

namespace moonlight_xbox_dx {

class FFMpegDecoder {
  public:
	// Singleton accessor
	static FFMpegDecoder &instance();

	void CompleteInitialization(const std::shared_ptr<DX::DeviceResources> &res, STREAM_CONFIGURATION *config, bool framePacingImmediate);
	int Init(int videoFormat, int width, int height, int redrawRate, void *context, int drFlags);
	void Cleanup();
	int SubmitDecodeUnit(PDECODE_UNIT decodeUnit);
	static FFMpegDecoder *getInstance();
	static DECODER_RENDERER_CALLBACKS getDecoder();
	int videoFormat, width, height, fps;
	std::recursive_mutex m_mutex;

	// locking helper
	class LockGuard {
	  public:
		explicit LockGuard(FFMpegDecoder &ff)
		    : m_ff(ff) {
			m_ff.m_mutex.lock();
		}
		~LockGuard() {
			m_ff.m_mutex.unlock();
		}
		LockGuard(const LockGuard &) = delete;
		LockGuard &operator=(const LockGuard &) = delete;

	  private:
		FFMpegDecoder &m_ff;
	};

	[[nodiscard]] static LockGuard Lock() {
		return LockGuard(instance());
	}

  private:
	FFMpegDecoder();
	FFMpegDecoder(const FFMpegDecoder &) = delete;
	FFMpegDecoder &operator=(const FFMpegDecoder &) = delete;

	const AVCodec *decoder;
	AVCodecContext *decoder_ctx;
	AVHWDeviceContext *device_ctx;
	AVD3D11VADeviceContext *d3d11va_device_ctx;
	unsigned char *ffmpeg_buffer;
	int ffmpeg_buffer_size;
	std::shared_ptr<DX::DeviceResources> m_deviceResources;
	int m_LastFrameNumber;
	int64_t m_StreamEpochQpc;
};
} // namespace moonlight_xbox_dx
