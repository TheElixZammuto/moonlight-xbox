#include "pch.h"
#include "FramePacer.h"
#include <Common\DirectXHelper.h>

void FramePacer::SubmitFrame(Frame frame) {
	if (textures.size() >= queueSize)textures.pop();
	textures.push(frame);
}

void FramePacer::PrepareFrameForRendering() {
	if (textures.size() > 0) {
		if (stagingTexture != nullptr)stagingTexture->Release();
		preparedFrame = textures.front();
		textures.pop();
		AVFrame* frame = preparedFrame.frame;
		D3D11_TEXTURE2D_DESC stagingDesc = { 0 };
		stagingDesc.Width = frame->width;
		stagingDesc.Height = frame->height;
		stagingDesc.ArraySize = 1;
		stagingDesc.Format = DXGI_FORMAT_NV12;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.MipLevels = 1;
		stagingDesc.SampleDesc.Quality = 0;
		stagingDesc.SampleDesc.Count = 1;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		stagingDesc.MiscFlags = 0;
		int luminanceLength = frame->height * frame->linesize[0];
		int chrominanceLength = frame->height * (frame->linesize[1] / 2);
		unsigned char* textureData = (unsigned char*) malloc((luminanceLength + chrominanceLength) * sizeof(unsigned char));
		memcpy(textureData, frame->data[0], luminanceLength);
		memcpy((textureData + luminanceLength + 1), frame->data[1], chrominanceLength);
		D3D11_SUBRESOURCE_DATA data;
		data.SysMemPitch = frame->linesize[0];
		data.pSysMem = textureData;
		DX::ThrowIfFailed(renderingDevice->CreateTexture2D(&stagingDesc, &data, stagingTexture.GetAddressOf()));
		free(textureData);
	}
	else {
		//moonlight_xbox_dx::Utils::Log("Frame underrun - using previous frame\n");
	}
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> FramePacer::GetCurrentRenderingFrame() {
	return stagingTexture;
}

void FramePacer::ReleaseFrame() {
	
}