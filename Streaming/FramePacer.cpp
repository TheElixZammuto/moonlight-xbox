#include "pch.h"
#include "FramePacer.h"
#include <Common\DirectXHelper.h>
#include <Utils.hpp>
#include <synchapi.h>

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
	QueryPerformanceCounter(&startTimer);
	QueryPerformanceFrequency(&frequency);
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
			0,
			width,
			height
		});
	}
}
void FramePacer::SubmitFrame(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,int index,Microsoft::WRL::ComPtr<ID3D11DeviceContext> decodeContext) {
	int i = (decodeIndex + 1) % queueSize;
	VideoFrame currentFrame = frames[i];
	D3D11_TEXTURE2D_DESC desc;
	texture->GetDesc(&desc);
	D3D11_BOX box;
	box.left = 0;
	box.top = 0;
	box.right = min(desc.Width, currentFrame.textureWidth);
	box.bottom = min(desc.Height, currentFrame.textureHeight);
	box.front = 0;
	box.back = 1;
	currentFrame.frameNumber = decodeIndex + 1;
	HRESULT status = currentFrame.decodeMutex->AcquireSync(0, INFINITE);
	if (status != S_OK) {
		return;
	}
	decodeContext->CopySubresourceRegion(currentFrame.decodeTexture.Get(), 0, 0, 0, 0, texture.Get(), index, &box);
	status = currentFrame.decodeMutex->ReleaseSync(1);
	if (status != S_OK) {
		return;
	}
	decodeIndex = decodeIndex + 1;
	statsMutex.lock();
	decodedFrames++;
	statsMutex.unlock();
	UpdateStats();
	//currentFrame.mutex->unlock();

}

void FramePacer::UpdateStats() {
	statsMutex.lock();
	QueryPerformanceCounter(&lastTimer);
	if (lastTimer.QuadPart - startTimer.QuadPart > frequency.QuadPart) {
		startTimer = lastTimer;
		moonlight_xbox_dx::Utils::stats.decoderFPS = decodedFrames;
		moonlight_xbox_dx::Utils::stats.rendererFPS = renderedFrames;
		moonlight_xbox_dx::Utils::stats.drLatency = decodeIndex - renderIndex;
		moonlight_xbox_dx::Utils::stats.averageRenderingTime = totalRenderingTime / (double) renderedFrames;
		decodedFrames = 0;
		renderedFrames = 0;
		totalRenderingTime = 0;
	}
	statsMutex.unlock();
}

bool FramePacer::PrepareFrameForRendering() {
	if (decodeIndex < 0)return false;
	int nextIndex = renderIndex + 1;
	if (decodeIndex - nextIndex >= 0) {
		int i = nextIndex % queueSize;
		VideoFrame nextFrame = frames[i];
		HRESULT status = nextFrame.renderMutex->AcquireSync(1, INFINITE);
		if (status == S_OK) {
			renderIndex = nextIndex;
			return true;
		}
		else {
			return false;
		}
	}
	UpdateStats();
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> FramePacer::GetCurrentRenderingFrame() {
	if (frames.size() < queueSize || renderIndex < 0)return NULL;
	VideoFrame currentFrame = frames[renderIndex % queueSize];
	statsMutex.lock();
	renderedFrames++;
	statsMutex.unlock();
	return frames[renderIndex % queueSize].renderTexure;
}

void FramePacer::ReleaseTexture() {
	int i = renderIndex % queueSize;
	if (renderIndex >= 0 && decodeIndex >= 0) {
		VideoFrame currentFrame = frames[i];
		currentFrame.renderMutex->ReleaseSync(0);
	}
}
