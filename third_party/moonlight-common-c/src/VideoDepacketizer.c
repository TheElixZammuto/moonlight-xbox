#include "Limelight-internal.h"

static PLENTRY nalChainHead;
static PLENTRY nalChainTail;
static int nalChainDataLength;

static unsigned int nextFrameNumber;
static unsigned int startFrameNumber;
static bool waitingForNextSuccessfulFrame;
static bool waitingForIdrFrame;
static unsigned int lastPacketInStream;
static bool decodingFrame;
static bool strictIdrFrameWait;
static uint64_t firstPacketReceiveTime;
static unsigned int firstPacketPresentationTime;
static bool dropStatePending;
static bool idrFrameProcessed;

#define DR_CLEANUP -1000

#define CONSECUTIVE_DROP_LIMIT 120
static unsigned int consecutiveFrameDrops;

static LINKED_BLOCKING_QUEUE decodeUnitQueue;

typedef struct _BUFFER_DESC {
    char* data;
    unsigned int offset;
    unsigned int length;
} BUFFER_DESC, *PBUFFER_DESC;

typedef struct _LENTRY_INTERNAL {
    LENTRY entry;
    void* allocPtr;
} LENTRY_INTERNAL, *PLENTRY_INTERNAL;

#define H264_NAL_TYPE(x) ((x) & 0x1F)
#define HEVC_NAL_TYPE(x) (((x) & 0x7E) >> 1)

#define H264_NAL_TYPE_SPS 7
#define H264_NAL_TYPE_PPS 8
#define H264_NAL_TYPE_AUD 9
#define HEVC_NAL_TYPE_VPS 32
#define HEVC_NAL_TYPE_SPS 33
#define HEVC_NAL_TYPE_PPS 34
#define HEVC_NAL_TYPE_AUD 35

// Init
void initializeVideoDepacketizer(int pktSize) {
    LbqInitializeLinkedBlockingQueue(&decodeUnitQueue, 15);

    nextFrameNumber = 1;
    startFrameNumber = 0;
    waitingForNextSuccessfulFrame = false;
    waitingForIdrFrame = true;
    lastPacketInStream = UINT32_MAX;
    decodingFrame = false;
    firstPacketReceiveTime = 0;
    firstPacketPresentationTime = 0;
    dropStatePending = false;
    idrFrameProcessed = false;
    strictIdrFrameWait = !isReferenceFrameInvalidationEnabled();
}

// Free the NAL chain
static void cleanupFrameState(void) {
    PLENTRY_INTERNAL lastEntry;

    while (nalChainHead != NULL) {
        lastEntry = (PLENTRY_INTERNAL)nalChainHead;
        nalChainHead = lastEntry->entry.next;
        free(lastEntry->allocPtr);
    }

    nalChainTail = NULL;

    nalChainDataLength = 0;
}

// Cleanup frame state and set that we're waiting for an IDR Frame
static void dropFrameState(void) {
    // This may only be called at frame boundaries
    LC_ASSERT(!decodingFrame);

    // We're dropping frame state now
    dropStatePending = false;

    // We'll need an IDR frame now if we're in strict mode
    // or if we've never seen one before
    if (strictIdrFrameWait || !idrFrameProcessed) {
        waitingForIdrFrame = true;
    }

    // Count the number of consecutive frames dropped
    consecutiveFrameDrops++;

    // If we reach our limit, immediately request an IDR frame and reset
    if (consecutiveFrameDrops == CONSECUTIVE_DROP_LIMIT) {
        Limelog("Reached consecutive drop limit\n");

        // Restart the count
        consecutiveFrameDrops = 0;

        // Request an IDR frame
        waitingForIdrFrame = true;
        requestIdrOnDemand();
    }

    cleanupFrameState();
}

// Cleanup the list of decode units
static void freeDecodeUnitList(PLINKED_BLOCKING_QUEUE_ENTRY entry) {
    PLINKED_BLOCKING_QUEUE_ENTRY nextEntry;

    while (entry != NULL) {
        nextEntry = entry->flink;

        // Complete this with a failure status
        LiCompleteVideoFrame(entry->data, DR_CLEANUP);

        entry = nextEntry;
    }
}

void stopVideoDepacketizer(void) {
    LbqSignalQueueShutdown(&decodeUnitQueue);
}

// Cleanup video depacketizer and free malloced memory
void destroyVideoDepacketizer(void) {
    freeDecodeUnitList(LbqDestroyLinkedBlockingQueue(&decodeUnitQueue));
    cleanupFrameState();
}

static bool isSeqFrameStart(PBUFFER_DESC candidate) {
    return (candidate->length == 4 && candidate->data[candidate->offset + candidate->length - 1] == 1);
}

static bool isSeqAnnexBStart(PBUFFER_DESC candidate) {
    return (candidate->data[candidate->offset + candidate->length - 1] == 1);
}

static bool isSeqPadding(PBUFFER_DESC candidate) {
    return (candidate->data[candidate->offset + candidate->length - 1] == 0);
}

static bool getSpecialSeq(PBUFFER_DESC current, PBUFFER_DESC candidate) {
    if (current->length < 3) {
        return false;
    }

    if (current->data[current->offset] == 0 &&
        current->data[current->offset + 1] == 0) {
        // Padding or frame start
        if (current->data[current->offset + 2] == 0) {
            if (current->length >= 4 && current->data[current->offset + 3] == 1) {
                // Frame start
                candidate->data = current->data;
                candidate->offset = current->offset;
                candidate->length = 4;
                return true;
            }
            else {
                // Padding
                candidate->data = current->data;
                candidate->offset = current->offset;
                candidate->length = 3;
                return true;
            }
        }
        else if (current->data[current->offset + 2] == 1) {
            // NAL start
            candidate->data = current->data;
            candidate->offset = current->offset;
            candidate->length = 3;
            return true;
        }
    }

    return false;
}

bool LiWaitForNextVideoFrame(VIDEO_FRAME_HANDLE* frameHandle, PDECODE_UNIT* decodeUnit) {
    PQUEUED_DECODE_UNIT qdu;

    int err = LbqWaitForQueueElement(&decodeUnitQueue, (void**)&qdu);
    if (err != LBQ_SUCCESS) {
        return false;
    }

    *frameHandle = qdu;
    *decodeUnit = &qdu->decodeUnit;
    return true;
}

bool LiPollNextVideoFrame(VIDEO_FRAME_HANDLE* frameHandle, PDECODE_UNIT* decodeUnit) {
    PQUEUED_DECODE_UNIT qdu;

    int err = LbqPollQueueElement(&decodeUnitQueue, (void**)&qdu);
    if (err != LBQ_SUCCESS) {
        return false;
    }

    *frameHandle = qdu;
    *decodeUnit = &qdu->decodeUnit;
    return true;
}

bool LiPeekNextVideoFrame(PDECODE_UNIT* decodeUnit) {
    PQUEUED_DECODE_UNIT qdu;

    int err = LbqPeekQueueElement(&decodeUnitQueue, (void**)&qdu);
    if (err != LBQ_SUCCESS) {
        return false;
    }

    *decodeUnit = &qdu->decodeUnit;
    return true;
}

// Cleanup a decode unit by freeing the buffer chain and the holder
void LiCompleteVideoFrame(VIDEO_FRAME_HANDLE handle, int drStatus) {
    PQUEUED_DECODE_UNIT qdu = handle;
    PLENTRY_INTERNAL lastEntry;

    if (qdu->decodeUnit.frameType == FRAME_TYPE_IDR) {
        notifyKeyFrameReceived();
    }

    if (drStatus == DR_NEED_IDR) {
        Limelog("Requesting IDR frame on behalf of DR\n");
        requestDecoderRefresh();
    }
    else if (drStatus == DR_OK && qdu->decodeUnit.frameType == FRAME_TYPE_IDR) {
        // Remember that the IDR frame was processed. We can now use
        // reference frame invalidation.
        idrFrameProcessed = true;
    }

    while (qdu->decodeUnit.bufferList != NULL) {
        lastEntry = (PLENTRY_INTERNAL)qdu->decodeUnit.bufferList;
        qdu->decodeUnit.bufferList = lastEntry->entry.next;
        free(lastEntry->allocPtr);
    }

    // We will have stack-allocated entries iff we have a direct-submit decoder
    if ((VideoCallbacks.capabilities & CAPABILITY_DIRECT_SUBMIT) == 0) {
        free(qdu);
    }
}

static bool isSeqReferenceFrameStart(PBUFFER_DESC specialSeq) {
    if (NegotiatedVideoFormat & VIDEO_FORMAT_MASK_H264) {
        return H264_NAL_TYPE(specialSeq->data[specialSeq->offset + specialSeq->length]) == 5;
    }
    else if (NegotiatedVideoFormat & VIDEO_FORMAT_MASK_H265) {
        switch (HEVC_NAL_TYPE(specialSeq->data[specialSeq->offset + specialSeq->length])) {
            case 16:
            case 17:
            case 18:
            case 19:
            case 20:
            case 21:
                return true;

            default:
                return false;
        }
    }
    else {
        LC_ASSERT(false);
        return false;
    }
}

static bool isAccessUnitDelimiter(PBUFFER_DESC buffer) {
    BUFFER_DESC specialSeq;

    if (!getSpecialSeq(buffer, &specialSeq)) {
        return false;
    }

    if (NegotiatedVideoFormat & VIDEO_FORMAT_MASK_H264) {
        return H264_NAL_TYPE(specialSeq.data[specialSeq.offset + specialSeq.length]) == H264_NAL_TYPE_AUD;
    }
    else if (NegotiatedVideoFormat & VIDEO_FORMAT_MASK_H265) {
        return HEVC_NAL_TYPE(specialSeq.data[specialSeq.offset + specialSeq.length]) == HEVC_NAL_TYPE_AUD;
    }
    else {
        LC_ASSERT(false);
        return false;
    }
}

// Advance the buffer descriptor to the start of the next NAL
static void skipToNextNal(PBUFFER_DESC buffer) {
    BUFFER_DESC specialSeq;

    // If we're starting on a NAL boundary, skip to the next one
    if (getSpecialSeq(buffer, &specialSeq) && isSeqAnnexBStart(&specialSeq)) {
        buffer->offset += specialSeq.length;
        buffer->length -= specialSeq.length;
    }

    // Loop until we find an Annex B start sequence (3 or 4 byte)
    while (!getSpecialSeq(buffer, &specialSeq) || !isSeqAnnexBStart(&specialSeq)) {
        if (buffer->length == 0) {
            // If we skipped all the data, something has gone horribly wrong
            LC_ASSERT(buffer->length > 0);
            return;
        }

        buffer->offset++;
        buffer->length--;
    }
}

static bool isIdrFrameStart(PBUFFER_DESC buffer) {
    BUFFER_DESC specialSeq;

    if (!getSpecialSeq(buffer, &specialSeq) || !isSeqFrameStart(&specialSeq)) {
        return false;
    }

    if (NegotiatedVideoFormat & VIDEO_FORMAT_MASK_H264) {
        return H264_NAL_TYPE(specialSeq.data[specialSeq.offset + specialSeq.length]) == H264_NAL_TYPE_SPS;
    }
    else if (NegotiatedVideoFormat & VIDEO_FORMAT_MASK_H265) {
        return HEVC_NAL_TYPE(specialSeq.data[specialSeq.offset + specialSeq.length]) == HEVC_NAL_TYPE_VPS;
    }
    else {
        LC_ASSERT(false);
        return false;
    }
}

// Reassemble the frame with the given frame number
static void reassembleFrame(int frameNumber) {
    if (nalChainHead != NULL) {
        QUEUED_DECODE_UNIT qduDS;
        PQUEUED_DECODE_UNIT qdu;

        // Use a stack allocation if we won't be queuing this
        if ((VideoCallbacks.capabilities & CAPABILITY_DIRECT_SUBMIT) == 0) {
            qdu = (PQUEUED_DECODE_UNIT)malloc(sizeof(*qdu));
        }
        else {
            qdu = &qduDS;
        }

        if (qdu != NULL) {
            qdu->decodeUnit.bufferList = nalChainHead;
            qdu->decodeUnit.fullLength = nalChainDataLength;
            qdu->decodeUnit.frameNumber = frameNumber;
            qdu->decodeUnit.receiveTimeMs = firstPacketReceiveTime;
            qdu->decodeUnit.presentationTimeMs = firstPacketPresentationTime;
            qdu->decodeUnit.enqueueTimeMs = LiGetMillis();

            // IDR frames will have leading CSD buffers
            if (nalChainHead->bufferType != BUFFER_TYPE_PICDATA) {
                qdu->decodeUnit.frameType = FRAME_TYPE_IDR;
            }
            else {
                qdu->decodeUnit.frameType = FRAME_TYPE_PFRAME;
            }

            nalChainHead = nalChainTail = NULL;
            nalChainDataLength = 0;

            if ((VideoCallbacks.capabilities & CAPABILITY_DIRECT_SUBMIT) == 0) {
                if (LbqOfferQueueItem(&decodeUnitQueue, qdu, &qdu->entry) == LBQ_BOUND_EXCEEDED) {
                    Limelog("Video decode unit queue overflow\n");

                    // Clear frame state and wait for an IDR
                    nalChainHead = qdu->decodeUnit.bufferList;
                    nalChainDataLength = qdu->decodeUnit.fullLength;
                    dropFrameState();

                    // Free the DU
                    free(qdu);

                    // Flush the decode unit queue
                    freeDecodeUnitList(LbqFlushQueueItems(&decodeUnitQueue));

                    // FIXME: Get proper bounds to use reference frame invalidation
                    requestIdrOnDemand();
                    return;
                }
            }
            else {
                // Submit the frame to the decoder
                LiCompleteVideoFrame(qdu, VideoCallbacks.submitDecodeUnit(&qdu->decodeUnit));
            }

            // Notify the control connection
            connectionReceivedCompleteFrame(frameNumber);

            // Clear frame drops
            consecutiveFrameDrops = 0;
        }
    }
}

static int getBufferFlags(char* data, int length) {
    BUFFER_DESC buffer;
    BUFFER_DESC candidate;

    buffer.data = data;
    buffer.length = (unsigned int)length;
    buffer.offset = 0;

    if (!getSpecialSeq(&buffer, &candidate) || !isSeqFrameStart(&candidate)) {
        return BUFFER_TYPE_PICDATA;
    }

    if (NegotiatedVideoFormat & VIDEO_FORMAT_MASK_H264) {
        switch (H264_NAL_TYPE(candidate.data[candidate.offset + candidate.length])) {
        case H264_NAL_TYPE_SPS:
            return BUFFER_TYPE_SPS;

        case H264_NAL_TYPE_PPS:
            return BUFFER_TYPE_PPS;

        default:
            return BUFFER_TYPE_PICDATA;
        }
    }
    else if (NegotiatedVideoFormat & VIDEO_FORMAT_MASK_H265) {
        switch (HEVC_NAL_TYPE(candidate.data[candidate.offset + candidate.length])) {
            case HEVC_NAL_TYPE_SPS:
                return BUFFER_TYPE_SPS;

            case HEVC_NAL_TYPE_PPS:
                return BUFFER_TYPE_PPS;

            case HEVC_NAL_TYPE_VPS:
                return BUFFER_TYPE_VPS;

            default:
                return BUFFER_TYPE_PICDATA;
        }
    }
    else {
        LC_ASSERT(false);
        return BUFFER_TYPE_PICDATA;
    }
}

// As an optimization, we can cast the existing packet buffer to a PLENTRY and avoid
// a malloc() and a memcpy() of the packet data.
static void queueFragment(PLENTRY_INTERNAL* existingEntry, char* data, int offset, int length) {
    PLENTRY_INTERNAL entry;

    if (existingEntry == NULL || *existingEntry == NULL) {
        entry = (PLENTRY_INTERNAL)malloc(sizeof(*entry) + length);
    }
    else {
        entry = *existingEntry;
    }

    if (entry != NULL) {
        entry->entry.next = NULL;
        entry->entry.length = length;

        // If we had to allocate a new entry, we must copy the data. If not,
        // the data already resides within the LENTRY allocation.
        if (existingEntry == NULL || *existingEntry == NULL) {
            entry->allocPtr = entry;

            entry->entry.data = (char*)(entry + 1);
            memcpy(entry->entry.data, &data[offset], entry->entry.length);
        }
        else {
            entry->entry.data = &data[offset];

            // The caller should have already set this up for us
            LC_ASSERT(entry->allocPtr != NULL);

            // We now own the packet buffer and will manage freeing it
            *existingEntry = NULL;
        }

        entry->entry.bufferType = getBufferFlags(entry->entry.data, entry->entry.length);

        nalChainDataLength += entry->entry.length;

        if (nalChainTail == NULL) {
            LC_ASSERT(nalChainHead == NULL);
            nalChainHead = nalChainTail = (PLENTRY)entry;
        }
        else {
            LC_ASSERT(nalChainHead != NULL);
            nalChainTail->next = (PLENTRY)entry;
            nalChainTail = nalChainTail->next;
        }
    }
}

// Process an RTP Payload using the slow path that handles multiple NALUs per packet
static void processRtpPayloadSlow(PBUFFER_DESC currentPos, PLENTRY_INTERNAL* existingEntry) {
    BUFFER_DESC specialSeq;
    bool decodingVideo = false;

    // We should not have any NALUs when processing the first packet in an IDR frame
    LC_ASSERT(nalChainHead == NULL);
    LC_ASSERT(nalChainTail == NULL);

    while (currentPos->length != 0) {
        int start = currentPos->offset;
        bool containsPicData = false;

        if (getSpecialSeq(currentPos, &specialSeq)) {
            if (isSeqAnnexBStart(&specialSeq)) {
                // Now we're decoding video
                decodingVideo = true;

                if (isSeqFrameStart(&specialSeq)) {
                    // Now we're working on a frame
                    decodingFrame = true;

                    if (isSeqReferenceFrameStart(&specialSeq)) {
                        // No longer waiting for an IDR frame
                        waitingForIdrFrame = false;
                        
                        // Cancel any pending IDR frame request
                        waitingForNextSuccessfulFrame = false;

                        // Use the cached LENTRY for this NALU since it will be
                        // the bulk of the data in this packet.
                        containsPicData = true;
                    }
                }

                // Skip the start sequence
                currentPos->length -= specialSeq.length;
                currentPos->offset += specialSeq.length;
            }
            else {
                // Not decoding video
                decodingVideo = false;

                // Just skip this byte
                currentPos->length--;
                currentPos->offset++;
            }
        }

        // Move to the next special sequence
        while (currentPos->length != 0) {
            // Check if this should end the current NAL
            if (getSpecialSeq(currentPos, &specialSeq)) {
                if (decodingVideo || !isSeqPadding(&specialSeq)) {
                    break;
                }
            }

            // This byte is part of the NAL data
            currentPos->offset++;
            currentPos->length--;
        }

        if (decodingVideo) {
            // To minimize copies, we'll use allocate for SPS, PPS, and VPS to allow
            // us to reuse the packet buffer for the picture data in the I-frame.
            queueFragment(containsPicData ? existingEntry : NULL,
                          currentPos->data, start, currentPos->offset - start);
        }
    }
}

// Dumps the decode unit queue and ensures the next frame submitted to the decoder will be
// an IDR frame
void requestDecoderRefresh(void) {
    // Wait for the next IDR frame
    waitingForIdrFrame = true;
    
    // Flush the decode unit queue
    freeDecodeUnitList(LbqFlushQueueItems(&decodeUnitQueue));
    
    // Request the receive thread drop its state
    // on the next call. We can't do it here because
    // it may be trying to queue DUs and we'll nuke
    // the state out from under it.
    dropStatePending = true;
    
    // Request the IDR frame
    requestIdrOnDemand();
}

// Return 1 if packet is the first one in the frame
static int isFirstPacket(uint8_t flags, uint8_t fecBlockNumber) {
    // Clear the picture data flag
    flags &= ~FLAG_CONTAINS_PIC_DATA;

    // Check if it's just the start or both start and end of a frame
    return (flags == (FLAG_SOF | FLAG_EOF) || flags == FLAG_SOF) && fecBlockNumber == 0;
}

// Process an RTP Payload
// The caller will free *existingEntry unless we NULL it
static void processRtpPayload(PNV_VIDEO_PACKET videoPacket, int length,
                       uint64_t receiveTimeMs, unsigned int presentationTimeMs,
                       PLENTRY_INTERNAL* existingEntry) {
    BUFFER_DESC currentPos;
    uint32_t frameIndex;
    uint8_t flags;
    uint32_t firstPacket;
    uint32_t streamPacketIndex;
    uint8_t fecCurrentBlockNumber;
    uint8_t fecLastBlockNumber;

    // Mask the top 8 bits from the SPI
    videoPacket->streamPacketIndex >>= 8;
    videoPacket->streamPacketIndex &= 0xFFFFFF;

    currentPos.data = (char*)(videoPacket + 1);
    currentPos.offset = 0;
    currentPos.length = length - sizeof(*videoPacket);

    fecCurrentBlockNumber = (videoPacket->multiFecBlocks >> 4) & 0x3;
    fecLastBlockNumber = (videoPacket->multiFecBlocks >> 6) & 0x3;
    frameIndex = videoPacket->frameIndex;
    flags = videoPacket->flags;
    firstPacket = isFirstPacket(flags, fecCurrentBlockNumber);

    LC_ASSERT((flags & ~(FLAG_SOF | FLAG_EOF | FLAG_CONTAINS_PIC_DATA)) == 0);

    streamPacketIndex = videoPacket->streamPacketIndex;
    
    // Drop packets from a previously corrupt frame
    if (isBefore32(frameIndex, nextFrameNumber)) {
        return;
    }

    // The FEC queue can sometimes recover corrupt frames (see comments in RtpFecQueue).
    // It almost always detects them before they get to us, but in case it doesn't
    // the streamPacketIndex not matching correctly should find nearly all of the rest.
    if (isBefore24(streamPacketIndex, U24(lastPacketInStream + 1)) ||
            (!(flags & FLAG_SOF) && streamPacketIndex != U24(lastPacketInStream + 1))) {
        Limelog("Depacketizer detected corrupt frame: %d", frameIndex);
        decodingFrame = false;
        nextFrameNumber = frameIndex + 1;
        waitingForNextSuccessfulFrame = true;
        dropFrameState();
        return;
    }
    
    // Verify that we didn't receive an incomplete frame
    LC_ASSERT(firstPacket ^ decodingFrame);
    
    // Check sequencing of this frame to ensure we didn't
    // miss one in between
    if (firstPacket) {
        // Make sure this is the next consecutive frame
        if (isBefore32(nextFrameNumber, frameIndex)) {
            if (nextFrameNumber + 1 == frameIndex) {
                Limelog("Network dropped 1 frame (frame %d)\n", frameIndex - 1);
            }
            else {
                Limelog("Network dropped %d frames (frames %d to %d)\n",
                        frameIndex - nextFrameNumber,
                        nextFrameNumber,
                        frameIndex - 1);
            }

            nextFrameNumber = frameIndex;

            // Wait until next complete frame
            waitingForNextSuccessfulFrame = true;
            dropFrameState();
        }
        else {
            LC_ASSERT(nextFrameNumber == frameIndex);
        }

        // We're now decoding a frame
        decodingFrame = true;
        firstPacketReceiveTime = receiveTimeMs;
        firstPacketPresentationTime = presentationTimeMs;
    }

    lastPacketInStream = streamPacketIndex;

    // If this is the first packet, skip the frame header (if one exists)
    if (firstPacket) {
        if (APP_VERSION_AT_LEAST(7, 1, 415)) {
            // >= 7.1.415
            // The first IDR frame now has smaller headers than the rest. We seem to be able to tell
            // them apart by looking at the first byte. It will be 0x81 for the long header and 0x01
            // for the short header.
            // TODO: This algorithm seems to actually work on GFE 3.18 (first byte always 0x01), so
            // maybe we could unify this codepath in the future.
            if (currentPos.data[0] == 0x01) {
                currentPos.offset += 8;
                currentPos.length -= 8;
            }
            else {
                LC_ASSERT(currentPos.data[0] == (char)0x81);
                currentPos.offset += 24;
                currentPos.length -= 24;
            }
        }
        else if (APP_VERSION_AT_LEAST(7, 1, 350)) {
            // [7.1.350, 7.1.415) should use the 8 byte header again
            currentPos.offset += 8;
            currentPos.length -= 8;
        }
        else if (APP_VERSION_AT_LEAST(7, 1, 320)) {
            // [7.1.320, 7.1.350) should use the 12 byte frame header
            currentPos.offset += 12;
            currentPos.length -= 12;
        }
        else if (APP_VERSION_AT_LEAST(5, 0, 0)) {
            // [5.x, 7.1.320) should use the 8 byte header
            currentPos.offset += 8;
            currentPos.length -= 8;
        }
        else {
            // Other versions don't have a frame header at all
        }

        // Assert that the frame start NALU prefix is next
        LC_ASSERT(currentPos.data[currentPos.offset + 0] == 0);
        LC_ASSERT(currentPos.data[currentPos.offset + 1] == 0);
        LC_ASSERT(currentPos.data[currentPos.offset + 2] == 0);
        LC_ASSERT(currentPos.data[currentPos.offset + 3] == 1);

        // If an AUD NAL is prepended to this frame data, remove it.
        // Other parts of this code are not prepared to deal with a
        // NAL of that type, so stripping it is the easiest option.
        if (isAccessUnitDelimiter(&currentPos)) {
            skipToNextNal(&currentPos);
        }
    }

    if (firstPacket && isIdrFrameStart(&currentPos))
    {
        // SPS and PPS prefix is padded between NALs, so we must decode it with the slow path
        processRtpPayloadSlow(&currentPos, existingEntry);
    }
    else
    {
        queueFragment(existingEntry, currentPos.data, currentPos.offset, currentPos.length);
    }

    if ((flags & FLAG_EOF) && fecCurrentBlockNumber == fecLastBlockNumber) {
        // Move on to the next frame
        decodingFrame = false;
        nextFrameNumber = frameIndex + 1;

        // If waiting for next successful frame and we got here
        // with an end flag, we can send a message to the server
        if (waitingForNextSuccessfulFrame) {
            // This is the next successful frame after a loss event
            connectionDetectedFrameLoss(startFrameNumber, frameIndex - 1);
            waitingForNextSuccessfulFrame = false;
        }

        // If we need an IDR frame first, then drop this frame
        if (waitingForIdrFrame) {
            Limelog("Waiting for IDR frame\n");

            dropFrameState();
            return;
        }

        // Carry out any pending state drops. We can't just do this
        // arbitrarily in the middle of processing a frame because
        // may cause the depacketizer state to become corrupted. For
        // example, if we drop state after the first packet, the
        // depacketizer will next try to process a non-SOF packet,
        // and cause it to assert.
        if (dropStatePending) {
            if (nalChainHead && nalChainHead->bufferType != BUFFER_TYPE_PICDATA) {
                // Don't drop the frame state if this frame is an IDR frame itself,
                // otherwise we'll lose this IDR frame without another in flight
                // and have to wait until we hit our consecutive drop limit to
                // request a new one (potentially several seconds).
                dropStatePending = false;
            }
            else {
                dropFrameState();
                return;
            }
        }

        reassembleFrame(frameIndex);

        startFrameNumber = nextFrameNumber;
    }
}

// Add an RTP Packet to the queue
void queueRtpPacket(PRTPV_QUEUE_ENTRY queueEntryPtr) {
    int dataOffset;
    RTPV_QUEUE_ENTRY queueEntry = *queueEntryPtr;

    LC_ASSERT(!queueEntry.isParity);
    LC_ASSERT(queueEntry.receiveTimeMs != 0);

    dataOffset = sizeof(*queueEntry.packet);
    if (queueEntry.packet->header & FLAG_EXTENSION) {
        dataOffset += 4; // 2 additional fields
    }

    // Reuse the memory reserved for the RTPFEC_QUEUE_ENTRY to store the LENTRY_INTERNAL
    // now that we're in the depacketizer. We saved a copy of the real FEC queue entry
    // on the stack here so we can safely modify this memory in place.
    LC_ASSERT(sizeof(LENTRY_INTERNAL) <= sizeof(RTPV_QUEUE_ENTRY));
    PLENTRY_INTERNAL existingEntry = (PLENTRY_INTERNAL)queueEntryPtr;
    existingEntry->allocPtr = queueEntry.packet;

    processRtpPayload((PNV_VIDEO_PACKET)(((char*)queueEntry.packet) + dataOffset),
                      queueEntry.length - dataOffset,
                      queueEntry.receiveTimeMs,
                      queueEntry.presentationTimeMs,
                      &existingEntry);

    if (existingEntry != NULL) {
        // processRtpPayload didn't want this packet, so just free it
        free(existingEntry->allocPtr);
    }
}

int LiGetPendingVideoFrames(void) {
    return LbqGetItemCount(&decodeUnitQueue);
}
