#pragma once
#include "pch.h"
#include <queue>
extern "C" {
#include <libavcodec/avcodec.h>
}
struct Frame
{
	//Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	AVFrame *frame;
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
	Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
};

