#pragma once

#include "Common/DeviceResources.h"
#include<queue>

extern "C" {
#include <Limelight.h>
#include <libavcodec/avcodec.h>
#include<libswscale/swscale.h>
#include<libavutil/hwcontext_d3d11va.h>
}
#define MAX_BUFFER 1024 * 1024
namespace moonlight_xbox_dx
{
	
	class FFMpegDecoder {
	public: 
		FFMpegDecoder(std::shared_ptr<DX::DeviceResources> r) {
			resources = r;
		};
		int Init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags);
		void Start();
		void Stop();
		void Cleanup();
		void SubmitDU();
		AVFrame* GetFrame();
		static FFMpegDecoder* getInstance();
		static DECODER_RENDERER_CALLBACKS getDecoder();
		static FFMpegDecoder* createDecoderInstance(std::shared_ptr<DX::DeviceResources> resources);
		std::recursive_mutex mutex;
		bool shouldUnlock = false;
	private:
		int Decode(unsigned char* indata, int inlen);
		AVPacket pkt;
		const AVCodec* decoder;
		AVCodecContext* decoder_ctx;
		AVHWDeviceContext* device_ctx;
		AVD3D11VADeviceContext* d3d11va_device_ctx;
		unsigned char* ffmpeg_buffer;
		int dec_frames_cnt;
		AVFrame** dec_frames;
		AVFrame** ready_frames;
		int next_frame, current_frame;
		std::shared_ptr<DX::DeviceResources> resources;
	};
}