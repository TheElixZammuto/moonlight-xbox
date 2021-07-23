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
	
	void ffmpeg_log_callback(void* avcl,int	level,const char* fmt,va_list vl) {
		//char message[2048];
		//sprintf_s(message, fmt, 2048, vl);
		//OutputDebugStringA(fmt);
	}
	
	int FFMpegDecoder::Init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
		av_log_set_callback(&ffmpeg_log_callback);
		av_log_set_level(AV_LOG_INFO);
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
		static AVBufferRef* hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
		AVHWDeviceContext* device_ctx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
		AVD3D11VADeviceContext* d3d11va_device_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(device_ctx->hwctx);
		D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2,
			D3D_FEATURE_LEVEL_9_1
		};
		D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, featureLevels, 6, D3D11_SDK_VERSION, &(d3d11va_device_ctx->device), NULL, &(d3d11va_device_ctx->device_context));
		//DX11-FFMpeg association
		int err2;
		if ((err2 = av_hwdevice_ctx_init(hw_device_ctx)) < 0) {
			fprintf(stderr, "Failed to create specified HW device.\n");
			return err2;

		}

		decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
		ffmpegDevice = (ID3D11Device1*)d3d11va_device_ctx->device;
		ffmpegDeviceContext = d3d11va_device_ctx->device_context;
		decoder_ctx->width = width;
        decoder_ctx->height = height;

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
		ffmpegDevice->OpenSharedResource1(resources->sharedHandle, __uuidof(ID3D11Texture2D), (void**)&sharedTexture);
		if (sharedTexture == NULL)return 1;
		DX::ThrowIfFailed(sharedTexture->QueryInterface(dxgiMutex.GetAddressOf()));
		//Create a Staging Texture
		D3D11_TEXTURE2D_DESC stagingDesc = { 0 };
		stagingDesc.Width = 1280;
		stagingDesc.Height = 720;
		stagingDesc.ArraySize = 1;
		stagingDesc.Format = DXGI_FORMAT_NV12;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.MipLevels = 1;
		stagingDesc.SampleDesc.Quality = 0;
		stagingDesc.SampleDesc.Count = 1;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		stagingDesc.MiscFlags = 0;
		DX::ThrowIfFailed(ffmpegDevice->CreateTexture2D(&stagingDesc, NULL, &stagingTexture));
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
		OutputDebugStringA("Got frame\n");
		//frameLock.lock();
		PLENTRY entry = decodeUnit->bufferList;
		uint32_t length = 0;
		while (entry != NULL) {
			/*if (entry->bufferType == BUFFER_TYPE_SPS)
				gs_sps_fix(entry, GS_SPS_BITSTREAM_FIXUP, ffmpeg_buffer, &length);
			else {*/
			/*if (entry->bufferType == BUFFER_TYPE_PICDATA)OutputDebugStringA("Got PICDATA\n");
			if (entry->bufferType == BUFFER_TYPE_PPS)OutputDebugStringA("Got PPS\n");
			if (entry->bufferType == BUFFER_TYPE_SPS)OutputDebugStringA("Got SPS\n");
			if (entry->bufferType == BUFFER_TYPE_VPS)OutputDebugStringA("Got VPS\n");*/
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
		setup = true;
		return DR_OK;
	}

	int FFMpegDecoder::Decode(unsigned char* indata, int inlen) {
		int err;

		pkt.data = indata;
		pkt.size = inlen;
		int ts = GetTickCount64();
		err = avcodec_send_packet(decoder_ctx, &pkt);
		if (err < 0) {
			char errorstring[512];
			av_strerror(err, errorstring, sizeof(errorstring));
			OutputDebugStringA(errorstring);
			return -1;
		}
		err = GetFrame();
		if (err != 0)return err;
		int te = GetTickCount64();
		/*char msg[4096];
		sprintf(msg, "Decoding took: %d ms\n", te - ts);
		OutputDebugStringA(msg);*/
		return err < 0 ? err : 0;
	}

	int FFMpegDecoder::GetFrame() {
		int err = avcodec_receive_frame(decoder_ctx, dec_frames[next_frame]);
		if (dec_frames[next_frame]->key_frame) {
			OutputDebugStringA("Got a KeyFrame\n");
		}
		decodedFrameNumber++;
		if (err == 0 && sharedTexture != NULL && (decodedFrameNumber - renderedFrameNumber) <= 1) {
			av_hwframe_transfer_data(ready_frames[next_frame], dec_frames[next_frame], 0);
			AVFrame *frame = ready_frames[next_frame];
			D3D11_MAPPED_SUBRESOURCE ms;
			DX::ThrowIfFailed(ffmpegDeviceContext->Map(stagingTexture, 0, D3D11_MAP_WRITE, 0, &ms));
			int luminanceLength = frame->height * frame->linesize[0];
			int chrominanceLength = frame->height * (frame->linesize[1] / 2);
			unsigned char* texturePointer = (unsigned char*)ms.pData;
			memcpy(texturePointer, frame->data[0], luminanceLength);
			memcpy((texturePointer + luminanceLength + 1), frame->data[1], chrominanceLength);
			//memcpy((texturePointer + luminanceLength + chrominanceLength), frame->data[2], chrominance2Length);
			ffmpegDeviceContext->Unmap(stagingTexture, 0);
			dxgiMutex->AcquireSync(0, 1000);
			ffmpegDeviceContext->CopyResource(sharedTexture, stagingTexture);
			dxgiMutex->ReleaseSync(1);
			OutputDebugStringA("Decoded frame\n");
			return 0;
		}
		return err;
	}

	AVFrame* FFMpegDecoder::GetLastFrame() {
		return NULL;
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