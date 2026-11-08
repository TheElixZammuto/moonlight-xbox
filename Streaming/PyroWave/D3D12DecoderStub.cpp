// clang-format off
#include "pch.h"
// clang-format on

// W10M (ARM) build: Windows 10 Mobile has no D3D12, and the PyroWave kernels
// need Shader Model 6.4 wave intrinsics regardless. This stub replaces
// D3D12Decoder.cpp so the app links without importing d3d12.dll (which would
// fail to load on the phone). Every entry point reports "unavailable":
// PyroWaveDecoder::Init() fails with -1 and streaming uses the FFmpeg decoder
// path, exactly as on desktops without D3D12 support.

#include "D3D12Decoder.h"

namespace moonlight_xbox_dx {
namespace PyroWaveD3D12 {

Decoder::~Decoder() {}

bool Decoder::Init(ID3D11Device *device11, ID3D11DeviceContext *context11, int width, int height, bool chroma444,
                   int poolSize) {
	return false;
}

bool Decoder::PushPacket(const void *data, size_t size) {
	return false;
}

bool Decoder::DecodeIsReady(bool allowPartialFrame, int numPristineBands, float minimumPacketRatio) const {
	return false;
}

void Decoder::Clear() {}

bool Decoder::Decode(ID3D11DeviceContext *context11, PyroWaveFrameSet *set) {
	return false;
}

bool Decoder::PollGpuTimeMs(double *outMs) {
	return false;
}

} // namespace PyroWaveD3D12
} // namespace moonlight_xbox_dx
