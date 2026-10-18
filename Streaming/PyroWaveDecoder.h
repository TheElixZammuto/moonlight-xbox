#pragma once

// Moonlight-facing PyroWave decoder, sibling of FFMpegDecoder. The
// DECODER_RENDERER_CALLBACKS in FFmpegDecoder.cpp route here when the
// negotiated format has VIDEO_FORMAT_MASK_PYROWAVE set.
//
// Each decode unit is one independent frame. moonlight-common-c delivers it
// with lost packets zero-filled and marked (BUFFER_TYPE_LOST), and
// PyroWaveFraming::parse turns it into runs of intact bitstream records,
// skipping records that lost bytes. The records go to the D3D12 decoder, which
// decodes into PyroWaveFramePool planes; the wrapped AVFrame goes to Pacer,
// which owns it from then on (the pool recycles plane sets via the AVFrame free
// callback). The receive path follows moonlight-qt's Metal PyroWave decoder.

#include "PyroWave\D3D12Decoder.h"
#include "PyroWave\FramePool.h"
#include "PyroWave\PyroWaveFraming.h"
#include <memory>
#include <vector>

extern "C" {
#include <Limelight.h>
}

namespace DX {
class DeviceResources;
}

namespace moonlight_xbox_dx {

class PyroWaveDecoder {
  public:
	static PyroWaveDecoder &instance();

	// Called before connection start (alongside FFMpegDecoder's): stashes
	// device resources and the negotiated color range.
	void CompleteInitialization(const std::shared_ptr<DX::DeviceResources> &res, STREAM_CONFIGURATION *config);

	int Init(int videoFormat, int width, int height, int redrawRate);
	void Cleanup();
	int SubmitDecodeUnit(PDECODE_UNIT decodeUnit);

	bool IsActive() const { return m_active; }

	int videoFormat = 0;
	int width = 0, height = 0;

  private:
	PyroWaveDecoder();
	~PyroWaveDecoder();
	PyroWaveDecoder(const PyroWaveDecoder &) = delete;
	PyroWaveDecoder &operator=(const PyroWaveDecoder &) = delete;

	// Plane sets in the output pool. DUs can arrive in a burst right after loss
	// recovery while the renderer is still catching up.
	static constexpr int kPoolSize = 6;
	// Partial-frame readiness, as upstream and moonlight-qt: the two coarsest
	// bands must be intact and 90% of the blocks must have arrived.
	static constexpr int kNumPristineBands = 2;
	static constexpr float kMinimumPacketRatio = 0.9f;
	static constexpr int kFailedDecodesWarnThreshold = 20;

	bool AssembleDecodeUnit(PDECODE_UNIT decodeUnit);
	bool DecodeTestFrame();
	int DropFrame(const char *reason);
	int DecodeFailed(const char *reason);
	void ApplyFrameColor(AVFrame *frame, const DECODE_UNIT *decodeUnit);

	std::shared_ptr<DX::DeviceResources> m_deviceResources;
	int m_colorRange = COLOR_RANGE_LIMITED; // COLOR_RANGE_* from STREAM_CONFIGURATION

	std::unique_ptr<PyroWaveD3D12::Decoder> m_decoder;
	std::unique_ptr<PyroWaveFramePool> m_pool;
	// Pools whose frames may still be in flight when a new session starts;
	// freed on the next Init (Pacer has long since drained them by then).
	std::vector<std::unique_ptr<PyroWaveFramePool>> m_retiredPools;

	// Per-frame parse state, kept to reuse allocations
	std::unique_ptr<PyroWave::BlockLayout> m_layout;
	std::vector<uint32_t> m_bitstream;
	std::vector<PyroWaveFraming::Segment> m_segments;
	PyroWaveFraming::Frame m_parsed;

	int m_LastFrameNumber = 0;
	int64_t m_StreamEpochQpc = 0;
	int m_failedDecodes = 0;
	bool m_tenBit = false;
	bool m_active = false;
};

} // namespace moonlight_xbox_dx
