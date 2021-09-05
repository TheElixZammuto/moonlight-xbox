#pragma once
#include "pch.h"
#include <queue>
struct Frame
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	Microsoft::WRL::ComPtr <IDXGIKeyedMutex> mutex;
};

class FramePacer
{
public:
	void SubmitFrame(Frame frame);
	int queueSize = 4;
	Microsoft::WRL::ComPtr<ID3D11Device1> renderingDevice, decodingDevice;
	void PrepareFrameForRendering();
	Microsoft::WRL::ComPtr<ID3D11Texture2D> GetCurrentRenderingFrame();
private:
	std::queue<Frame> textures;
	Frame preparedFrame;
	HANDLE sharedHandle;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> currentFrameTexture;
	Microsoft::WRL::ComPtr<IDXGIKeyedMutex> mutex;
	Microsoft::WRL::ComPtr<IDXGIResource1> dxgiResource;
	bool textureLock = false;
};

