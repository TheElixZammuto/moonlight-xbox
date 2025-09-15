#pragma once

#include <atomic>
#include <queue>
#include "../Common/StepTimer.h"
#include "Pacer.h"
#include "VideoRenderer.h"
#include "Utils.hpp"

extern "C" {
	#include <Limelight.h>
	#include <libavcodec/avcodec.h>
	#include <libswscale/swscale.h>
	#include <libavutil/hwcontext_d3d11va.h>
}

#define MAX_BUFFER 1024 * 1024

namespace moonlight_xbox_dx
{

	class FFMpegDecoder {
	public:
		// Singleton accessor
		static FFMpegDecoder &instance();

		void CompleteInitialization(const std::shared_ptr<DX::DeviceResources>& res, STREAM_CONFIGURATION *config);
		int Init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags);
		void Cleanup();
		int SubmitDecodeUnit(PDECODE_UNIT decodeUnit);
		int ModifyFrameDropTarget(bool increase);
		void WaitForFrame();
		bool RenderFrameOnMainThread(std::shared_ptr<VideoRenderer>& sceneRenderer);
		static FFMpegDecoder* getInstance();
		static DECODER_RENDERER_CALLBACKS getDecoder();
		std::recursive_mutex mutex;
		bool hackWait = false;
		int videoFormat, width, height;

	private:
		FFMpegDecoder();
		FFMpegDecoder(const FFMpegDecoder &) = delete;
		FFMpegDecoder &operator=(const FFMpegDecoder &) = delete;

		const AVCodec* decoder;
		AVCodecContext* decoder_ctx;
		AVHWDeviceContext* device_ctx;
		AVD3D11VADeviceContext* d3d11va_device_ctx;
		unsigned char* ffmpeg_buffer;
		int ffmpeg_buffer_size;
		std::shared_ptr<DX::DeviceResources> resources;
		std::atomic_int frameDropTarget;
		int m_LastFrameNumber;
		std::unique_ptr<Pacer> m_Pacer;
	};
}

inline void LOCK_D3D(const char *msg) {
#if !defined(NDEBUG)
	LARGE_INTEGER t0, t1;
	QueryPerformanceCounter(&t0);
#endif

	FFMpegDecoder::instance().mutex.lock();

#if !defined(NDEBUG)
	QueryPerformanceCounter(&t1);
	Utils::Logf("%s acquired lock_context in %.3f ms\n", msg, QpcToMs(t1.QuadPart - t0.QuadPart));
#endif
}

inline void UNLOCK_D3D() {
	FFMpegDecoder::instance().mutex.unlock();
}
