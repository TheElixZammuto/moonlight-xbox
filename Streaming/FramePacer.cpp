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
			0
		});
	}
}
void FramePacer::SubmitFrame(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,int index,Microsoft::WRL::ComPtr<ID3D11DeviceContext> decodeContext) {
	Frame currentFrame = frames[(decodeIndex+1) % queueSize];
	currentFrame.frameNumber = decodeIndex +1;
	currentFrame.decodeMutex->AcquireSync(0, 100);
	decodeContext->CopySubresourceRegion(currentFrame.decodeTexture.Get(), 0, 0, 0, 0, texture.Get(), index, NULL);
	currentFrame.decodeMutex->ReleaseSync(1);
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
	}
	statsMutex.unlock();
}

void FramePacer::PrepareFrameForRendering() {
	if (decodeIndex < 0)return;
	bool advanced = false;
	int nextIndex = renderIndex + 1;
	int di = decodeIndex;
	//Try a couple times before going to next frame
	for (int i = 0; i < 16; i++) {
		nextIndex = renderIndex + 1;
		di = decodeIndex;
		if (di - nextIndex > 4) {
			nextIndex = di;
			moonlight_xbox_dx::Utils::Log("Catch up\n");
			droppedFrames = 0;
		}
		if (di - nextIndex >= 0) {
			droppedFrames = 0;
			if (renderIndex >= 0 && decodeIndex > 0) {
				Frame currentFrame = frames[renderIndex % queueSize];
				currentFrame.renderMutex->ReleaseSync(0);
			}
			Frame nextFrame = frames[nextIndex % queueSize];
			nextFrame.renderMutex->AcquireSync(1, 100);
			renderIndex = nextIndex;
			advanced = true;
			break;
		}
		UpdateStats();
		//Sleep(1);
	}
	if (!advanced) {
		//char msg[4096];
		//std::snprintf(msg, sizeof(msg), "Locked: %d - %d (%d)\n", nextIndex, di,droppedFrames);
		//moonlight_xbox_dx::Utils::Log(msg);
		droppedFrames++;
	}
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> FramePacer::GetCurrentRenderingFrame() {
	if (frames.size() < queueSize || renderIndex < 0)return NULL;
	Frame currentFrame = frames[renderIndex % queueSize];
	statsMutex.lock();
	renderedFrames++;
	statsMutex.unlock();
	return frames[renderIndex % queueSize].renderTexure;
}

void FramePacer::ReleaseTexture() {
	
}