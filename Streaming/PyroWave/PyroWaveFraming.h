// Copied from moonlight-qt (app/streaming/video/pyrowaveframing.h), which adapted it
// from Nonary moonlight-qt vrr18 (record framing and partial recovery). Kept free of
// app dependencies so it builds standalone for desktop tests.
#pragma once

// Parses one PyroWave frame as delivered by moonlight-common-c into the runs of
// PyroWave records that pyrowave_decoder_push_packet() accepts. Supports both
// framings in Vibeshine's PyroWave protocol:
//
//  - Record framing: a sequence header, block records and padding records
//    (0xFFFFFFFF, N, N zero words), concatenated.
//  - Length-prefixed framing: [u32 count] { [u32 size] [packet] } * count.
//
// This adapter validates transport boundaries and enforces one sequence per
// frame. Codec validation (dimensions, chroma, header mode and block indices)
// belongs to the upstream BitstreamParser used by pyrowave_decoder_push_packet().
// Records shorter than their headers are rejected here before reaching it.
//
// A record-framed frame may arrive with packets missing (zero-filled by
// moonlight-common-c). Records that lost any byte are skipped. When a record
// header itself was lost, parsing resumes at the next received packet that the
// host flagged as starting with a record. Without flags, it resumes at the next
// received packet once the finer levels' oversized records (larger than a
// packet, sent before their other records) are behind, since the host packs
// every other record without crossing a packet boundary. The frame is then
// marked partial.
//
// The permissive partial-frame policy requires an intact coarsest-level prefix.
// The host sends those blocks (below get_num_active_blocks(layout, 2)) first,
// right after the sequence header, protects the
// packets holding them with parity, and announces how many packets that is in
// the frame header. Hosts that do not announce it still send them first, so
// they are known to be intact when no loss precedes the first finer block.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PyroWave { struct BlockLayout; }

namespace PyroWaveFraming {

enum class Framing {
    Records,
    LengthPrefixed,
};

// A byte range of the input that holds whole records and no padding.
struct Span {
    size_t offset;
    size_t size;
};

// The part of the frame one RTP packet carried, whether that packet was lost
// (its bytes are then zero-filled and meaningless), and whether the host
// flagged it as starting with a record.
struct Segment {
    size_t offset;
    size_t size;
    bool lost;
    bool recordStart = false;
};

struct Frame {
    Framing framing = Framing::Records;
    std::vector<Span> spans;
    // Number of block records passed on (excluding sequence headers and padding)
    uint32_t blockRecords = 0;
    // total_blocks from the sequence header, or 0 if none was present
    uint32_t announcedBlocks = 0;
    bool sequenceHeaderSeen = false;
    uint32_t sequence = 0;
    uint32_t paddingBytes = 0;
    // Some records were lost with their packets
    bool partial = false;
    // Every coarsest-level block that was sent arrived (always true when not partial)
    bool coarseLevelIntact = true;
};

constexpr uint32_t k_PaddingMagic = 0xFFFFFFFFu;

// segments must tile [0, size) in order, or be empty for a frame that arrived
// whole and whose packet boundaries are unknown. criticalPackets is the number
// of leading packets the host announced as holding the coarsest level, or 0.
bool parse(const uint8_t* data, size_t size, const std::vector<Segment>& segments,
           size_t criticalPackets, const PyroWave::BlockLayout& layout, Frame& frame, std::string& error);

bool parse(const uint8_t* data, size_t size, const std::vector<Segment>& segments,
           const PyroWave::BlockLayout& layout, Frame& frame, std::string& error);

bool parse(const uint8_t* data, size_t size, const PyroWave::BlockLayout& layout,
           Frame& frame, std::string& error);

}
