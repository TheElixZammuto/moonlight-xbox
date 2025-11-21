#pragma once

#include <atomic>
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
	uint32_t presentationTimeUs; // host's pts
	int64_t decodeEndQpc;        // when we finished decoding
	int64_t presentTargetQpc;    // timestamp when frame should be presented (slightly earlier than vsync)
	int64_t presentVsyncQpc;     // hard vsync deadline
} MLFrameData;

namespace moonlight_xbox_dx {

class FFMpegDecoder {
  public:
	// Singleton accessor
	static FFMpegDecoder &instance();

	void CompleteInitialization(const std::shared_ptr<DX::DeviceResources> &res, STREAM_CONFIGURATION *config);
	int Init(int videoFormat, int width, int height, int redrawRate, void *context, int drFlags);
	void Cleanup();
	int SubmitDecodeUnit(PDECODE_UNIT decodeUnit);
	static FFMpegDecoder *getInstance();
	static DECODER_RENDERER_CALLBACKS getDecoder();
	int videoFormat, width, height;

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
	std::shared_ptr<DX::DeviceResources> resources;
	int m_LastFrameNumber;
};
} // namespace moonlight_xbox_dx
