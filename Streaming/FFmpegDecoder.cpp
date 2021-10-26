#include "pch.h"
#include "FFMpegDecoder.h"
#include "Common/DeviceResources.h"
#include <Common\DirectXHelper.h>
#include <d3d11_1.h>
#include "Utils.hpp"

extern "C" {
#include "Limelight.h"
#include <third_party\h264bitstream\h264_stream.h>
#include <libavcodec/avcodec.h>
#include<libswscale/swscale.h>
#include<libavutil/hwcontext_d3d11va.h>
}
#define DECODER_BUFFER_SIZE 1048576
namespace moonlight_xbox_dx {
	
	void ffmpeg_log_callback(void* avcl,int	level,const char* fmt,va_list vl) {
		/*if (level > AV_LOG_INFO)return;
		char message[2048];
		vsprintf_s(message, fmt, vl);
		Utils::Log(message);*/
	}
	
	int FFMpegDecoder::Init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58,10,100)
        avcodec_register_all();
#endif

		av_log_set_callback(&ffmpeg_log_callback);
#pragma warning(suppress : 4996)
        av_init_packet(&pkt);

        switch (videoFormat) {
        case VIDEO_FORMAT_H264:
            decoder = avcodec_find_decoder_by_name("h264");
			Utils::Log("Using H264\n");
            break;
        case VIDEO_FORMAT_H265:
            decoder = avcodec_find_decoder_by_name("hevc");
			Utils::Log("Using HEVC\n");
            break;
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

		//DirectX Initializazion
		D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2,
			D3D_FEATURE_LEVEL_9_1
		};
		ID3D11Device *dev;
		UINT creationFlags = 0;
		#if defined(_DEBUG)
				if (DX::SdkLayersAvailable())
				{
					// If the project is in a debug build, enable debugging via SDK Layers with this flag.
					creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
				}
		#endif
		D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, featureLevels, 6, D3D11_SDK_VERSION, &dev, NULL, ffmpegDeviceContext.GetAddressOf());
		//DX11-FFMpeg association
		ffmpegDevice = (ID3D11Device1*)dev;
	    AVBufferRef* hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
		device_ctx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
		d3d11va_device_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(device_ctx->hwctx);
		d3d11va_device_ctx->device = dev;
		d3d11va_device_ctx->device_context = ffmpegDeviceContext.Get();
		int err2;
		if ((err2 = av_hwdevice_ctx_init(hw_device_ctx)) < 0) {
			char msg[2048];
			sprintf(msg, "Failed to create specified DirectX Video device: %d\n", err2);
			Utils::Log(msg);
			return err2;

		}
		decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
		
		decoder_ctx->width = width;
        decoder_ctx->height = height;

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
		pacer->decodingDevice = ffmpegDevice.Get();
		pacer->Setup(width, height);
		return 0;
	}

	void FFMpegDecoder::Start() {
		Utils::Log("Decoding Started\n");
	}

	void FFMpegDecoder::Stop() {
		Utils::Log("Decoding Stopped\n");
	}

	void FFMpegDecoder::Cleanup() {
		//av_free_packet(&pkt);
		avcodec_close(decoder_ctx);
		avcodec_free_context(&decoder_ctx);
		ffmpegDevice->Release();
		ffmpegDeviceContext->Release();
		free(ready_frames);
		free(dec_frames);
		free(ffmpeg_buffer);
		pacer = NULL;
		Utils::Log("Decoding Clean\n");
	}

	int FFMpegDecoder::SubmitDU(PDECODE_UNIT decodeUnit) {
		if (decodeUnit->fullLength > DECODER_BUFFER_SIZE) {
			Utils::Log("(0) Decoder Buffer Size reached\n");
			return -1;
		}
		PLENTRY entry = decodeUnit->bufferList;
		uint32_t length = 0;
		while (entry != NULL) {
			memcpy(ffmpeg_buffer + length, entry->data, entry->length);
			length += entry->length;
			entry = entry->next;
		}
		int err;
		err = Decode(ffmpeg_buffer, length);
		if (err < 0) {
			char errorstringnew[1024];
			sprintf(errorstringnew, "!!!!!!!Error from FFMPEG while decoding: %s\n", errorstringnew);
			Utils::Log(errorstringnew);
			return DR_NEED_IDR;
		}
		return DR_OK;
	}

	int FFMpegDecoder::Decode(unsigned char* indata, int inlen) {
		int err;

		pkt.data = indata;
		pkt.size = inlen;
		int ts = GetTickCount64();
		err = avcodec_send_packet(decoder_ctx, &pkt);
		if (err < 0) {

			char errorstringnew[2048],ffmpegError[1024];
			av_strerror(err, ffmpegError, 1024);
			sprintf(errorstringnew, "Error avcodec_send_packet: %s\n", ffmpegError);
			Utils::Log(errorstringnew);
			return err == 1 ? 0 : err;
		}
		err = GetFrame();
		if (err != 0) {
			char errorstringnew[1024];
			sprintf(errorstringnew, "Error GetFrame: %d\n", AVERROR(err));
			Utils::Log(errorstringnew);
			return err;
		}
		int te = GetTickCount64();
		return err < 0 ? err : 0;
	}

	int FFMpegDecoder::GetFrame() {
		int err = avcodec_receive_frame(decoder_ctx, dec_frames[next_frame]);
		if (err != 0 && err != AVERROR(EAGAIN)) {
			char errorstringnew[1024];
			sprintf(errorstringnew, "Error avcodec_receive_frame: %d\n", AVERROR(err));
			Utils::Log(errorstringnew);
			return err;
		}
		if (err == 0) {
			AVFrame* frame = dec_frames[next_frame];
			ID3D11Texture2D* ffmpegTexture = (ID3D11Texture2D*)(frame->data[0]);
			int index = (int)(frame->data[1]);
			pacer->SubmitFrame(ffmpegTexture, index, ffmpegDeviceContext);
		}
		return 0;
	}
	
	//Helpers
	FFMpegDecoder *instance;

	FFMpegDecoder* FFMpegDecoder::createDecoderInstance(std::shared_ptr<DX::DeviceResources> resources, FramePacer *pacer) {
		if (instance == NULL) {
			instance = new FFMpegDecoder(resources, pacer);
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
	int submitCallback(PDECODE_UNIT decodeUnit) {
		return instance->SubmitDU(decodeUnit);
	}

	FFMpegDecoder* FFMpegDecoder::getInstance() {
		return instance;
	}

	DECODER_RENDERER_CALLBACKS FFMpegDecoder::getDecoder() {
		instance = FFMpegDecoder::getInstance();
		DECODER_RENDERER_CALLBACKS decoder_callbacks_sdl;
		decoder_callbacks_sdl.setup = initCallback;
		decoder_callbacks_sdl.start = startCallback;
		decoder_callbacks_sdl.stop = stopCallback;
		decoder_callbacks_sdl.cleanup = cleanupCallback;
		decoder_callbacks_sdl.submitDecodeUnit = submitCallback;
		decoder_callbacks_sdl.capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC | CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC;
		return decoder_callbacks_sdl;
	}
}