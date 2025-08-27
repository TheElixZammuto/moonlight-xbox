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
#define DECODER_BUFFER_SIZE 1048576

namespace moonlight_xbox_dx {

	void lock_context(void* dec) {
		auto ff = (FFMpegDecoder*)dec;
		ff->mutex.lock();
	}

	void unlock_context(void* dec) {
		auto ff = (FFMpegDecoder*)dec;
		ff->mutex.unlock();
	}

	enum AVPixelFormat ffGetFormat(AVCodecContext* context, const enum AVPixelFormat* pixFmts)
	{
		FFMpegDecoder* d = ((FFMpegDecoder*)context->opaque);
		const enum AVPixelFormat* p;

		for (p = pixFmts; *p != -1; p++) {
			// Only match our hardware decoding codec or preferred SW pixel
			// format (if not using hardware decoding). It's crucial
			// to override the default get_format() which will try
			// to gracefully fall back to software decode and break us.
			if (*p == AV_PIX_FMT_D3D11) {
				return *p;
			}
		}



		return AV_PIX_FMT_NONE;
	}

	void ffmpeg_log_callback(void* avcl, int	level, const char* fmt, va_list vl) {
		if (level > AV_LOG_INFO) return;

		char message[2048];
		vsprintf_s(message, fmt, vl);
		OutputDebugStringA("[FFMPEG]");
		OutputDebugStringA(message);
		if (level <= AV_LOG_INFO) {
			Utils::Log("[FFMPEG]");
			Utils::Log(message);
			Utils::Log("\n");
		}
	}

	int FFMpegDecoder::Init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
		this->videoFormat = videoFormat;
		this->width = width;
		this->height = height;

		this->frameDropTarget = 2; // user can modify this value
		this->m_LastFrameNumber = 0;


#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58,10,100)
		avcodec_register_all();
#endif
		av_log_set_level(AV_LOG_INFO);
		av_log_set_callback(&ffmpeg_log_callback);
#pragma warning(suppress : 4996)
		av_init_packet(&pkt);

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
			Utils::Log("Couldn't allocate context");
			return -1;
		}
		decoder_ctx->opaque = this;

		AVBufferRef* hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
		device_ctx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
		d3d11va_device_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(device_ctx->hwctx);
		d3d11va_device_ctx->device = this->resources->GetD3DDevice();
		d3d11va_device_ctx->device_context = this->resources->GetD3DDeviceContext();
		d3d11va_device_ctx->lock = lock_context;
		d3d11va_device_ctx->unlock = unlock_context;
		d3d11va_device_ctx->lock_ctx = this;
		int err2;
		if ((err2 = av_hwdevice_ctx_init(hw_device_ctx)) < 0) {
			char msg[2048];
			sprintf(msg, "Failed to create specified DirectX Video device: %d\n", err2);
			Utils::Log(msg);
			return err2;

		}

		decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
		decoder_ctx->pix_fmt = AV_PIX_FMT_D3D11;
		decoder_ctx->sw_pix_fmt = (videoFormat & VIDEO_FORMAT_MASK_10BIT) ? AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;
		decoder_ctx->width = width;
		decoder_ctx->height = height;
		decoder_ctx->get_format = ffGetFormat;

		int err = avcodec_open2(decoder_ctx, decoder, NULL);
		if (err < 0) {
			char msg[2048];
			sprintf(msg, "Failed to create FFMpeg Codec: %d\n", err);
			Utils::Log(msg);
			return err;
		}
		ffmpeg_buffer = (unsigned char*)malloc(DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
		if (ffmpeg_buffer == NULL) {
			Utils::Log("OOM\n");
			Cleanup();
			return -1;
		}
		int buffer_count = 2;
		dec_frames_cnt = buffer_count;
		dec_frames = (AVFrame**)malloc(buffer_count * sizeof(AVFrame*));
		ready_frames = (AVFrame**)malloc(buffer_count * sizeof(AVFrame*));
		if (dec_frames == NULL) {
			Utils::Log("Cannot allocate Frames\n");
			return -1;
		}

		for (int i = 0; i < buffer_count; i++) {
			dec_frames[i] = av_frame_alloc();
			if (dec_frames[i] == NULL) {
				Utils::Log("Cannot allocate Frames\n");
				return -1;
			}
			ready_frames[i] = av_frame_alloc();
			if (ready_frames[i] == NULL) {
				Utils::Log("Cannot allocate Frames\n");
				return -1;
			}
		}
		GAMING_DEVICE_MODEL_INFORMATION info = {};
		GetGamingDeviceModelInformation(&info);
		if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT &&
			(info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE ||
				info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE_S ||
				info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X ||
				info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X_DEVKIT)) {
			Utils::Log("Using hack for Xbox One Consoles");
			hackWait = true;
		}
		return 0;
	}

	void FFMpegDecoder::Start() {
		Utils::Log("Decoding Started\n");
	}

	void FFMpegDecoder::Stop() {
		Utils::Log("Decoding Stopped\n");
	}

	void FFMpegDecoder::Cleanup() {
		if (dec_frames != NULL) {
			for (int i = 0; i < dec_frames_cnt; i++) {
				av_frame_free(&dec_frames[i]);
			}
			free(dec_frames);
		}
		avcodec_close(decoder_ctx);
		avcodec_free_context(&decoder_ctx);
		if (ffmpeg_buffer != NULL) {
			free(ffmpeg_buffer);
			ffmpeg_buffer = NULL;
		}
		if (ready_frames != NULL) {
			free(ready_frames);
			ready_frames = NULL;
		}
		m_LastFrameNumber = 0;
		shouldUnlock = false;
		Utils::Log("Decoding Clean\n");
	}

	bool FFMpegDecoder::SubmitDU() {
		PDECODE_UNIT decodeUnit = nullptr;
		VIDEO_FRAME_HANDLE frameHandle = nullptr;
		bool status = LiWaitForNextVideoFrame(&frameHandle, &decodeUnit);
		if (status == false)return false;
		if (decodeUnit->fullLength > DECODER_BUFFER_SIZE) {
			Utils::Log("(0) Decoder Buffer Size reached\n");
			LiCompleteVideoFrame(frameHandle, DR_NEED_IDR);
			return false;
		}
		PLENTRY entry = decodeUnit->bufferList;
		uint32_t length = 0;
		while (entry != NULL) {
			memcpy(ffmpeg_buffer + length, entry->data, entry->length);
			length += entry->length;
			entry = entry->next;
		}

		// Detect breaks in the frame sequence indicating dropped packets
		uint32_t droppedFramesNetwork = 0;
		if (m_LastFrameNumber > 0 && decodeUnit->frameNumber > (m_LastFrameNumber + 1)) {
			// Any frame number greater than m_LastFrameNumber + 1 represents a dropped frame
			droppedFramesNetwork = decodeUnit->frameNumber - (m_LastFrameNumber + 1);
		}
		m_LastFrameNumber = decodeUnit->frameNumber;

		// track stats for a variety of things we can track at the same time
		this->resources->GetStats()->SubmitVideoBytesAndReassemblyTime(length, decodeUnit, droppedFramesNetwork);

		int err;
		err = Decode(ffmpeg_buffer, length);
		if (err < 0) {
			LiCompleteVideoFrame(frameHandle, DR_NEED_IDR);
			return false;
		}
		LiCompleteVideoFrame(frameHandle, DR_OK);
		return true;
	}

	int FFMpegDecoder::Decode(unsigned char* indata, int inlen) {
		int err;

		pkt.data = indata;
		pkt.size = inlen;
		decodeStartTime = av_gettime_relative();
		err = avcodec_send_packet(decoder_ctx, &pkt);
		if (err < 0) {

			char errorstringnew[2048], ffmpegError[1024];
			av_strerror(err, ffmpegError, 1024);
			sprintf(errorstringnew, "Error avcodec_send_packet: %s\n", ffmpegError);
			Utils::Log(errorstringnew);
			return err == 1 ? 0 : err;
		}
		return err < 0 ? err : 0;
	}

	AVFrame* FFMpegDecoder::GetFrame() {
		int err = avcodec_receive_frame(decoder_ctx, dec_frames[next_frame]);
		if (err != 0 && err != AVERROR(EAGAIN)) {
			char errorstringnew[1024];
			sprintf(errorstringnew, "Error avcodec_receive_frame: %d\n", AVERROR(err));
			Utils::Log(errorstringnew);
			return nullptr;
		}
		if (err == 0) {
			AVFrame *frame = dec_frames[next_frame];

			this->resources->GetStats()->SubmitDecodeMs((double)(av_gettime_relative() - decodeStartTime) / 1000.0);

			// Frame pacing (to be improved)
			int dropTarget = this->frameDropTarget;
			bool is_idr = (frame->pict_type == AV_PICTURE_TYPE_I) && (frame->flags & AV_FRAME_FLAG_KEY);
			int pending = LiGetPendingVideoFrames();
			bool shouldDrop = pending > dropTarget && !is_idr;
			ImGuiPlots::instance().observeFloat(PLOT_DROPPED_PACER, shouldDrop ? 1 : 0);
			if (shouldDrop) {
				Utils::Logf("dropping frame because pending frame count %d > frame queue size %d\n", pending, dropTarget);
				// Drop frame to reduce latency (unless it's an IDR frame)
				this->resources->GetStats()->SubmitDroppedFrame(1);
				av_frame_unref(frame);
				return nullptr;
			}

			//Not the best way to handle this. BUT IT DOES FIX XBOX ONES!!!!
			//Honestly this did take too much time of my life to care to make a better version
			//If you want to fix this, have fun! (And hopefully you have Microsoft blessing/tools/support for that)
			if (hackWait && LiGetPendingVideoFrames() < 2)moonlight_xbox_dx::usleep(12000);

			return frame;
		}
		return nullptr;
	}

	int FFMpegDecoder::ModifyFrameDropTarget(bool increase) {
		int current = frameDropTarget;
		if (increase) {
			if (current >= 14) {
				// decodeUnitQueue is 15, so our max here is 14
				return current;
			}
			return ++frameDropTarget;
		}
		else {
			if (current == 0) {
				return current;
			}
			return --frameDropTarget;
		}
	}

	//Helpers
	FFMpegDecoder* instance;

	FFMpegDecoder* FFMpegDecoder::createDecoderInstance(std::shared_ptr<DX::DeviceResources> resources) {
		if (instance == NULL) {
			instance = new FFMpegDecoder(resources);
		}
		return instance;
	}

	FFMpegDecoder* getDecoderInstance() {
		return instance;
	}

	int initCallback(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
		return instance->Init(videoFormat, width, height, redrawRate, context, drFlags);
	}
	void startCallback() {
		instance->Start();
	}
	void stopCallback() {
		instance->Stop();
	}
	void cleanupCallback() {
		instance->Cleanup();
		delete instance;
		instance = NULL;
	}

	FFMpegDecoder* FFMpegDecoder::getInstance() {
		return instance;
	}

	DECODER_RENDERER_CALLBACKS FFMpegDecoder::getDecoder() {
		instance = FFMpegDecoder::getInstance();
		DECODER_RENDERER_CALLBACKS decoder_callbacks_sdl;
		LiInitializeVideoCallbacks(&decoder_callbacks_sdl);
		decoder_callbacks_sdl.setup = initCallback;
		decoder_callbacks_sdl.start = startCallback;
		decoder_callbacks_sdl.stop = stopCallback;
		decoder_callbacks_sdl.cleanup = cleanupCallback;
		decoder_callbacks_sdl.submitDecodeUnit = NULL;
		decoder_callbacks_sdl.capabilities = CAPABILITY_PULL_RENDERER | CAPABILITY_INTRA_REFRESH;
		return decoder_callbacks_sdl;
	}


}

