// Copied from moonlight-qt (app/streaming/video/pyrowaveframing.cpp), which adapted it
// from Nonary moonlight-qt vrr18 (record framing and partial recovery).
#include "PyroWaveFraming.h"

#include <pyrowave_bitstream.hpp>
#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace PyroWaveFraming {

namespace {

constexpr size_t k_HeaderBytes = sizeof(PyroWave::BitstreamHeader);
constexpr uint32_t k_HeaderWords = k_HeaderBytes / sizeof(uint32_t);

uint32_t readU32(const uint8_t* p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

template <typename T>
T readHeader(const uint8_t* data)
{
    // The upstream wire structs target little-endian macOS. Copy instead of
    // casting so the transport adapter can also inspect unaligned input.
    T header;
    std::memcpy(&header, data, sizeof(header));
    return header;
}

// The frame's packets, looked up by byte offset. Without segments, the frame
// is one received packet of unknown boundaries.
class SegmentMap
{
public:
    explicit SegmentMap(const std::vector<Segment>& segments)
        : m_Segments(segments),
          m_AnyLost(std::any_of(segments.begin(), segments.end(),
                               [](const Segment& segment) { return segment.lost; }))
    {
    }

    bool anyLost() const
    {
        return m_AnyLost;
    }

    // Whether any byte of [start, end) was lost
    bool lost(size_t start, size_t end) const
    {
        // Most frames are intact. Avoid a packet lookup for every record in
        // that case, while retaining the same record validation below.
        if (!m_AnyLost) {
            return false;
        }
        for (size_t i = indexOf(start); i < m_Segments.size() && m_Segments[i].offset < end; i++) {
            if (m_Segments[i].lost) {
                return true;
            }
        }
        return false;
    }

    // Whether [start, end) came in a single packet
    bool withinOnePacket(size_t start, size_t end) const
    {
        const size_t i = indexOf(start);
        return i < m_Segments.size() && end <= m_Segments[i].offset + m_Segments[i].size;
    }

    // Frame bytes a full RTP packet carries: the first packet is full whenever
    // another follows, and also carries the 8-byte frame header. SIZE_MAX when
    // unknown (a single packet, or no packet map).
    size_t payloadSize() const
    {
        return m_Segments.size() >= 2 ? m_Segments[0].size + 8 : std::numeric_limits<size_t>::max();
    }

    // Whether the host flags the packets that start with a record (it always
    // flags the first, which starts with the sequence header)
    bool recordStartsFlagged() const
    {
        return !m_Segments.empty() && m_Segments[0].recordStart;
    }

    // Start of the first received packet after pos (flagged as starting with a
    // record when flagRequired), or SIZE_MAX
    size_t nextReceivedStart(size_t pos, bool flagRequired) const
    {
        for (size_t i = indexOf(pos) + 1; i < m_Segments.size(); i++) {
            if (!m_Segments[i].lost && (m_Segments[i].recordStart || !flagRequired)) {
                return m_Segments[i].offset;
            }
        }
        return std::numeric_limits<size_t>::max();
    }

private:
    // Index of the segment holding pos (segments tile the frame)
    size_t indexOf(size_t pos) const
    {
        if (m_Segments.empty()) {
            return 0;
        }
        // Record traversal moves forward, often looking up the same packet
        // several times. Advance once per packet rather than binary-searching
        // the entire map for each record header and payload.
        if (pos >= m_Segments[m_Index].offset) {
            while (m_Index + 1 < m_Segments.size() &&
                    m_Segments[m_Index + 1].offset <= pos) {
                ++m_Index;
            }
            return m_Index;
        }
        auto it = std::upper_bound(m_Segments.begin(), m_Segments.end(), pos,
                                   [](size_t value, const Segment& segment) { return value < segment.offset; });
        if (it == m_Segments.begin()) {
            return m_Segments.size();
        }
        m_Index = size_t(it - m_Segments.begin()) - 1;
        return m_Index;
    }

    const std::vector<Segment>& m_Segments;
    const bool m_AnyLost;
    mutable size_t m_Index = 0;
};

// Called with at least a full header. Inspect only what the transport walker
// needs; the decoder's BitstreamParser owns codec validation and accumulation.
bool inspectRecord(const uint8_t* data, size_t size, uint32_t maxBlocks,
                   Frame& frame, PyroWave::BitstreamHeader& header,
                   size_t& recordBytes, std::string& error)
{
    header = readHeader<PyroWave::BitstreamHeader>(data);
    if (header.extended) {
        if (frame.sequenceHeaderSeen) {
            error = "second sequence header";
            return false;
        }
        const auto sequence = readHeader<PyroWave::BitstreamSequenceHeader>(data);
        if (sequence.total_blocks > maxBlocks) {
            error = "sequence header announces more blocks than the frame can hold";
            return false;
        }
        frame.sequenceHeaderSeen = true;
        frame.sequence = sequence.sequence;
        frame.announcedBlocks = sequence.total_blocks;
        recordBytes = sizeof(sequence);
        return true;
    }

    if (!frame.sequenceHeaderSeen || header.sequence != frame.sequence) {
        error = "block record before sequence header or from another frame";
        return false;
    }
    // Keep this guard even for duplicate blocks: upstream currently bypasses
    // its minimum-length check for duplicates, and zero length cannot advance.
    if (header.payload_words < k_HeaderWords) {
        error = "block record shorter than its header";
        return false;
    }
    recordBytes = size_t(header.payload_words) * sizeof(uint32_t);
    if (recordBytes > size) {
        error = "block record runs past the end of the packet or frame";
        return false;
    }
    return true;
}

// Walks a record-framed frame, skipping records that lost bytes with their
// packets. The host sends the sequence header, the coarsest wavelet level, then
// the finer levels: first their oversized records (too large to share a packet
// with a padding record), then the rest packed so that none crosses a packet
// boundary. Once a finer record of ordinary size shows the oversized ones are
// behind, a lost record header is recovered from at the next received packet.
// Before that, and for hosts that let ordinary records straddle packets, the
// rest of the frame is lost.
bool walkRecordFrame(const uint8_t* data, size_t size, const SegmentMap& segments,
                     bool coarseLevelKnown, const PyroWave::BlockLayout& layout,
                     Frame& frame, std::string& error)
{
    const uint32_t coarseBlocks = uint32_t(PyroWave::get_num_active_blocks(layout, 2));
    const size_t payloadSize = segments.payloadSize();
    const bool flagged = segments.recordStartsFlagged();
    size_t pos = 0;
    size_t spanStart = 0;
    // Every later packet starts with a record
    bool aligned = false;
    // Every coarsest-level block has been passed
    bool pastCoarseLevel = false;

    auto skipTo = [&](size_t next) {
        if (pos > spanStart) {
            frame.spans.push_back({spanStart, pos - spanStart});
        }
        pos = spanStart = next;
    };

    auto noteLoss = [&]() {
        frame.partial = true;
        if (!pastCoarseLevel && !coarseLevelKnown) {
            frame.coarseLevelIntact = false;
        }
    };

    while (pos < size) {
        if (segments.lost(pos, std::min(pos + k_HeaderBytes, size))) {
            noteLoss();
            skipTo(flagged || aligned ? std::min(segments.nextReceivedStart(pos, flagged), size) : size);
            continue;
        }

        if (size - pos < k_HeaderBytes) {
            error = "trailing bytes shorter than a record header";
            return false;
        }

        const uint32_t word0 = readU32(data + pos);
        const uint32_t word1 = readU32(data + pos + 4);

        if (word0 == k_PaddingMagic) {
            const uint64_t padBytes = k_HeaderBytes + uint64_t(word1) * 4;
            if (padBytes > size - pos) {
                error = "padding record runs past the end of the frame";
                return false;
            }

            frame.paddingBytes += uint32_t(padBytes);
            skipTo(pos + size_t(padBytes));
            continue;
        }

        PyroWave::BitstreamHeader header;
        size_t recordBytes;
        if (!inspectRecord(data + pos, size - pos, uint32_t(layout.block_count_32x32),
                           frame, header, recordBytes, error)) {
            return false;
        }
        if (header.extended) {
            pos += recordBytes;
            continue;
        }
        const uint32_t blockIndex = header.block_index;

        // The whole coarsest level precedes the first finer block
        if (blockIndex >= coarseBlocks) {
            pastCoarseLevel = true;
        }

        const size_t end = pos + recordBytes;
        if (segments.lost(pos, end)) {
            // Only a record that spans packets can lose its payload but not its header
            noteLoss();
            aligned = false;
            skipTo(end);
            continue;
        }

        // Only the finer level's ordinary records are packed so that none crosses a
        // packet boundary; its oversized ones come first.
        const bool ordinary = end - pos + k_HeaderBytes <= payloadSize;
        aligned = segments.anyLost() && ordinary && blockIndex >= coarseBlocks &&
                  segments.withinOnePacket(pos, end);
        frame.blockRecords++;
        pos = end;
    }

    skipTo(pos);
    return true;
}

// Walks one length-prefixed packet, which never contains padding.
bool walkPacket(const uint8_t* data, size_t base, size_t size,
                const PyroWave::BlockLayout& layout,
                Frame& frame, std::string& error)
{
    size_t pos = 0;

    while (pos < size) {
        if (size - pos < k_HeaderBytes) {
            error = "trailing bytes shorter than a record header";
            return false;
        }

        const uint32_t word0 = readU32(data + base + pos);

        if (word0 == k_PaddingMagic) {
            error = "padding record inside a length-prefixed packet";
            return false;
        }

        PyroWave::BitstreamHeader header;
        size_t recordBytes;
        if (!inspectRecord(data + base + pos, size - pos, uint32_t(layout.block_count_32x32),
                           frame, header, recordBytes, error)) {
            return false;
        }
        if (!header.extended) {
            frame.blockRecords++;
        }
        pos += recordBytes;
    }

    if (size != 0) {
        frame.spans.push_back({base, size});
    }
    return true;
}
}

bool parse(const uint8_t* data, size_t size, const PyroWave::BlockLayout& layout,
           Frame& frame, std::string& error)
{
    return parse(data, size, {}, 0, layout, frame, error);
}

bool parse(const uint8_t* data, size_t size, const std::vector<Segment>& segments,
           const PyroWave::BlockLayout& layout, Frame& frame, std::string& error)
{
    return parse(data, size, segments, 0, layout, frame, error);
}

bool parse(const uint8_t* data, size_t size, const std::vector<Segment>& segments,
           size_t criticalPackets, const PyroWave::BlockLayout& layout, Frame& frame, std::string& error)
{
    // Keep the per-decoder span allocation across frames. All semantic state
    // is still reset, including after a failed or partial frame.
    auto spans = std::move(frame.spans);
    spans.clear();
    frame = Frame();
    frame.spans = std::move(spans);

    if (data == nullptr || size < k_HeaderBytes || (size % 4) != 0) {
        error = "frame is empty or not word aligned";
        return false;
    }
    if (layout.block_count_32x32 <= 0) {
        error = "stream layout was not initialized";
        return false;
    }

    size_t tiled = 0;
    for (const auto& segment : segments) {
        if (segment.offset != tiled || segment.size == 0 || segment.size > size - tiled) {
            error = "packet map does not tile the frame";
            return false;
        }
        tiled += segment.size;
    }
    if (!segments.empty() && tiled != size) {
        error = "packet map does not tile the frame";
        return false;
    }

    const SegmentMap packets(segments);

    // The first packet carries the sequence header; without it nothing decodes
    if (packets.lost(0, k_HeaderBytes)) {
        error = "the frame's first packet was lost";
        return false;
    }

    const uint32_t firstWord = readU32(data);

    // A record-framed frame starts with a sequence header (extended bit set,
    // which a padding record also has); a packet count never sets bit 31.
    if (readHeader<PyroWave::BitstreamHeader>(data).extended) {
        frame.framing = Framing::Records;
        // Vibeshine only aligns records to shards for word-aligned payloads of
        // at least 24 bytes. Otherwise a surviving packet start is not a safe
        // place to resume after a lost header.
        if (packets.anyLost() && (packets.payloadSize() < 24 || packets.payloadSize() % 4 != 0)) {
            error = "packet size does not support partial record recovery";
            return false;
        }

        // With the count announced, the coarsest level is intact exactly when its
        // packets are; parity has usually repaired them already.
        const bool coarseLevelKnown = criticalPackets != 0 && !segments.empty();
        if (coarseLevelKnown) {
            if (criticalPackets > segments.size()) {
                error = "critical packet count exceeds the frame";
                return false;
            }
            const size_t count = std::min(criticalPackets, segments.size());
            frame.coarseLevelIntact = std::none_of(segments.begin(), segments.begin() + count,
                                                   [](const Segment& segment) { return segment.lost; });
        }

        if (!walkRecordFrame(data, size, packets, coarseLevelKnown, layout, frame, error)) {
            return false;
        }
    }
    else {
        frame.framing = Framing::LengthPrefixed;
        if (packets.anyLost()) {
            // Packet lengths cannot be recovered once one is lost
            error = "packet loss in a length-prefixed frame";
            return false;
        }
        const uint32_t count = firstWord;
        if (count == 0 || count > (size - 4) / 8) {
            error = "invalid packet count " + std::to_string(count);
            return false;
        }

        size_t pos = 4;
        for (uint32_t i = 0; i < count; i++) {
            if (size - pos < 4) {
                error = "packet length runs past the end of the frame";
                return false;
            }
            const uint32_t packetSize = readU32(data + pos);
            pos += 4;
            if (packetSize == 0 || (packetSize % 4) != 0 || packetSize > size - pos) {
                error = "invalid packet length " + std::to_string(packetSize);
                return false;
            }
            if (!walkPacket(data, pos, packetSize, layout, frame, error)) {
                return false;
            }
            pos += packetSize;
        }

        if (pos != size) {
            error = "bytes after the last packet";
            return false;
        }
    }

    if (!frame.sequenceHeaderSeen) {
        error = "frame has no sequence header";
        return false;
    }
    // A partial frame is decoded if the decoder finds enough of it intact
    if (!frame.partial && frame.blockRecords != frame.announcedBlocks) {
        error = "frame carries " + std::to_string(frame.blockRecords) + " of " +
                std::to_string(frame.announcedBlocks) + " announced blocks";
        return false;
    }

    return true;
}

}
