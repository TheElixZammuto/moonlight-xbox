#include "pch.h"
#include "FramePacer.h"
#include <Common\DirectXHelper.h>
#include <Utils.hpp>

void FramePacer::Setup(int width, int height) {
	D3D11_TEXTURE2D_DESC desc;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_NV12;
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.SampleDesc.Quality = 0;
	desc.SampleDesc.Count = 1;
	desc.CPUAccessFlags = 0;
	desc.BindFlags = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
	for (int i = 0; i < queueSize; i++) {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> decodingTexture,renderingTexture;
		Microsoft::WRL::ComPtr<IDXGIKeyedMutex> decodingMutex,renderingMutex;
		Microsoft::WRL::ComPtr<IDXGIResource1> resource;
		decodingDevice->CreateTexture2D(&desc, NULL, decodingTexture.GetAddressOf());
		decodingTexture.As(&decodingMutex);
		decodingTexture.As(&resource);
		HANDLE handle;
		resource->CreateSharedHandle(NULL,DXGI_SHARED_RESOURCE_READ, NULL, &handle);
		renderingDevice->OpenSharedResource1(handle, __uuidof(ID3D11Texture2D), (void**)(renderingTexture.GetAddressOf()));
		renderingTexture.As(&renderingMutex);
		frames.push_back({
			decodingTexture,
			decodingMutex,
			handle,
			renderingTexture,
			renderingMutex,
			new std::mutex(),
			0
		});
	}
}
void FramePacer::SubmitFrame(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,int index,Microsoft::WRL::ComPtr<ID3D11DeviceContext> decodeContext) {
	int i = 1;
	Frame currentFrame = frames[(decodeIndex + i) % queueSize];
	currentFrame.frameNumber = (decodeIndex+i);
	currentFrame.decodeMutex->AcquireSync(0, 16);
	decodeContext->CopySubresourceRegion(currentFrame.decodeTexture.Get(), 0, 0, 0, 0, texture.Get(), index, NULL);
	currentFrame.decodeMutex->ReleaseSync(1);
	//currentFrame.mutex->unlock();
	decodeIndex = decodeIndex + i;
}

void FramePacer::PrepareFrameForRendering() {
	if (decodeIndex < 0)return;
	int nextIndex = renderIndex + 1;
	if (decodeIndex - nextIndex >= 4) {
		nextIndex++;
		moonlight_xbox_dx::Utils::Log("Catch up\n");
	}
	if (decodeIndex - nextIndex >= 0) {
		char msg[2048];
		//sprintf(msg, "Next (%d) \n", decodeIndex - nextIndex);
		moonlight_xbox_dx::Utils::Log(msg);
		Frame currentFrame = frames[nextIndex % queueSize];
		/*bool locked = currentFrame.mutex->try_lock();
		if (!locked)return;*/
		renderIndex = nextIndex;
	}
	else {
		char msg[4096];
		sprintf(msg, "Locked: %d - %d\n", decodeIndex, nextIndex);
		moonlight_xbox_dx::Utils::Log(msg);
	}
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> FramePacer::GetCurrentRenderingFrame() {
	if (frames.size() < queueSize || renderIndex < 0)return NULL;
	Frame currentFrame = frames[renderIndex % queueSize];
	currentFrame.renderMutex->AcquireSync(1, 16);
	return frames[renderIndex % queueSize].renderTexure;
}

void FramePacer::ReleaseTexture() {
	if (frames.size() < queueSize)return;
	Frame currentFrame = frames[renderIndex % queueSize];
	currentFrame.renderMutex->ReleaseSync(0);
	//currentFrame.mutex->unlock();
}