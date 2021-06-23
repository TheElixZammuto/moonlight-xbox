#include "Limelight-internal.h"

static SOCKET inputSock = INVALID_SOCKET;
static unsigned char currentAesIv[16];
static bool initialized;
static PPLT_CRYPTO_CONTEXT cryptoContext;

static LINKED_BLOCKING_QUEUE packetQueue;
static LINKED_BLOCKING_QUEUE packetHolderFreeList;
static PLT_THREAD inputSendThread;

#define MAX_INPUT_PACKET_SIZE 128
#define INPUT_STREAM_TIMEOUT_SEC 10

#define MAX_QUEUED_INPUT_PACKETS 150

// Contains input stream packets
typedef struct _PACKET_HOLDER {
    int packetLength;
    union {
        NV_KEYBOARD_PACKET keyboard;
        NV_REL_MOUSE_MOVE_PACKET mouseMoveRel;
        NV_ABS_MOUSE_MOVE_PACKET mouseMoveAbs;
        NV_MOUSE_BUTTON_PACKET mouseButton;
        NV_CONTROLLER_PACKET controller;
        NV_MULTI_CONTROLLER_PACKET multiController;
        NV_SCROLL_PACKET scroll;
        NV_HAPTICS_PACKET haptics;
    } packet;
    LINKED_BLOCKING_QUEUE_ENTRY entry;
} PACKET_HOLDER, *PPACKET_HOLDER;

// Initializes the input stream
int initializeInputStream(void) {
    memcpy(currentAesIv, StreamConfig.remoteInputAesIv, sizeof(currentAesIv));
    
    // Set a high maximum queue size limit to ensure input isn't dropped
    // while the input send thread is blocked for short periods.
    LbqInitializeLinkedBlockingQueue(&packetQueue, MAX_QUEUED_INPUT_PACKETS);
    LbqInitializeLinkedBlockingQueue(&packetHolderFreeList, MAX_QUEUED_INPUT_PACKETS);

    cryptoContext = PltCreateCryptoContext();
    return 0;
}

// Destroys and cleans up the input stream
void destroyInputStream(void) {
    PLINKED_BLOCKING_QUEUE_ENTRY entry, nextEntry;
    
    PltDestroyCryptoContext(cryptoContext);

    entry = LbqDestroyLinkedBlockingQueue(&packetQueue);

    while (entry != NULL) {
        nextEntry = entry->flink;

        // The entry is stored in the data buffer
        free(entry->data);

        entry = nextEntry;
    }

    entry = LbqDestroyLinkedBlockingQueue(&packetHolderFreeList);

    while (entry != NULL) {
        nextEntry = entry->flink;

        // The entry is stored in the data buffer
        free(entry->data);

        entry = nextEntry;
    }
}

static int encryptData(unsigned char* plaintext, int plaintextLen,
                       unsigned char* ciphertext, int* ciphertextLen) {
    // Starting in Gen 7, AES GCM is used for encryption
    if (AppVersionQuad[0] >= 7) {
        if (!PltEncryptMessage(cryptoContext, ALGORITHM_AES_GCM, 0,
                               (unsigned char*)StreamConfig.remoteInputAesKey, sizeof(StreamConfig.remoteInputAesKey),
                               currentAesIv, sizeof(currentAesIv),
                               ciphertext, 16,
                               plaintext, plaintextLen,
                               &ciphertext[16], ciphertextLen)) {
            return -1;
        }

        // Increment the ciphertextLen to account for the tag
        *ciphertextLen += 16;
        return 0;
    }
    else {
        // PKCS7 padding may need to be added in-place, so we must copy this into a buffer
        // that can safely be modified.
        unsigned char paddedData[ROUND_TO_PKCS7_PADDED_LEN(MAX_INPUT_PACKET_SIZE)];

        memcpy(paddedData, plaintext, plaintextLen);

        // Prior to Gen 7, 128-bit AES CBC is used for encryption with each message padded
        // to the block size to ensure messages are not delayed within the cipher.
        return PltEncryptMessage(cryptoContext, ALGORITHM_AES_CBC, CIPHER_FLAG_PAD_TO_BLOCK_SIZE,
                                 (unsigned char*)StreamConfig.remoteInputAesKey, sizeof(StreamConfig.remoteInputAesKey),
                                 currentAesIv, sizeof(currentAesIv),
                                 NULL, 0,
                                 paddedData, plaintextLen,
                                 ciphertext, ciphertextLen) ? 0 : -1;
    }
}

static void freePacketHolder(PPACKET_HOLDER holder) {
    // Place the packet holder back into the free list
    if (LbqOfferQueueItem(&packetHolderFreeList, holder, &holder->entry) != LBQ_SUCCESS) {
        free(holder);
    }
}

static PPACKET_HOLDER allocatePacketHolder(void) {
    PPACKET_HOLDER holder;
    int err;

    // Grab an entry from the free list (if available)
    err = LbqPollQueueElement(&packetHolderFreeList, (void**)&holder);
    if (err == LBQ_SUCCESS) {
        return holder;
    }
    else if (err == LBQ_INTERRUPTED) {
        // We're shutting down. Don't bother allocating.
        return NULL;
    }
    else {
        LC_ASSERT(err == LBQ_NO_ELEMENT);

        // Otherwise we'll have to allocate
        return malloc(sizeof(*holder));
    }
}

// Input thread proc
static void inputSendThreadProc(void* context) {
    SOCK_RET err;
    PPACKET_HOLDER holder;
    char encryptedBuffer[MAX_INPUT_PACKET_SIZE];
    uint32_t encryptedSize;
    bool encryptedControlStream = APP_VERSION_AT_LEAST(7, 1, 431);

    while (!PltIsThreadInterrupted(&inputSendThread)) {
        int encryptedLengthPrefix;

        err = LbqWaitForQueueElement(&packetQueue, (void**)&holder);
        if (err != LBQ_SUCCESS) {
            return;
        }

        // If it's a multi-controller packet we can do batching
        if (holder->packet.multiController.header.packetType == BE32(PACKET_TYPE_MULTI_CONTROLLER)) {
            PPACKET_HOLDER controllerBatchHolder;
            PNV_MULTI_CONTROLLER_PACKET origPkt;

            origPkt = &holder->packet.multiController;
            for (;;) {
                PNV_MULTI_CONTROLLER_PACKET newPkt;

                // Peek at the next packet
                if (LbqPeekQueueElement(&packetQueue, (void**)&controllerBatchHolder) != LBQ_SUCCESS) {
                    break;
                }

                // If it's not a controller packet, we're done
                if (controllerBatchHolder->packet.multiController.header.packetType != BE32(PACKET_TYPE_MULTI_CONTROLLER)) {
                    break;
                }

                // Check if it's able to be batched
                // NB: GFE does some discarding of gamepad packets received very soon after another.
                // Thus, this batching is needed for correctness in some cases, as GFE will inexplicably
                // drop *newer* packets in that scenario. The brokenness can be tested with consecutive
                // calls to LiSendMultiControllerEvent() with different values for analog sticks (max -> zero).
                newPkt = &controllerBatchHolder->packet.multiController;
                if (newPkt->buttonFlags != origPkt->buttonFlags ||
                    newPkt->controllerNumber != origPkt->controllerNumber ||
                    newPkt->activeGamepadMask != origPkt->activeGamepadMask) {
                    // Batching not allowed
                    break;
                }

                // Remove the batchable controller packet
                if (LbqPollQueueElement(&packetQueue, (void**)&controllerBatchHolder) != LBQ_SUCCESS) {
                    break;
                }

                // Update the original packet
                origPkt->leftTrigger = newPkt->leftTrigger;
                origPkt->rightTrigger = newPkt->rightTrigger;
                origPkt->leftStickX = newPkt->leftStickX;
                origPkt->leftStickY = newPkt->leftStickY;
                origPkt->rightStickX = newPkt->rightStickX;
                origPkt->rightStickY = newPkt->rightStickY;

                // Free the batched packet holder
                freePacketHolder(controllerBatchHolder);
            }
        }
        // If it's a relative mouse move packet, we can also do batching
        else if (holder->packet.mouseMoveRel.header.packetType == BE32(PACKET_TYPE_REL_MOUSE_MOVE)) {
            PPACKET_HOLDER mouseBatchHolder;
            int totalDeltaX = (short)BE16(holder->packet.mouseMoveRel.deltaX);
            int totalDeltaY = (short)BE16(holder->packet.mouseMoveRel.deltaY);

            for (;;) {
                int partialDeltaX;
                int partialDeltaY;

                // Peek at the next packet
                if (LbqPeekQueueElement(&packetQueue, (void**)&mouseBatchHolder) != LBQ_SUCCESS) {
                    break;
                }

                // If it's not a mouse move packet, we're done
                if (mouseBatchHolder->packet.mouseMoveRel.header.packetType != BE32(PACKET_TYPE_REL_MOUSE_MOVE)) {
                    break;
                }

                partialDeltaX = (short)BE16(mouseBatchHolder->packet.mouseMoveRel.deltaX);
                partialDeltaY = (short)BE16(mouseBatchHolder->packet.mouseMoveRel.deltaY);

                // Check for overflow
                if (partialDeltaX + totalDeltaX > INT16_MAX ||
                    partialDeltaX + totalDeltaX < INT16_MIN ||
                    partialDeltaY + totalDeltaY > INT16_MAX ||
                    partialDeltaY + totalDeltaY < INT16_MIN) {
                    // Total delta would overflow our 16-bit short
                    break;
                }

                // Remove the batchable mouse move packet
                if (LbqPollQueueElement(&packetQueue, (void**)&mouseBatchHolder) != LBQ_SUCCESS) {
                    break;
                }

                totalDeltaX += partialDeltaX;
                totalDeltaY += partialDeltaY;

                // Free the batched packet holder
                freePacketHolder(mouseBatchHolder);
            }

            // Update the original packet
            holder->packet.mouseMoveRel.deltaX = BE16((short)totalDeltaX);
            holder->packet.mouseMoveRel.deltaY = BE16((short)totalDeltaY);
        }
        // If it's an absolute mouse move packet, we should only send the latest
        else if (holder->packet.mouseMoveAbs.header.packetType == BE32(PACKET_TYPE_ABS_MOUSE_MOVE)) {
            for (;;) {
                PPACKET_HOLDER mouseBatchHolder;

                // Peek at the next packet
                if (LbqPeekQueueElement(&packetQueue, (void**)&mouseBatchHolder) != LBQ_SUCCESS) {
                    break;
                }

                // If it's not a mouse position packet, we're done
                if (mouseBatchHolder->packet.mouseMoveAbs.header.packetType != BE32(PACKET_TYPE_ABS_MOUSE_MOVE)) {
                    break;
                }

                // Remove the mouse position packet
                if (LbqPollQueueElement(&packetQueue, (void**)&mouseBatchHolder) != LBQ_SUCCESS) {
                    break;
                }

                // Replace the current packet with the new one
                freePacketHolder(holder);
                holder = mouseBatchHolder;
            }
        }

        // On GFE 3.22, the entire control stream is encrypted (and support for separate RI encrypted)
        // has been removed. We send the plaintext packet through and the control stream code will do
        // the encryption.
        if (encryptedControlStream) {
            err = (SOCK_RET)sendInputPacketOnControlStream((unsigned char*)&holder->packet, holder->packetLength);
            freePacketHolder(holder);
            if (err < 0) {
                Limelog("Input: sendInputPacketOnControlStream() failed: %d\n", (int) err);
                ListenerCallbacks.connectionTerminated(err);
                return;
            }
        }
        else {
            // Encrypt the message into the output buffer while leaving room for the length
            encryptedSize = sizeof(encryptedBuffer) - 4;
            err = encryptData((unsigned char*)&holder->packet, holder->packetLength,
                (unsigned char*)&encryptedBuffer[4], (int*)&encryptedSize);
            freePacketHolder(holder);
            if (err != 0) {
                Limelog("Input: Encryption failed: %d\n", (int)err);
                ListenerCallbacks.connectionTerminated(err);
                return;
            }

            // Prepend the length to the message
            encryptedLengthPrefix = BE32(encryptedSize);
            memcpy(&encryptedBuffer[0], &encryptedLengthPrefix, 4);

            if (AppVersionQuad[0] < 5) {
                // Send the encrypted payload
                err = send(inputSock, (const char*) encryptedBuffer,
                    (int) (encryptedSize + sizeof(encryptedLengthPrefix)), 0);
                if (err <= 0) {
                    Limelog("Input: send() failed: %d\n", (int) LastSocketError());
                    ListenerCallbacks.connectionTerminated(LastSocketFail());
                    return;
                }
            }
            else {
                // For reasons that I can't understand, NVIDIA decides to use the last 16
                // bytes of ciphertext in the most recent game controller packet as the IV for
                // future encryption. I think it may be a buffer overrun on their end but we'll have
                // to mimic it to work correctly.
                if (AppVersionQuad[0] >= 7 && encryptedSize >= 16 + sizeof(currentAesIv)) {
                    memcpy(currentAesIv,
                           &encryptedBuffer[4 + encryptedSize - sizeof(currentAesIv)],
                           sizeof(currentAesIv));
                }

                err = (SOCK_RET)sendInputPacketOnControlStream((unsigned char*) encryptedBuffer,
                    (int) (encryptedSize + sizeof(encryptedLengthPrefix)));
                if (err < 0) {
                    Limelog("Input: sendInputPacketOnControlStream() failed: %d\n", (int) err);
                    ListenerCallbacks.connectionTerminated(err);
                    return;
                }
            }
        }
    }
}

// This function tells GFE that we support haptics and it should send rumble events to us
static int sendEnableHaptics(void) {
    PPACKET_HOLDER holder;
    int err;

    // Avoid sending this on earlier server versions, since they may terminate
    // the connection upon receiving an unexpected packet.
    if (AppVersionQuad[0] < 7 || (AppVersionQuad[0] == 7 && AppVersionQuad[1] < 1)) {
        return 0;
    }

    holder = allocatePacketHolder();
    if (holder == NULL) {
        return -1;
    }

    holder->packetLength = sizeof(NV_HAPTICS_PACKET);
    holder->packet.haptics.header.packetType = BE32(PACKET_TYPE_HAPTICS);
    holder->packet.haptics.magicA = LE32(H_MAGIC_A);
    holder->packet.haptics.magicB = LE32(H_MAGIC_B);

    err = LbqOfferQueueItem(&packetQueue, holder, &holder->entry);
    if (err != LBQ_SUCCESS) {
        LC_ASSERT(err == LBQ_BOUND_EXCEEDED);
        Limelog("Input queue reached maximum size limit\n");
        freePacketHolder(holder);
    }

    return err;
}

// Begin the input stream
int startInputStream(void) {
    int err;

    // After Gen 5, we send input on the control stream
    if (AppVersionQuad[0] < 5) {
        inputSock = connectTcpSocket(&RemoteAddr, RemoteAddrLen,
            35043, INPUT_STREAM_TIMEOUT_SEC);
        if (inputSock == INVALID_SOCKET) {
            return LastSocketFail();
        }

        enableNoDelay(inputSock);
    }

    err = PltCreateThread("InputSend", inputSendThreadProc, NULL, &inputSendThread);
    if (err != 0) {
        if (inputSock != INVALID_SOCKET) {
            closeSocket(inputSock);
            inputSock = INVALID_SOCKET;
        }
        return err;
    }

    // Allow input packets to be queued now
    initialized = true;

    // GFE will not send haptics events without this magic packet first
    sendEnableHaptics();

    return err;
}

// Stops the input stream
int stopInputStream(void) {
    // No more packets should be queued now
    initialized = false;
    LbqSignalQueueShutdown(&packetHolderFreeList);

    // Signal the input send thread to drain all pending
    // input packets before shutting down.
    LbqSignalQueueDrain(&packetQueue);
    PltJoinThread(&inputSendThread);
    PltCloseThread(&inputSendThread);

    if (inputSock != INVALID_SOCKET) {
        shutdownTcpSocket(inputSock);
    }
    
    if (inputSock != INVALID_SOCKET) {
        closeSocket(inputSock);
        inputSock = INVALID_SOCKET;
    }

    return 0;
}

// Send a mouse move event to the streaming machine
int LiSendMouseMoveEvent(short deltaX, short deltaY) {
    PPACKET_HOLDER holder;
    int err;

    if (!initialized) {
        return -2;
    }

    if (deltaX == 0 && deltaY == 0) {
        return 0;
    }

    holder = allocatePacketHolder();
    if (holder == NULL) {
        return -1;
    }

    holder->packetLength = sizeof(NV_REL_MOUSE_MOVE_PACKET);
    holder->packet.mouseMoveRel.header.packetType = BE32(PACKET_TYPE_REL_MOUSE_MOVE);
    holder->packet.mouseMoveRel.magic = MOUSE_MOVE_REL_MAGIC;
    // On Gen 5 servers, the header code is incremented by one
    if (AppVersionQuad[0] >= 5) {
        holder->packet.mouseMoveRel.magic++;
    }
    holder->packet.mouseMoveRel.magic = LE32(holder->packet.mouseMoveRel.magic);
    holder->packet.mouseMoveRel.deltaX = BE16(deltaX);
    holder->packet.mouseMoveRel.deltaY = BE16(deltaY);

    err = LbqOfferQueueItem(&packetQueue, holder, &holder->entry);
    if (err != LBQ_SUCCESS) {
        LC_ASSERT(err == LBQ_BOUND_EXCEEDED);
        Limelog("Input queue reached maximum size limit\n");
        freePacketHolder(holder);
    }

    return err;
}

// Send a mouse position update to the streaming machine
int LiSendMousePositionEvent(short x, short y, short referenceWidth, short referenceHeight) {
    PPACKET_HOLDER holder;
    int err;

    if (!initialized) {
        return -2;
    }

    holder = allocatePacketHolder();
    if (holder == NULL) {
        return -1;
    }

    holder->packetLength = sizeof(NV_ABS_MOUSE_MOVE_PACKET);
    holder->packet.mouseMoveAbs.header.packetType = BE32(PACKET_TYPE_ABS_MOUSE_MOVE);
    holder->packet.mouseMoveAbs.magic = LE32(MOUSE_MOVE_ABS_MAGIC);
    holder->packet.mouseMoveAbs.x = BE16(x);
    holder->packet.mouseMoveAbs.y = BE16(y);
    holder->packet.mouseMoveAbs.unused = 0;

    // There appears to be a rounding error in GFE's scaling calculation which prevents
    // the cursor from reaching the far edge of the screen when streaming at smaller
    // resolutions with a higher desktop resolution (like streaming 720p with a desktop
    // resolution of 1080p, or streaming 720p/1080p with a desktop resolution of 4K).
    // Subtracting one from the reference dimensions seems to work around this issue.
    holder->packet.mouseMoveAbs.width = BE16(referenceWidth - 1);
    holder->packet.mouseMoveAbs.height = BE16(referenceHeight - 1);

    err = LbqOfferQueueItem(&packetQueue, holder, &holder->entry);
    if (err != LBQ_SUCCESS) {
        LC_ASSERT(err == LBQ_BOUND_EXCEEDED);
        Limelog("Input queue reached maximum size limit\n");
        freePacketHolder(holder);
    }

    return err;
}

// Send a mouse button event to the streaming machine
int LiSendMouseButtonEvent(char action, int button) {
    PPACKET_HOLDER holder;
    int err;

    if (!initialized) {
        return -2;
    }

    holder = allocatePacketHolder();
    if (holder == NULL) {
        return -1;
    }

    holder->packetLength = sizeof(NV_MOUSE_BUTTON_PACKET);
    holder->packet.mouseButton.header.packetType = BE32(PACKET_TYPE_MOUSE_BUTTON);
    holder->packet.mouseButton.action = action;
    if (AppVersionQuad[0] >= 5) {
        holder->packet.mouseButton.action++;
    }
    holder->packet.mouseButton.button = BE32(button);

    err = LbqOfferQueueItem(&packetQueue, holder, &holder->entry);
    if (err != LBQ_SUCCESS) {
        LC_ASSERT(err == LBQ_BOUND_EXCEEDED);
        Limelog("Input queue reached maximum size limit\n");
        freePacketHolder(holder);
    }

    return err;
}

// Send a key press event to the streaming machine
int LiSendKeyboardEvent(short keyCode, char keyAction, char modifiers) {
    PPACKET_HOLDER holder;
    int err;

    if (!initialized) {
        return -2;
    }

    holder = allocatePacketHolder();
    if (holder == NULL) {
        return -1;
    }

    // For proper behavior, the MODIFIER flag must not be set on the modifier key down event itself
    // for the extended modifiers on the right side of the keyboard. If the MODIFIER flag is set,
    // GFE will synthesize an errant key down event for the non-extended key, causing that key to be
    // stuck down after the extended modifier key is raised. For non-extended keys, we must set the
    // MODIFIER flag for correct behavior.
    switch (keyCode & 0xFF) {
    case 0x5B: // VK_LWIN
    case 0x5C: // VK_RWIN
        // Any keyboard event with the META modifier flag is dropped by all known GFE versions.
        // This prevents us from sending shortcuts involving the meta key (Win+X, Win+Tab, etc).
        // The catch is that the meta key event itself would actually work if it didn't set its
        // own modifier flag, so we'll clear that here. This should be safe even if a new GFE
        // release comes out that stops dropping events with MODIFIER_META flag.
        modifiers &= ~MODIFIER_META;
        break;

    case 0xA0: // VK_LSHIFT
        modifiers |= MODIFIER_SHIFT;
        break;
    case 0xA1: // VK_RSHIFT
        modifiers &= ~MODIFIER_SHIFT;
        break;

    case 0xA2: // VK_LCONTROL
        modifiers |= MODIFIER_CTRL;
        break;
    case 0xA3: // VK_RCONTROL
        modifiers &= ~MODIFIER_CTRL;
        break;

    case 0xA4: // VK_LMENU
        modifiers |= MODIFIER_ALT;
        break;
    case 0xA5: // VK_RMENU
        modifiers &= ~MODIFIER_ALT;
        break;

    default:
        // No fixups
        break;
    }

    holder->packetLength = sizeof(NV_KEYBOARD_PACKET);
    holder->packet.keyboard.header.packetType = BE32(PACKET_TYPE_KEYBOARD);
    holder->packet.keyboard.keyAction = keyAction;
    holder->packet.keyboard.zero1 = 0;
    holder->packet.keyboard.keyCode = LE16(keyCode);
    holder->packet.keyboard.modifiers = modifiers;
    holder->packet.keyboard.zero2 = 0;

    err = LbqOfferQueueItem(&packetQueue, holder, &holder->entry);
    if (err != LBQ_SUCCESS) {
        LC_ASSERT(err == LBQ_BOUND_EXCEEDED);
        Limelog("Input queue reached maximum size limit\n");
        freePacketHolder(holder);
    }

    return err;
}

static int sendControllerEventInternal(short controllerNumber, short activeGamepadMask,
    short buttonFlags, unsigned char leftTrigger, unsigned char rightTrigger,
    short leftStickX, short leftStickY, short rightStickX, short rightStickY)
{
    PPACKET_HOLDER holder;
    int err;

    if (!initialized) {
        return -2;
    }

    holder = allocatePacketHolder();
    if (holder == NULL) {
        return -1;
    }

    if (AppVersionQuad[0] == 3) {
        // Generation 3 servers don't support multiple controllers so we send
        // the legacy packet
        holder->packetLength = sizeof(NV_CONTROLLER_PACKET);
        holder->packet.controller.header.packetType = BE32(PACKET_TYPE_CONTROLLER);
        holder->packet.controller.headerA = LE32(C_HEADER_A);
        holder->packet.controller.headerB = LE16(C_HEADER_B);
        holder->packet.controller.buttonFlags = LE16(buttonFlags);
        holder->packet.controller.leftTrigger = leftTrigger;
        holder->packet.controller.rightTrigger = rightTrigger;
        holder->packet.controller.leftStickX = LE16(leftStickX);
        holder->packet.controller.leftStickY = LE16(leftStickY);
        holder->packet.controller.rightStickX = LE16(rightStickX);
        holder->packet.controller.rightStickY = LE16(rightStickY);
        holder->packet.controller.tailA = LE32(C_TAIL_A);
        holder->packet.controller.tailB = LE16(C_TAIL_B);
    }
    else {
        // Generation 4+ servers support passing the controller number
        holder->packetLength = sizeof(NV_MULTI_CONTROLLER_PACKET);
        holder->packet.multiController.header.packetType = BE32(PACKET_TYPE_MULTI_CONTROLLER);
        holder->packet.multiController.headerA = MC_HEADER_A;
        // On Gen 5 servers, the header code is decremented by one
        if (AppVersionQuad[0] >= 5) {
            holder->packet.multiController.headerA--;
        }
        holder->packet.multiController.headerA = LE32(holder->packet.multiController.headerA);
        holder->packet.multiController.headerB = LE16(MC_HEADER_B);
        holder->packet.multiController.controllerNumber = LE16(controllerNumber);
        holder->packet.multiController.activeGamepadMask = LE16(activeGamepadMask);
        holder->packet.multiController.midB = LE16(MC_MID_B);
        holder->packet.multiController.buttonFlags = LE16(buttonFlags);
        holder->packet.multiController.leftTrigger = leftTrigger;
        holder->packet.multiController.rightTrigger = rightTrigger;
        holder->packet.multiController.leftStickX = LE16(leftStickX);
        holder->packet.multiController.leftStickY = LE16(leftStickY);
        holder->packet.multiController.rightStickX = LE16(rightStickX);
        holder->packet.multiController.rightStickY = LE16(rightStickY);
        holder->packet.multiController.tailA = LE32(MC_TAIL_A);
        holder->packet.multiController.tailB = LE16(MC_TAIL_B);
    }

    err = LbqOfferQueueItem(&packetQueue, holder, &holder->entry);
    if (err != LBQ_SUCCESS) {
        LC_ASSERT(err == LBQ_BOUND_EXCEEDED);
        Limelog("Input queue reached maximum size limit\n");
        freePacketHolder(holder);
    }

    return err;
}

// Send a controller event to the streaming machine
int LiSendControllerEvent(short buttonFlags, unsigned char leftTrigger, unsigned char rightTrigger,
    short leftStickX, short leftStickY, short rightStickX, short rightStickY)
{
    return sendControllerEventInternal(0, 0x1, buttonFlags, leftTrigger, rightTrigger,
        leftStickX, leftStickY, rightStickX, rightStickY);
}

// Send a controller event to the streaming machine
int LiSendMultiControllerEvent(short controllerNumber, short activeGamepadMask,
    short buttonFlags, unsigned char leftTrigger, unsigned char rightTrigger,
    short leftStickX, short leftStickY, short rightStickX, short rightStickY)
{
    return sendControllerEventInternal(controllerNumber, activeGamepadMask,
        buttonFlags, leftTrigger, rightTrigger,
        leftStickX, leftStickY, rightStickX, rightStickY);
}

// Send a high resolution scroll event to the streaming machine
int LiSendHighResScrollEvent(short scrollAmount) {
    PPACKET_HOLDER holder;
    int err;

    if (!initialized) {
        return -2;
    }

    if (scrollAmount == 0) {
        return 0;
    }

    holder = allocatePacketHolder();
    if (holder == NULL) {
        return -1;
    }

    holder->packetLength = sizeof(NV_SCROLL_PACKET);
    holder->packet.scroll.header.packetType = BE32(PACKET_TYPE_SCROLL);
    holder->packet.scroll.magicA = MAGIC_A;
    // On Gen 5 servers, the header code is incremented by one
    if (AppVersionQuad[0] >= 5) {
        holder->packet.scroll.magicA++;
    }
    holder->packet.scroll.zero1 = 0;
    holder->packet.scroll.zero2 = 0;
    holder->packet.scroll.scrollAmt1 = BE16(scrollAmount);
    holder->packet.scroll.scrollAmt2 = holder->packet.scroll.scrollAmt1;
    holder->packet.scroll.zero3 = 0;

    err = LbqOfferQueueItem(&packetQueue, holder, &holder->entry);
    if (err != LBQ_SUCCESS) {
        LC_ASSERT(err == LBQ_BOUND_EXCEEDED);
        Limelog("Input queue reached maximum size limit\n");
        freePacketHolder(holder);
    }

    return err;
}

// Send a scroll event to the streaming machine
int LiSendScrollEvent(signed char scrollClicks) {
    return LiSendHighResScrollEvent(scrollClicks * 120);
}
