#include "pch.h"
#include "FFMpegDecoder.h"
#include "Common/DeviceResources.h"
#include <Common\DirectXHelper.h>
#include <d3d11_1.h>

extern "C" {
#include "Limelight.h"
#include <third_party\h264bitstream\h264_stream.h>
#include <libavcodec/avcodec.h>
#include<libswscale/swscale.h>
#include<libavutil/hwcontext_d3d11va.h>
}
#define DECODER_BUFFER_SIZE 1048576
namespace moonlight_xbox_dx {
	
	void ffmpeg_log_callback(void* avcl,int	level,const char* fmt,va_list 	vl) {
		const size_t cSize = strlen(fmt) + 1;
		wchar_t* wc = new wchar_t[cSize];
		mbstowcs(wc, fmt, cSize);
		wchar_t string[8192];
		swprintf(string, 8192, wc);
		OutputDebugString(string);
		OutputDebugString(L"\r\n");
	}
	
	int FFMpegDecoder::Init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
		//av_log_set_callback(&ffmpeg_log_callback);
		av_log_set_level(AV_LOG_VERBOSE);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58,10,100)
        avcodec_register_all();
#endif

#pragma warning(suppress : 4996)
        av_init_packet(&pkt);

        switch (videoFormat) {
        case VIDEO_FORMAT_H264:
            decoder = avcodec_find_decoder_by_name("h264");
            break;
        case VIDEO_FORMAT_H265:
            decoder = avcodec_find_decoder_by_name("hevc");
            break;
        }

        if (decoder == NULL) {
            printf("Couldn't find decoder\n");
            return -1;
        }




        decoder_ctx = avcodec_alloc_context3(decoder);
        if (decoder_ctx == NULL) {
            printf("Couldn't allocate context");
            return -1;
        }

		//DirectX Initializazion
		static AVBufferRef* hw_device_ctx = NULL;
		int err2;
		AVDictionary* dictionary = NULL;
		av_dict_set_int(&dictionary, "debug", 1, 0);
		if ((err2 = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_D3D11VA, NULL, dictionary, 0)) < 0) {
			fprintf(stderr, "Failed to create specified HW device.\n");
			return err2;

		}

		decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
		AVHWDeviceContext* device_ctx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
		AVD3D11VADeviceContext* d3d11va_device_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(device_ctx->hwctx);
		//d3d11va_device_ctx->device = resources->GetD3DDevice();

        //decoder_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;

        decoder_ctx->width = width;
        decoder_ctx->height = height;
		//decoder_ctx->pix_fmt = AV_PIX_FMT_D3D11;

        int err = avcodec_open2(decoder_ctx, decoder, NULL);
        if (err < 0) {
            printf("Couldn't open codec");
            return err;
        }
		ffmpeg_buffer = (unsigned char*)malloc(DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
		if (ffmpeg_buffer == NULL) {
			fprintf(stderr, "Not enough memory\n");
			Cleanup();
			return -1;
		}
		int buffer_count = 2;
		dec_frames_cnt = buffer_count;
		dec_frames = (AVFrame**)malloc(buffer_count * sizeof(AVFrame*));
		ready_frames = (AVFrame**)malloc(buffer_count * sizeof(AVFrame*));
		if (dec_frames == NULL) {
			fprintf(stderr, "Couldn't allocate frames");
			return -1;
		}

		for (int i = 0; i < buffer_count; i++) {
			dec_frames[i] = av_frame_alloc();
			if (dec_frames[i] == NULL) {
				fprintf(stderr, "Couldn't allocate frame");
				return -1;
			}
			ready_frames[i] = av_frame_alloc();
			if (ready_frames[i] == NULL) {
				fprintf(stderr, "Couldn't allocate frame");
				return -1;
			}
		}
		return 0;
	}

	void FFMpegDecoder::Start() {

	}

	void FFMpegDecoder::Stop() {

	}

	void FFMpegDecoder::Cleanup() {

	}

	int FFMpegDecoder::SubmitDU(PDECODE_UNIT decodeUnit) {
		if (decodeUnit->fullLength > DECODER_BUFFER_SIZE) {
			printf("C");
			return -1;
		}
		//frameLock.lock();
		PLENTRY entry = decodeUnit->bufferList;
		uint32_t length = 0;
		while (entry != NULL) {
			/*if (entry->bufferType == BUFFER_TYPE_SPS)
				gs_sps_fix(entry, GS_SPS_BITSTREAM_FIXUP, ffmpeg_buffer, &length);
			else {*/
			memcpy(ffmpeg_buffer + length, entry->data, entry->length);
			length += entry->length;
			entry = entry->next;
			/*}*/
		}
		int err;
		err = Decode(ffmpeg_buffer, length);
		if (err < 0) {
			char errorstringnew[1024];
			sprintf(errorstringnew, "Error from FFMPEG: %d", AVERROR(err));
			return DR_NEED_IDR;
		}
		AVFrame *frame = GetFrame();
		if (frame == NULL)return DR_NEED_IDR;
		return DR_OK;
	}

	int FFMpegDecoder::Decode(unsigned char* indata, int inlen) {
		int err;

		pkt.data = indata;
		pkt.size = inlen;

		err = avcodec_send_packet(decoder_ctx, &pkt);
		if (err < 0) {
			char errorstring[512];
			av_strerror(err, errorstring, sizeof(errorstring));
			fprintf(stderr, "Decode failed - %s\n", errorstring);
		}

		return err < 0 ? err : 0;
	}

	AVFrame* FFMpegDecoder::GetFrame() {
		int err = avcodec_receive_frame(decoder_ctx, dec_frames[next_frame]);
		if (err == 0) {
			int a = av_hwframe_transfer_data(ready_frames[next_frame], dec_frames[next_frame], 0);
			if (a == 0) {
				current_frame = next_frame;
				next_frame = (current_frame + 1) % dec_frames_cnt;
				setup = true;
				return ready_frames[current_frame];
			}
		}
		else if (err != AVERROR(EAGAIN)) {
			char errorstring[512];
			av_strerror(err, errorstring, sizeof(errorstring));
			fprintf(stderr, "Receive failed - %d/%s\n", err, errorstring);
		}
		return NULL;
	}

	AVFrame* FFMpegDecoder::GetLastFrame() {
		return ready_frames[current_frame];
	}

	
	//Helpers
	FFMpegDecoder *instance;

	FFMpegDecoder* FFMpegDecoder::createDecoderInstance(std::shared_ptr<DX::DeviceResources> resources) {
		if (instance == NULL)instance = new FFMpegDecoder(resources);
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