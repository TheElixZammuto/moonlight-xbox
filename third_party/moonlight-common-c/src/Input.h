#pragma once

#pragma pack(push, 1)

typedef struct _NV_INPUT_HEADER {
    uint32_t packetType;
} NV_INPUT_HEADER, PNV_INPUT_HEADER;

#define PACKET_TYPE_HAPTICS 0x06
#define H_MAGIC_A 0x0000000D
#define H_MAGIC_B 0x00000001
typedef struct _NV_HAPTICS_PACKET {
    NV_INPUT_HEADER header;
    int magicA;
    int magicB;
} NV_HAPTICS_PACKET, *PNV_HAPTICS_PACKET;

#define PACKET_TYPE_KEYBOARD 0x0A
typedef struct _NV_KEYBOARD_PACKET {
    NV_INPUT_HEADER header;
    char keyAction;
    int zero1;
    short keyCode;
    char modifiers;
    short zero2;
} NV_KEYBOARD_PACKET, *PNV_KEYBOARD_PACKET;

#define PACKET_TYPE_REL_MOUSE_MOVE 0x08
#define MOUSE_MOVE_REL_MAGIC 0x06
typedef struct _NV_REL_MOUSE_MOVE_PACKET {
    NV_INPUT_HEADER header;
    int magic;
    short deltaX;
    short deltaY;
} NV_REL_MOUSE_MOVE_PACKET, *PNV_REL_MOUSE_MOVE_PACKET;

#define PACKET_TYPE_ABS_MOUSE_MOVE 0x0e
#define MOUSE_MOVE_ABS_MAGIC 0x05
typedef struct _NV_ABS_MOUSE_MOVE_PACKET {
    NV_INPUT_HEADER header;
    int magic;

    short x;
    short y;

    short unused;

    // Used on the server-side as a reference to scale x and y
    // to screen coordinates.
    short width;
    short height;
} NV_ABS_MOUSE_MOVE_PACKET, *PNV_ABS_MOUSE_MOVE_PACKET;

#define PACKET_TYPE_MOUSE_BUTTON 0x05
typedef struct _NV_MOUSE_BUTTON_PACKET {
    NV_INPUT_HEADER header;
    char action;
    int button;
} NV_MOUSE_BUTTON_PACKET, *PNV_MOUSE_BUTTON_PACKET;

#define PACKET_TYPE_CONTROLLER 0x18
#define C_HEADER_A 0x0000000A
#define C_HEADER_B 0x1400
#define C_TAIL_A 0x0000009C
#define C_TAIL_B 0x0055
typedef struct _NV_CONTROLLER_PACKET {
    NV_INPUT_HEADER header;
    int headerA;
    short headerB;
    short buttonFlags;
    unsigned char leftTrigger;
    unsigned char rightTrigger;
    short leftStickX;
    short leftStickY;
    short rightStickX;
    short rightStickY;
    int tailA;
    short tailB;
} NV_CONTROLLER_PACKET, *PNV_CONTROLLER_PACKET;

#define PACKET_TYPE_MULTI_CONTROLLER 0x1E
#define MC_HEADER_A 0x0000000D
#define MC_HEADER_B 0x001A
#define MC_MID_B 0x0014
#define MC_TAIL_A 0x0000009C
#define MC_TAIL_B 0x0055
typedef struct _NV_MULTI_CONTROLLER_PACKET {
    NV_INPUT_HEADER header;
    int headerA;
    short headerB;
    short controllerNumber;
    short activeGamepadMask;
    short midB;
    short buttonFlags;
    unsigned char leftTrigger;
    unsigned char rightTrigger;
    short leftStickX;
    short leftStickY;
    short rightStickX;
    short rightStickY;
    int tailA;
    short tailB;
} NV_MULTI_CONTROLLER_PACKET, *PNV_MULTI_CONTROLLER_PACKET;

#define PACKET_TYPE_SCROLL 0xA
#define MAGIC_A 0x09
typedef struct _NV_SCROLL_PACKET {
    NV_INPUT_HEADER header;
    char magicA;
    char zero1;
    short zero2;
    short scrollAmt1;
    short scrollAmt2;
    short zero3;
} NV_SCROLL_PACKET, *PNV_SCROLL_PACKET;

#pragma pack(pop)
