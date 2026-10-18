#include "pch.h"
#include "PyroWaveDecoder.h"
#include "FFmpegDecoder.h" // context lock + MLFrameData
#include "Pacer.h"
#include "State/Stats.h"
#include "../Common/DeviceResources.h"
#include <Utils.hpp>
#include <pyrowave_bitstream.hpp>

extern "C" {
#include <libavutil/frame.h>
}

namespace moonlight_xbox_dx {

PyroWaveDecoder &PyroWaveDecoder::instance() {
	static PyroWaveDecoder inst;
	return inst;
}

PyroWaveDecoder::PyroWaveDecoder() = default;
PyroWaveDecoder::~PyroWaveDecoder() = default;

void PyroWaveDecoder::CompleteInitialization(const std::shared_ptr<DX::DeviceResources> &res, STREAM_CONFIGURATION *config) {
	m_deviceResources = res;
	m_colorRange = config->colorRange;
}

int PyroWaveDecoder::Init(int videoFormat_, int width_, int height_, int redrawRate) {
	(void)redrawRate; // Pacer is configured in FFMpegDecoder::CompleteInitialization

	videoFormat = videoFormat_;
	width = width_;
	height = height_;
	m_LastFrameNumber = 0;
	m_StreamEpochQpc = 0;
	m_failedDecodes = 0;
	m_tenBit = (videoFormat & VIDEO_FORMAT_MASK_10BIT) != 0;

	// Previous sessions' pools: Pacer drained their frames at deinit
	m_retiredPools.clear();

	bool chroma444 = (videoFormat & VIDEO_FORMAT_MASK_YUV444) != 0;
	if (!chroma444 && ((width | height) & 1)) {
		Utils::Logf("PyroWave: 4:2:0 needs even dimensions, got %dx%d\n", width, height);
		return -1;
	}

	m_layout = std::make_unique<PyroWave::BlockLayout>();
	if (!m_layout->init(width, height, chroma444 ? PyroWave::ChromaSubsampling::Chroma444
	                                             : PyroWave::ChromaSubsampling::Chroma420)) {
		Utils::Logf("PyroWave: %dx%d cannot be represented in the bitstream\n", width, height);
		return -1;
	}

	auto *device = m_deviceResources->GetD3DDevice();
	m_decoder = std::make_unique<PyroWaveD3D12::Decoder>();
	if (!m_decoder->Init(device, m_deviceResources->GetD3DDeviceContext(), width, height, chroma444, kPoolSize)) {
		Utils::Log("PyroWave: D3D12 decoder Init failed\n");
		m_decoder.reset();
		return -1;
	}

	// The decoder writes the planes from its own D3D12 device, so they are shared.
	m_pool = std::make_unique<PyroWaveFramePool>();
	if (!m_pool->Init(device, width, height, chroma444, kPoolSize, true)) {
		Utils::Log("PyroWave: frame pool Init failed\n");
		m_decoder.reset();
		m_pool.reset();
		return -1;
	}

	// Exercise the D3D11 <-> D3D12 sharing and the decode path once before the
	// stream starts, so a failure shows here rather than as a black stream.
	if (!DecodeTestFrame()) {
		Utils::Log("PyroWave: test frame decode failed\n");
		m_decoder.reset();
		m_pool.reset();
		return -1;
	}

	m_active = true;
	Utils::Logf("PyroWave: init %dx%d %s %s, format 0x%04x\n", width, height, chroma444 ? "4:4:4" : "4:2:0",
	            m_tenBit ? "10-bit" : "8-bit", videoFormat);
	return 0;
}

// A complete frame with no active wavelet blocks decodes to neutral gray.
bool PyroWaveDecoder::DecodeTestFrame() {
	PyroWave::BitstreamSequenceHeader testFrame = {};
	testFrame.width_minus_1 = width - 1;
	testFrame.height_minus_1 = height - 1;
	testFrame.extended = 1;
	testFrame.code = PyroWave::BITSTREAM_EXTENDED_CODE_START_OF_FRAME;
	testFrame.chroma_resolution = m_pool->Chroma444() ? PyroWave::CHROMA_RESOLUTION_444 : PyroWave::CHROMA_RESOLUTION_420;

	PyroWaveFrameSet *set = m_pool->Acquire();
	if (!set)
		return false;

	bool ok = m_decoder->PushPacket(&testFrame, sizeof(testFrame)) &&
	          m_decoder->DecodeIsReady(false, kNumPristineBands, kMinimumPacketRatio);
	if (ok) {
		// The immediate context is shared with the render thread
		auto guard = FFMpegDecoder::Lock();
		ok = m_decoder->Decode(m_deviceResources->GetD3DDeviceContext(), set);
	}
	m_decoder->Clear();

	AVFrame *frame = m_pool->WrapFrame(set); // recycle the set via the free callback
	if (frame)
		av_frame_free(&frame);
	return ok;
}

void PyroWaveDecoder::Cleanup() {
	m_active = false;
	m_decoder.reset();
	if (m_pool) {
		// Frames wrapped around this pool's sets may still sit in Pacer's
		// queue until it drains; keep the pool alive until the next Init.
		m_retiredPools.push_back(std::move(m_pool));
	}
	m_LastFrameNumber = 0;

	Pacer::instance().deinit();

	Utils::Log("PyroWaveDecoder::Cleanup\n");
}

// Bit depth is negotiated for the session, but HDR can change while streaming.
// Apply the decode unit's color state to each frame, including HDR-to-SDR changes.
void PyroWaveDecoder::ApplyFrameColor(AVFrame *frame, const DECODE_UNIT *decodeUnit) {
	frame->color_range = m_colorRange == COLOR_RANGE_FULL ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;
	int colorspace = decodeUnit->hdrActive ? COLORSPACE_REC_2020 : decodeUnit->colorspace;
	frame->colorspace = colorspace == COLORSPACE_REC_601    ? AVCOL_SPC_SMPTE170M
	                    : colorspace == COLORSPACE_REC_2020 ? AVCOL_SPC_BT2020_NCL
	                                                        : AVCOL_SPC_BT709;
	frame->color_primaries = colorspace == COLORSPACE_REC_601    ? AVCOL_PRI_SMPTE170M
	                         : colorspace == COLORSPACE_REC_2020 ? AVCOL_PRI_BT2020
	                                                             : AVCOL_PRI_BT709;
	frame->color_trc = decodeUnit->hdrActive ? AVCOL_TRC_SMPTE2084 : AVCOL_TRC_BT709;
}

// Copies the decode unit into m_bitstream and records which byte range each
// RTP packet carried, whether it was lost (zero-filled), and whether the host
// flagged it as starting with a record. RTP fragments may split PyroWave
// records, so the whole decode unit is parsed at once.
bool PyroWaveDecoder::AssembleDecodeUnit(PDECODE_UNIT decodeUnit) {
	constexpr int kMaxFrameBytes = 64 * 1024 * 1024;
	if (decodeUnit->fullLength < 8 || decodeUnit->fullLength > kMaxFrameBytes ||
	    decodeUnit->fullLength % sizeof(uint32_t) != 0) {
		return false;
	}

	size_t length = 0;
	for (PLENTRY entry = decodeUnit->bufferList; entry != NULL; entry = entry->next) {
		if (!entry->data || entry->length <= 0 || size_t(entry->length) > size_t(decodeUnit->fullLength) - length)
			return false;
		length += entry->length;
	}
	if (length != size_t(decodeUnit->fullLength))
		return false;

	m_bitstream.resize(length / sizeof(uint32_t));
	m_segments.clear();
	auto *output = reinterpret_cast<uint8_t *>(m_bitstream.data());
	size_t offset = 0;
	for (PLENTRY entry = decodeUnit->bufferList; entry != NULL; entry = entry->next) {
		m_segments.push_back({ offset, size_t(entry->length), entry->bufferType == BUFFER_TYPE_LOST,
		                       entry->bufferType == BUFFER_TYPE_RECORD_START });
		memcpy(output + offset, entry->data, entry->length);
		offset += entry->length;
	}
	return true;
}

int PyroWaveDecoder::DropFrame(const char *reason) {
	FQLog("PyroWave: dropping frame: %s\n", reason);
	m_decoder->Clear();
	Stats::instance().SubmitDroppedFrame(1);
	// Every frame is independent. A dropped frame needs neither an IDR request
	// nor a decoder reset, even when several network frames are malformed.
	return DR_OK;
}

int PyroWaveDecoder::DecodeFailed(const char *reason) {
	Utils::Logf("PyroWave: decode failed: %s\n", reason);
	m_decoder->Clear();
	Stats::instance().SubmitDroppedFrame(1);
	if (++m_failedDecodes == kFailedDecodesWarnThreshold) {
		Utils::Logf("PyroWave: decode failed %d times in a row\n", m_failedDecodes);
	}
	return DR_OK;
}

// Called by the VideoDec thread (CAPABILITY_DIRECT_SUBMIT)
int PyroWaveDecoder::SubmitDecodeUnit(PDECODE_UNIT decodeUnit) {
	LARGE_INTEGER decodeStart, decodeEnd;
	QueryPerformanceCounter(&decodeStart);
	if (m_StreamEpochQpc == 0)
		m_StreamEpochQpc = decodeStart.QuadPart;

	if (!m_decoder || !m_pool)
		return DR_OK;

	// Detect breaks in the frame sequence indicating dropped frames
	uint32_t droppedFramesNetwork = 0;
	if (m_LastFrameNumber > 0 && decodeUnit->frameNumber > (m_LastFrameNumber + 1)) {
		droppedFramesNetwork = decodeUnit->frameNumber - (m_LastFrameNumber + 1);
	}
	m_LastFrameNumber = decodeUnit->frameNumber;

	if (!decodeUnit->rtpTimestamp) {
		// Estimate for hosts that don't send timestamps (e.g. Wolf)
		LogOnce("Warning: host is not sending RTP timestamps, this may hurt frame pacing\n");
		double ptsMs = QpcToMs(QpcNow() - m_StreamEpochQpc);
		decodeUnit->rtpTimestamp = (uint32_t)(ptsMs * 90.0);
		decodeUnit->presentationTimeUs = (uint64_t)(ptsMs * 1000.0);
	}

	Stats::instance().SubmitVideoBytesAndReassemblyTime(decodeUnit->fullLength, decodeUnit, droppedFramesNetwork);

	if (decodeUnit->hdrActive && !m_tenBit) {
		return DropFrame("HDR frame received on an 8-bit stream");
	}
	if (!AssembleDecodeUnit(decodeUnit)) {
		return DropFrame("invalid frame framing");
	}
	std::string parseError;
	if (!PyroWaveFraming::parse(reinterpret_cast<const uint8_t *>(m_bitstream.data()),
	                            m_bitstream.size() * sizeof(uint32_t), m_segments,
	                            decodeUnit->pyrowaveCriticalPackets, *m_layout, m_parsed, parseError)) {
		return DropFrame(parseError.c_str());
	}
	if (m_parsed.partial && !m_parsed.coarseLevelIntact) {
		Stats::instance().SubmitPyroWavePartialFrame(false);
		return DropFrame("part of the coarsest wavelet level was lost");
	}

	// Each decode unit is an independent frame. Reset the decoder's three-bit
	// sequence tracker so network gaps cannot make a newer frame look like an
	// old packet.
	m_decoder->Clear();
	for (const auto &span : m_parsed.spans) {
		if (!m_decoder->PushPacket(reinterpret_cast<const uint8_t *>(m_bitstream.data()) + span.offset, span.size)) {
			if (m_parsed.partial) {
				Stats::instance().SubmitPyroWavePartialFrame(false);
			}
			return DropFrame("codec rejected a record");
		}
	}

	bool ready = m_decoder->DecodeIsReady(m_parsed.partial, kNumPristineBands, kMinimumPacketRatio);
	if (m_parsed.partial) {
		Stats::instance().SubmitPyroWavePartialFrame(ready);
	}
	if (!ready) {
		return DropFrame("corrupt or incomplete bitstream");
	}

	PyroWaveFrameSet *set = m_pool->Acquire();
	if (!set) {
		// All plane sets in flight (renderer far behind). Every frame is
		// independent, so no keyframe is needed after dropping this one.
		return DropFrame("frame pool exhausted");
	}

	bool decoded;
	double gpuMs;
	bool haveGpuMs;
	{
		// The immediate context is shared with the render thread
		auto guard = FFMpegDecoder::Lock();
		decoded = m_decoder->Decode(m_deviceResources->GetD3DDeviceContext(), set);
		// Non-blocking: the GPU time of a frame decoded a few frames ago, if done.
		haveGpuMs = m_decoder->PollGpuTimeMs(&gpuMs);
	}
	if (haveGpuMs) {
		Stats::instance().SubmitGpuDecodeMs(gpuMs);
	}
	if (!decoded) {
		AVFrame *tmp = m_pool->WrapFrame(set); // recycle the set via the free callback
		if (tmp)
			av_frame_free(&tmp);
		return DecodeFailed("GPU decode submission failed");
	}
	m_failedDecodes = 0;

	AVFrame *frame = m_pool->WrapFrame(set);
	if (!frame) {
		return DecodeFailed("output frame allocation failed");
	}
	frame->pts = (int64_t)decodeUnit->rtpTimestamp;
	ApplyFrameColor(frame, decodeUnit);

	// Pacing userdata, same contract as the FFmpeg path
	QueryPerformanceCounter(&decodeEnd);
	AVBufferRef *buf = av_buffer_allocz(sizeof(MLFrameData));
	if (buf) {
		((MLFrameData *)buf->data)->decodeEndQpc = decodeEnd.QuadPart;
		frame->opaque_ref = buf;
	}

	// Frame is owned by Pacer from here (drops/errors recycle via the free callback)
	Pacer::instance().submitFrame(frame);

	Stats::instance().SubmitDecodeMs(QpcToMs(decodeEnd.QuadPart - decodeStart.QuadPart));
	return DR_OK;
}

} // namespace moonlight_xbox_dx
