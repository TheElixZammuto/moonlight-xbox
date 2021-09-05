#include "pch.h"
#include "FramePacer.h"
#include <Common\DirectXHelper.h>
#include <Utils.hpp>
void FramePacer::SubmitFrame(Frame frame) {
	if (textures.size() >= queueSize) {
		Frame frame = textures.front();
		frame.mutex->Release();
		frame.texture->Release();
		textures.pop();
	}
	textures.push(frame);
}

void FramePacer::PrepareFrameForRendering() {
	if (textures.size() > 0) {
		if (preparedFrame.texture != nullptr) {
			if (mutex != NULL) {
				mutex->ReleaseSync(0);
				mutex->Release();
			}
			dxgiResource->Release();
			preparedFrame.mutex->Release();
			preparedFrame.texture->Release();
		}
		preparedFrame = textures.front();
		textures.pop();
		if (preparedFrame.texture == nullptr)return;
		DX::ThrowIfFailed(preparedFrame.texture->QueryInterface(dxgiResource.GetAddressOf()));
		DX::ThrowIfFailed(dxgiResource->CreateSharedHandle(NULL, DXGI_SHARED_RESOURCE_READ,NULL,&sharedHandle));
		if (sharedHandle == NULL)return;
		if (currentFrameTexture != nullptr) {
			currentFrameTexture->Release();
		}
		DX::ThrowIfFailed(renderingDevice->OpenSharedResource1(sharedHandle, __uuidof(ID3D11Texture2D), (void**)(currentFrameTexture.GetAddressOf())));
		DX::ThrowIfFailed(currentFrameTexture->QueryInterface(mutex.GetAddressOf()));
		if (mutex != NULL) mutex->AcquireSync(1, INFINITE);
	}
	else {
		//moonlight_xbox_dx::Utils::Log("Frame underrun - using previous frame\n");
	}
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> FramePacer::GetCurrentRenderingFrame() {
	return currentFrameTexture;
}