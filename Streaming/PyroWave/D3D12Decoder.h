#pragma once

// D3D12 backend for the PyroWave decoder (see PyroWaveDecoder). Wraps the
// pyrowave_d3d12 library from the PyroWave repository, whose Shader Model 6.4
// kernels use real wave intrinsics.
//
// The renderer stays D3D11. Decoded planes are PyroWaveFramePool textures created
// shared (NT handle) on the app's D3D11 device and opened here on a D3D12 device on
// the same adapter. Per frame, on the D3D11 immediate context (caller holds the
// context lock):
//   1. D3D11 signals a shared fence: everything already submitted, including the
//      renderer's reads of this plane set from its previous use, precedes it.
//   2. The D3D12 queue waits for that value, decodes into the planes, signals its
//      own shared fence.
//   3. The D3D11 context waits for the decode, so every later D3D11 command, the
//      draw that presents this frame included, sees the decoded planes.
// All waits are GPU-side; the CPU never blocks on the decode.

#include "FramePool.h"
#include <d3d11_4.h>
#include <d3d12.h>
#include <deque>
#include <vector>
#include <wrl/client.h>

struct pyrowave_d3d12_device_opaque;
struct pyrowave_d3d12_decoder_opaque;

namespace moonlight_xbox_dx {
namespace PyroWaveD3D12 {

class Decoder {
  public:
	~Decoder();

	// Returns false (after logging why) if D3D12, the PyroWave kernels or the
	// D3D11 <-> D3D12 sharing are not available. The PyroWaveFramePool used with
	// Decode() must be created shared.
	bool Init(ID3D11Device *device11, ID3D11DeviceContext *context11, int width, int height, bool chroma444,
	          int poolSize);

	// Feeds whole bitstream records (a span from PyroWaveFraming::parse).
	bool PushPacket(const void *data, size_t size);
	// A partial frame is ready when its numPristineBands coarsest bands arrived
	// intact and at least minimumPacketRatio of its blocks did.
	bool DecodeIsReady(bool allowPartialFrame, int numPristineBands, float minimumPacketRatio) const;
	void Clear();

	// Records and submits the decode into the set's planes. Caller must hold the
	// D3D11 device-context lock.
	bool Decode(ID3D11DeviceContext *context11, PyroWaveFrameSet *set);

	// Non-blocking: GPU time of some recent decode, when one has completed.
	bool PollGpuTimeMs(double *outMs);

  private:
	bool OpenPlanes(PyroWaveFrameSet *set);

	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_list;

	static constexpr int kInFlight = 4;
	struct Submission {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
		UINT64 fenceValue = 0;
	};
	Submission m_submissions[kInFlight];
	int m_nextSubmission = 0;
	// Submission slots whose timestamps have not been read yet, oldest first.
	std::deque<int> m_pendingTimings;
	HANDLE m_waitEvent = nullptr;

	// D3D12 -> D3D11: decode finished. Owned by D3D12, opened on D3D11.
	Microsoft::WRL::ComPtr<ID3D12Fence> m_decodeFence;
	Microsoft::WRL::ComPtr<ID3D11Fence> m_decodeFenceOn11;
	UINT64 m_decodeValue = 0;

	// D3D11 -> D3D12: prior D3D11 work (the renderer's reads) finished.
	Microsoft::WRL::ComPtr<ID3D11Fence> m_releaseFence;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_releaseFenceOn12;
	UINT64 m_releaseValue = 0;

	Microsoft::WRL::ComPtr<ID3D11DeviceContext4> m_context4;

	// GPU timing: two timestamps per submission slot, resolved into a readback buffer.
	Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_timestamps;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_timestampReadback;
	UINT64 m_timestampFrequency = 0;

	// Planes opened on D3D12, indexed by PyroWaveFrameSet::index.
	struct Planes {
		Microsoft::WRL::ComPtr<ID3D12Resource> tex[3];
	};
	std::vector<Planes> m_planes;

	pyrowave_d3d12_device_opaque *m_pwDevice = nullptr;
	pyrowave_d3d12_decoder_opaque *m_pwDecoder = nullptr;
};

} // namespace PyroWaveD3D12
} // namespace moonlight_xbox_dx
