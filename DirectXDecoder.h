#pragma once

#include "Common/DeviceResources.h"

extern "C" {
#include <Limelight.h>
}
#define MAX_BUFFER 1024 * 1024
namespace moonlight_xbox_dx
{
	DECODER_RENDERER_CALLBACKS getDecoder(DX::DeviceResources r);
	class DirectXDecoder {
	public: 
		DirectXDecoder(DX::DeviceResources resources) {
			mResources = resources;
		}
		DECODER_RENDERER_CALLBACKS GetCallbacks();
		int Init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags);
		void Start();
		void Stop();
		void Cleanup();
		int SubmitDU(PDECODE_UNIT decodeUnit);
	private:
		DX::DeviceResources mResources;
		unsigned char* ffmpeg_buffer;
		Microsoft::WRL::ComPtr<ID3D11VideoDevice1> videoDevice;
		Microsoft::WRL::ComPtr<ID3D11VideoContext> videoContext;
		Microsoft::WRL::ComPtr<ID3D11VideoDecoder> videoDecoder;
	};
}