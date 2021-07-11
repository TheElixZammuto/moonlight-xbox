#include "pch.h"
#include "DirectXDecoder.h"
#include "Common/DeviceResources.h"
#include <Common\DirectXHelper.h>
#include <d3d11_1.h>

extern "C" {
#include "Limelight.h"
#include <third_party\h264bitstream\h264_stream.h>
}

namespace moonlight_xbox_dx {
	int DirectXDecoder::Init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
		DX::ThrowIfFailed(mResources.GetD3DDevice()->QueryInterface<ID3D11VideoDevice1>(videoDevice.GetAddressOf()));
		DX::ThrowIfFailed(mResources.GetD3DDeviceContext()->QueryInterface<ID3D11VideoContext>(videoContext.GetAddressOf()));
		D3D11_VIDEO_DECODER_DESC videoDecoderDesc = { 0 };
		videoDecoderDesc.SampleHeight = height;
		videoDecoderDesc.SampleWidth = width;
		//Guid to H264_VB_NOFGT
		videoDecoderDesc.Guid = GUID{0x1b81be68, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5};
		GUID guid;
		int decoderCount = videoDevice->GetVideoDecoderProfileCount();
		for (int i = 0; i < decoderCount;  i++) {
			videoDevice->GetVideoDecoderProfile(i, &guid);
		}
		videoDecoderDesc.OutputFormat = DXGI_FORMAT_NV12;
		D3D11_VIDEO_DECODER_CONFIG videoDecoderConfig = { 0 };
		videoDevice->GetVideoDecoderConfig(&videoDecoderDesc, 0, &videoDecoderConfig);
		DX::ThrowIfFailed(videoDevice->CreateVideoDecoder(&videoDecoderDesc, &videoDecoderConfig, videoDecoder.GetAddressOf()));
		ffmpeg_buffer = (unsigned char*) malloc(MAX_BUFFER);
		return 0;
	}

	void DirectXDecoder::Start() {

	}

	void DirectXDecoder::Stop() {

	}

	void DirectXDecoder::Cleanup() {

	}

	int DirectXDecoder::SubmitDU(PDECODE_UNIT decodeUnit) {
		if (decodeUnit->fullLength > MAX_BUFFER) {
			printf("C");
			return -1;
		}
		PLENTRY entry = decodeUnit->bufferList;
		uint32_t length = 0;
		Microsoft::WRL::ComPtr<ID3D11VideoDecoderOutputView> decoderView;
		videoContext->DecoderBeginFrame(videoDecoder.Get(), decoderView.Get(), 0, NULL);
		while (entry != NULL) {
			h264_stream_t* h = h264_new();
			int nal_start, nal_end;
			find_nal_unit((uint8_t*)entry->data,entry->length,&nal_start,&nal_end);
			read_nal_unit(h, (uint8_t*)(&entry->data[nal_start]), nal_end - nal_start);
			/*D3D11_VIDEO_DECODER_BUFFER_DESC desc;
			desc.DataOffset = 0;
			desc.DataSize = entry->length;
			switch (entry->bufferType) {
			case BUFFER_TYPE_PICDATA:
			case BUFFER_TYPE_SPS:
				desc.BufferType = D3D11_VIDEO_DECODER_BUFFER_MACROBLOCK_CONTROL;
				break;
			case BUFFER_TYPE_PPS:
				desc.BufferType = D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS;
				break;
			case BUFFER_TYPE_VPS:
			}
			videoContext->SubmitDecoderBuffers(videoDecoder.Get(), 1, &desc);*/
			printf("%d",h->nal->nal_unit_type);
		}
		int err;
		D3D11_VIDEO_DECODER_BUFFER_DESC desc;
		return DR_OK;
	}

	
	//Helpers
	DirectXDecoder *instance;
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

	DECODER_RENDERER_CALLBACKS getDecoder(DX::DeviceResources r) {
		instance = new DirectXDecoder(r);
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