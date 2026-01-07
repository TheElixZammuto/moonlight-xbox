#include "pch.h"
#include "FFMpegDecoder.h"
#include "../Plot/ImGuiPlots.h"
#include "StatsRenderer.h"

#include <Common\DirectXHelper.h>
#include <d3d11_1.h>
#include "Utils.hpp"
#include "moonlight_xbox_dxMain.h"
#include <gamingdeviceinformation.h>

extern "C" {
#include "Limelight.h"
#include <third_party\h264bitstream\h264_stream.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/time.h>
}

using namespace moonlight_xbox_dx;

#define INITIAL_DECODER_BUFFER_SIZE (256 * 1024)

static bool ensure_buf_size(unsigned char **buf, int *buf_size, int required_size)
{
	if (*buf_size >= required_size)
		return true;

	FQLog("ensure_buf_size grew from %d -> %d\n", *buf_size, required_size);

	*buf_size = required_size;
	*buf = (unsigned char *)realloc(*buf, *buf_size);
	if (!*buf) {
		return false;
	}

	return true;
}

namespace moonlight_xbox_dx {
	FFMpegDecoder &FFMpegDecoder::instance() {
		static FFMpegDecoder inst;
		return inst;
	}

	FFMpegDecoder::FFMpegDecoder():
		width(0),
		height(0),
		videoFormat(0),
		decoder(nullptr),
		decoder_ctx(nullptr),
		device_ctx(nullptr),
		d3d11va_device_ctx(nullptr),
		ffmpeg_buffer(nullptr),
		ffmpeg_buffer_size(0),
		m_deviceResources(nullptr),
		m_LastFrameNumber(0) {
	}

	void lock_context(void *user) {
		auto me = (FFMpegDecoder*)user;
		me->m_mutex.lock();
	}

	void unlock_context(void *user) {
		auto me = (FFMpegDecoder*)user;
		me->m_mutex.unlock();
	}

	void ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
		char lineBuffer[1024];
		static int printPrefix = 1;

		if ((level & 0xFF) > av_log_get_level()) {
			return;
		}

		// We need to use the *previous* printPrefix value to determine whether to
		// print the prefix this time. av_log_format_line() will set the printPrefix
		// value to indicate whether the prefix should be printed *next time*.
		bool shouldPrefixThisMessage = printPrefix != 0;

		av_log_format_line(ptr, level, fmt, vl, lineBuffer, sizeof(lineBuffer), &printPrefix);
		Utils::Logf(shouldPrefixThisMessage ? "[ffmpeg] %s" : "%s", lineBuffer);
	}

    void FFMpegDecoder::CompleteInitialization(const std::shared_ptr<DX::DeviceResources>& res, STREAM_CONFIGURATION *config, bool framePacingImmediate) {
		m_deviceResources = res;
		Pacer::instance().init(res, config->fps, res->GetRefreshRate(), framePacingImmediate);
	}

	int FFMpegDecoder::Init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
		this->videoFormat = videoFormat;
		this->width = width;
		this->height = height;

		this->m_LastFrameNumber = 0;
		this->ffmpeg_buffer_size = 0;


#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58,10,100)
		avcodec_register_all();
#endif

		av_log_set_level(AV_LOG_ERROR);

		av_log_set_callback(&ffmpeg_log_callback);
#pragma warning(suppress : 4996)

		if (videoFormat & VIDEO_FORMAT_MASK_H264) {
			decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
			Utils::Log("Using H264\n");
		}
		else if (videoFormat & VIDEO_FORMAT_MASK_H265) {
			decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
			Utils::Log("Using HEVC\n");
		}

		if (decoder == NULL) {
			Utils::Log("Couldn't find decoder\n");
			return -1;
		}

		decoder_ctx = avcodec_alloc_context3(decoder);
		if (decoder_ctx == NULL) {
			Utils::Log("Couldn't allocate context\n");
			return -1;
		}
		decoder_ctx->opaque = this;

		AVBufferRef* hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
		device_ctx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
		d3d11va_device_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(device_ctx->hwctx);
		d3d11va_device_ctx->device = m_deviceResources->GetD3DDevice();
		d3d11va_device_ctx->device_context = m_deviceResources->GetD3DDeviceContext();
		d3d11va_device_ctx->lock = lock_context;
		d3d11va_device_ctx->unlock = unlock_context;
		d3d11va_device_ctx->lock_ctx = this;
		int err2;
		if ((err2 = av_hwdevice_ctx_init(hw_device_ctx)) < 0) {
			Utils::Logf("Failed to create specified DirectX Video device: %d\n", err2);
			Cleanup();
			return err2;
		}

		decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
		av_buffer_unref(&hw_device_ctx);
		decoder_ctx->pix_fmt = AV_PIX_FMT_D3D11;
		decoder_ctx->sw_pix_fmt = (videoFormat & VIDEO_FORMAT_MASK_10BIT) ? AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;
		decoder_ctx->pkt_timebase.num = 1;
		decoder_ctx->pkt_timebase.den = 90000;
		decoder_ctx->width = width;
		decoder_ctx->height = height;

		int err = avcodec_open2(decoder_ctx, decoder, NULL);
		if (err < 0) {
			char msg[2048];
			sprintf(msg, "Failed to create FFMpeg Codec: %d\n", err);
			Utils::Log(msg);
			return err;
		}

		if (decoder_ctx->pix_fmt != AV_PIX_FMT_D3D11) {
    		Utils::Log("Warning: decoder did not select AV_PIX_FMT_D3D11\n");
		}

		if (!ensure_buf_size(&ffmpeg_buffer, &ffmpeg_buffer_size, INITIAL_DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE)) {
			Utils::Log("Couldn't allocate initial ffmpeg_buffer\n");
			Cleanup();
			return -1;
		}

		return 0;
	}

	void FFMpegDecoder::Cleanup() {
		avcodec_free_context(&decoder_ctx);
		if (ffmpeg_buffer != NULL) {
			free(ffmpeg_buffer);
			ffmpeg_buffer = NULL;
			ffmpeg_buffer_size = 0;
		}
		m_LastFrameNumber = 0;

		Pacer::instance().deinit();

		Utils::Log("FFMpegDecoder::Cleanup\n");
	}

    static inline int frame_attach_userdata(AVFrame *frame, int64_t decodeEndQpc) {
	    if (!frame) return AVERROR(EINVAL);

	    if (frame->opaque_ref) {
		    av_buffer_unref(&frame->opaque_ref);
	    }

	    AVBufferRef *buf = av_buffer_allocz(sizeof(MLFrameData));
	    if (!buf) return AVERROR(ENOMEM);

	    MLFrameData *data = (MLFrameData *)buf->data;
	    data->decodeEndQpc = decodeEndQpc;
	    frame->opaque_ref = buf;

	    return 0;
    }

    // Called by the VideoDec thread
	int FFMpegDecoder::SubmitDecodeUnit(PDECODE_UNIT decodeUnit) {
		LARGE_INTEGER decodeStart, decodeEnd;
		PLENTRY entry = decodeUnit->bufferList;
		int length = 0;
		QueryPerformanceCounter(&decodeStart);

		if (!ensure_buf_size(&ffmpeg_buffer, &ffmpeg_buffer_size, decodeUnit->fullLength + AV_INPUT_BUFFER_PADDING_SIZE)) {
			Utils::Logf("Couldn't realloc ffmpeg_buffer\n");
			return DR_NEED_IDR;
		}

	    while (entry != NULL) {
		    memcpy(ffmpeg_buffer + length, entry->data, entry->length);
		    length += entry->length;
		    entry = entry->next;
	    }
		memset(ffmpeg_buffer + length, 0, AV_INPUT_BUFFER_PADDING_SIZE);

		// Detect breaks in the frame sequence indicating dropped packets
		uint32_t droppedFramesNetwork = 0;
		if (m_LastFrameNumber > 0 && decodeUnit->frameNumber > (m_LastFrameNumber + 1)) {
			// Any frame number greater than m_LastFrameNumber + 1 represents a dropped frame
			droppedFramesNetwork = decodeUnit->frameNumber - (m_LastFrameNumber + 1);
		}
		m_LastFrameNumber = decodeUnit->frameNumber;

		// track stats for a variety of things we can track at the same time
		m_deviceResources->GetStats()->SubmitVideoBytesAndReassemblyTime(length, decodeUnit, droppedFramesNetwork);

		// ffmpeg_decode
		AVPacket *pkt = av_packet_alloc();
		pkt->data = ffmpeg_buffer;
		pkt->size = length;
		pkt->pts = (int64_t)decodeUnit->rtpTimestamp;
		pkt->dts = pkt->pts;

		int err = avcodec_send_packet(decoder_ctx, pkt);
		av_packet_unref(pkt);
		av_packet_free(&pkt);
		if (err < 0) {
			char ffmpegError[1024];
			av_strerror(err, ffmpegError, 1024);
			Utils::Logf("avcodec_send_packet failed: %s\n", ffmpegError);
			return DR_NEED_IDR;
		}

		while (err >= 0) {
			AVFrame* frame = av_frame_alloc();
			err = avcodec_receive_frame(decoder_ctx, frame);
			if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
				av_frame_free(&frame);
				break;
			}
			else if (err < 0) {
				char ffmpegError[1024];
				av_strerror(err, ffmpegError, sizeof(ffmpegError));
				Utils::Logf("avcodec_receive_frame failed: %s\n", ffmpegError);
				av_frame_free(&frame);
				return DR_NEED_IDR;
			}
		    // Not the best way to handle this. BUT IT DOES FIX XBOX ONE TEARING!!!!
		    // Honestly this did take too much time of my life (and AndyG life too) to care to make a better version
		    // If you want to fix this, have fun! (And hopefully you have Microsoft blessing/tools/support for that)
		    if (LiGetPendingVideoFrames() < 2 && IsXboxOne()) moonlight_xbox_dx::usleep(12000);
			// Capture a frame timestamp to measuring pacing delay
			QueryPerformanceCounter(&decodeEnd);
			frame_attach_userdata(frame, decodeEnd.QuadPart);

			FQLog("✓ Frame decoded [pts: %.3fms] [in#: %d] [out#: %d] [lost: %d] decode time %.3fms\n",
				frame->pts / 90.0,
				decodeUnit->frameNumber, decoder_ctx->frame_num,
				decodeUnit->frameNumber - decoder_ctx->frame_num,
				QpcToMs(decodeEnd.QuadPart - decodeStart.QuadPart));

			// Queue the frame for rendering. frame is now owned by Pacer.
			Pacer::instance().submitFrame(frame);

			// Even though we have a valid frame, the ffmpeg API needs us to loop and call avcodec_receive_frame()
			// again where we expect to get AVERROR(EAGAIN) and break out.
		}

		if (decodeEnd.QuadPart > decodeStart.QuadPart) {
			m_deviceResources->GetStats()->SubmitDecodeMs(QpcToMs(decodeEnd.QuadPart - decodeStart.QuadPart));
		}

		return DR_OK;
	}

	//Helpers
	int initCallback(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) noexcept {
		return FFMpegDecoder::instance().Init(videoFormat, width, height, redrawRate, context, drFlags);
	}

	void cleanupCallback()noexcept {
		FFMpegDecoder::instance().Cleanup();
	}

	int submitDecodeUnit(PDECODE_UNIT decodeUnit) noexcept {
		return FFMpegDecoder::instance().SubmitDecodeUnit(decodeUnit);
	}

	DECODER_RENDERER_CALLBACKS FFMpegDecoder::getDecoder() {
		DECODER_RENDERER_CALLBACKS decoder_callbacks_sdl;
		LiInitializeVideoCallbacks(&decoder_callbacks_sdl);
		decoder_callbacks_sdl.setup = initCallback;
		decoder_callbacks_sdl.cleanup = cleanupCallback;
		decoder_callbacks_sdl.submitDecodeUnit = submitDecodeUnit;
		decoder_callbacks_sdl.capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_INTRA_REFRESH;
		//decoder_callbacks_sdl.capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC;
		return decoder_callbacks_sdl;
	}
}
