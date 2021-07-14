#pragma once

#include "Common/DeviceResources.h"
#include<queue>

extern "C" {
#include <Limelight.h>
#include <libavcodec/avcodec.h>
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
		int SubmitDU(PDECODE_UNIT decodeUnit);
		AVFrame* GetFrame();
		AVFrame* GetLastFrame();
		static FFMpegDecoder* getInstance();
		static DECODER_RENDERER_CALLBACKS getDecoder();
		static FFMpegDecoder* createDecoderInstance(std::shared_ptr<DX::DeviceResources> resources);
		bool setup = false;
	private:
		int Decode(unsigned char* indata, int inlen);
		AVPacket pkt;
		AVCodec* decoder;
		AVCodecContext* decoder_ctx;
		unsigned char* ffmpeg_buffer;
		int dec_frames_cnt;
		AVFrame** dec_frames;
		AVFrame** ready_frames;
		int next_frame, current_frame;
		std::shared_ptr<DX::DeviceResources> resources;
		Microsoft::WRL::ComPtr<ID3D11Device1> ffmpegDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> ffmpegDeviceContext;
		ID3D11Texture2D* sharedTexture;
		ID3D11Texture2D* ffmpegTexture;
	};
}