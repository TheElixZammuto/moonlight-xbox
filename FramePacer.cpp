#include "pch.h"
#include "FramePacer.h"
#include <Common\DirectXHelper.h>

void FramePacer::SubmitFrame(Frame frame) {
	if (textures.size() >= queueSize)textures.pop();
	textures.push(frame);
}

void FramePacer::PrepareFrameForRendering() {
	if (textures.size() > 0) {
		if (preparedFrame.texture != nullptr)preparedFrame.texture.ReleaseAndGetAddressOf();
		preparedFrame = textures.front();
		textures.pop();
		Microsoft::WRL::ComPtr<IDXGIResource1> dxgiResource;
		DX::ThrowIfFailed(preparedFrame.texture->QueryInterface(dxgiResource.GetAddressOf()));
		//DX::ThrowIfFailed(preparedFrame.texture->QueryInterface(mutex.GetAddressOf()));
		DX::ThrowIfFailed(dxgiResource->GetSharedHandle(&sharedHandle));
		if (sharedHandle == NULL)return;
		DX::ThrowIfFailed(renderingDevice->OpenSharedResource(sharedHandle, __uuidof(ID3D11Texture2D), (void**)(currentFrameTexture.ReleaseAndGetAddressOf())));
	}
	else {
		moonlight_xbox_dx::Utils::Log("Frame underrun - using previous frame\n");
	}
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> FramePacer::GetCurrentRenderingFrame() {
	
	return currentFrameTexture;
}

void FramePacer::ReleaseFrame() {

}