#pragma once
#include "pch.h"
#include <queue>
struct Frame
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
};

class FramePacer
{
public:
	void SubmitFrame(Frame frame);
	int queueSize = 4;
	Microsoft::WRL::ComPtr<ID3D11Device1> renderingDevice, decodingDevice;
	void PrepareFrameForRendering();
	Microsoft::WRL::ComPtr<ID3D11Texture2D> GetCurrentRenderingFrame();
	void ReleaseFrame();
private:
	std::queue<Frame> textures;
	Frame preparedFrame;
	HANDLE sharedHandle;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> currentFrameTexture;
	Microsoft::WRL::ComPtr<IDXGIKeyedMutex> mutex;
	bool textureLock = false;
};

