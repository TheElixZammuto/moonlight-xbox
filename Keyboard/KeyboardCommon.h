//https://github.com/reactos/reactos/blob/master/sdk/include/ndk/kbd.h
#pragma once
#include "pch.h"
#define VK_LBUTTON	1
#define VK_RBUTTON	2
#define VK_CANCEL	3
#define VK_MBUTTON	4
#define VK_XBUTTON1	5
#define VK_XBUTTON2	6
#define VK_BACK	8
#define VK_TAB	9
#define VK_CLEAR	12
#define VK_RETURN	13
#define VK_SHIFT	16
#define VK_CONTROL	17
#define VK_MENU	18
#define VK_PAUSE	19
#define VK_CAPITAL	20
#define VK_KANA	0x15
#define VK_HANGEUL	0x15
#define VK_HANGUL	0x15
#define VK_JUNJA	0x17
#define VK_FINAL	0x18
#define VK_HANJA	0x19
#define VK_KANJI	0x19
#define VK_ESCAPE	0x1B
#define VK_CONVERT	0x1C
#define VK_NONCONVERT	0x1D
#define VK_ACCEPT	0x1E
#define VK_MODECHANGE	0x1F
#define VK_SPACE	32
#define VK_PRIOR	33
#define VK_NEXT	34
#define VK_END	35
#define VK_HOME	36
#define VK_LEFT	37
#define VK_UP	38
#define VK_RIGHT	39
#define VK_DOWN	40
#define VK_SELECT	41
#define VK_PRINT	42
#define VK_EXECUTE	43
#define VK_SNAPSHOT	44
#define VK_INSERT	45
#define VK_DELETE	46
#define VK_HELP	47
#define VK_LWIN	0x5B
#define VK_RWIN	0x5C
#define VK_APPS	0x5D
#define VK_SLEEP	0x5F
#define VK_NUMPAD0	0x60
#define VK_NUMPAD1	0x61
#define VK_NUMPAD2	0x62
#define VK_NUMPAD3	0x63
#define VK_NUMPAD4	0x64
#define VK_NUMPAD5	0x65
#define VK_NUMPAD6	0x66
#define VK_NUMPAD7	0x67
#define VK_NUMPAD8	0x68
#define VK_NUMPAD9	0x69
#define VK_MULTIPLY	0x6A
#define VK_ADD	0x6B
#define VK_SEPARATOR	0x6C
#define VK_SUBTRACT	0x6D
#define VK_DECIMAL	0x6E
#define VK_DIVIDE	0x6F
#define VK_F1	0x70
#define VK_F2	0x71
#define VK_F3	0x72
#define VK_F4	0x73
#define VK_F5	0x74
#define VK_F6	0x75
#define VK_F7	0x76
#define VK_F8	0x77
#define VK_F9	0x78
#define VK_F10	0x79
#define VK_F11	0x7A
#define VK_F12	0x7B
#define VK_F13	0x7C
#define VK_F14	0x7D
#define VK_F15	0x7E
#define VK_F16	0x7F
#define VK_F17	0x80
#define VK_F18	0x81
#define VK_F19	0x82
#define VK_F20	0x83
#define VK_F21	0x84
#define VK_F22	0x85
#define VK_F23	0x86
#define VK_F24	0x87
#define VK_NUMLOCK	0x90
#define VK_SCROLL	0x91
#define VK_OEM_NEC_EQUAL	0x92
#define VK_LSHIFT	0xA0
#define VK_RSHIFT	0xA1
#define VK_LCONTROL	0xA2
#define VK_RCONTROL	0xA3
#define VK_LMENU	0xA4
#define VK_RMENU	0xA5
#define VK_BROWSER_BACK	0xA6
#define VK_BROWSER_FORWARD	0xA7
#define VK_BROWSER_REFRESH	0xA8
#define VK_BROWSER_STOP	0xA9
#define VK_BROWSER_SEARCH	0xAA
#define VK_BROWSER_FAVORITES	0xAB
#define VK_BROWSER_HOME	0xAC
#define VK_VOLUME_MUTE	0xAD
#define VK_VOLUME_DOWN	0xAE
#define VK_VOLUME_UP	0xAF
#define VK_MEDIA_NEXT_TRACK	0xB0
#define VK_MEDIA_PREV_TRACK	0xB1
#define VK_MEDIA_STOP	0xB2
#define VK_MEDIA_PLAY_PAUSE	0xB3
#define VK_LAUNCH_MAIL	0xB4
#define VK_LAUNCH_MEDIA_SELECT	0xB5
#define VK_LAUNCH_APP1	0xB6
#define VK_LAUNCH_APP2	0xB7
#define VK_OEM_1	0xBA
#define VK_OEM_PLUS	0xBB
#define VK_OEM_COMMA	0xBC
#define VK_OEM_MINUS	0xBD
#define VK_OEM_PERIOD	0xBE
#define VK_OEM_2	0xBF
#define VK_OEM_3	0xC0
#define VK_OEM_4	0xDB
#define VK_OEM_5	0xDC
#define VK_OEM_6	0xDD
#define VK_OEM_7	0xDE
#define VK_OEM_8	0xDF
#define VK_OEM_102	0xE2
#define VK_ICO_HELP	0xE3  /* Help key on ICO */
#define VK_ICO_00	0xE4  /* 00 key on ICO */
#define VK_PROCESSKEY	0xE5
#define VK_PACKET	0xE7
#define VK_OEM_RESET	0xE9
#define VK_OEM_JUMP	0xEA
#define VK_OEM_PA1	0xEB
#define VK_OEM_PA2	0xEC
#define VK_OEM_PA3	0xED
#define VK_OEM_WSCTRL	0xEE
#define VK_OEM_CUSEL	0xEF
#define VK_OEM_ATTN	0xF0
#define VK_OEM_FINISH	0xF1
#define VK_OEM_COPY	0xF2
#define VK_OEM_AUTO	0xF3
#define VK_OEM_ENLW	0xF4
#define VK_OEM_BACKTAB	0xF5
#define VK_ATTN	0xF6
#define VK_CRSEL	0xF7
#define VK_EXSEL	0xF8
#define VK_EREOF	0xF9
#define VK_PLAY	0xFA
#define VK_ZOOM	0xFB
#define VK_NONAME	0xFC
#define VK_PA1	0xFD
#define VK_OEM_CLEAR	0xFE

#define VK_EMPTY  0xff   /* The non-existent VK */

	/* Virtual key flags */
#define KBDEXT     0x100  /* Extended key code */
#define KBDMULTIVK 0x200  /* Multi-key */
#define KBDSPECIAL 0x400  /* Special key */
#define KBDNUMPAD  0x800  /* Number-pad */

/* Modifier bits */
#define KBDSHIFT   0x001  /* Shift modifier */
#define KBDCTRL    0x002  /* Ctrl modifier */
#define KBDALT     0x004  /* Alt modifier */

/* Invalid shift */
#define SHFT_INVALID 0x0F

typedef struct _VK_TO_BIT {
	BYTE Vk;
	BYTE ModBits;
} VK_TO_BIT, * PVK_TO_BIT;

typedef struct _MODIFIERS {
	PVK_TO_BIT pVkToBit;
	WORD wMaxModBits;
	BYTE ModNumber[];
} MODIFIERS, * PMODIFIERS;

#define TYPEDEF_VK_TO_WCHARS(i) \
  typedef struct _VK_TO_WCHARS ## i { \
    BYTE VirtualKey; \
    BYTE Attributes; \
    WCHAR wch[i]; \
  } VK_TO_WCHARS ## i, *PVK_TO_WCHARS ## i;

TYPEDEF_VK_TO_WCHARS(1)
TYPEDEF_VK_TO_WCHARS(2)
TYPEDEF_VK_TO_WCHARS(3)
TYPEDEF_VK_TO_WCHARS(4)
TYPEDEF_VK_TO_WCHARS(5)
TYPEDEF_VK_TO_WCHARS(6)
TYPEDEF_VK_TO_WCHARS(7)
TYPEDEF_VK_TO_WCHARS(8)
TYPEDEF_VK_TO_WCHARS(9)
TYPEDEF_VK_TO_WCHARS(10)

typedef struct _VK_TO_WCHAR_TABLE {
	PVK_TO_WCHARS1 pVkToWchars;
	BYTE nModifications;
	BYTE cbSize;
} VK_TO_WCHAR_TABLE, * PVK_TO_WCHAR_TABLE;

typedef struct _DEADKEY {
	DWORD dwBoth;
	WCHAR wchComposed;
	USHORT uFlags;
} DEADKEY, * PDEADKEY;

typedef WCHAR* DEADKEY_LPWSTR;

#define DKF_DEAD 1

typedef struct _VSC_LPWSTR {
	BYTE vsc;
	LPWSTR pwsz;
} VSC_LPWSTR, * PVSC_LPWSTR;

typedef struct _VSC_VK {
	BYTE Vsc;
	USHORT Vk;
} VSC_VK, * PVSC_VK;

#define TYPEDEF_LIGATURE(i) \
typedef struct _LIGATURE ## i { \
  BYTE VirtualKey; \
  WORD ModificationNumber; \
  WCHAR wch[i]; \
} LIGATURE ## i, *PLIGATURE ## i;

TYPEDEF_LIGATURE(1)
TYPEDEF_LIGATURE(2)
TYPEDEF_LIGATURE(3)
TYPEDEF_LIGATURE(4)
TYPEDEF_LIGATURE(5)

#define KBD_VERSION 1
#define GET_KBD_VERSION(p) (HIWORD((p)->fLocalFlags))
#define KLLF_ALTGR     0x1
#define KLLF_SHIFTLOCK 0x2
#define KLLF_LRM_RLM   0x4

typedef struct _KBDTABLES {
	PMODIFIERS pCharModifiers;
	PVK_TO_WCHAR_TABLE pVkToWcharTable;
	PDEADKEY pDeadKey;
	VSC_LPWSTR* pKeyNames;
	VSC_LPWSTR* pKeyNamesExt;
	LPWSTR* pKeyNamesDead;
	USHORT* pusVSCtoVK;
	BYTE bMaxVSCtoVK;
	PVSC_VK pVSCtoVK_E0;
	PVSC_VK pVSCtoVK_E1;
	DWORD fLocaleFlags;
	BYTE nLgMaxd;
	BYTE cbLgEntry;
	PLIGATURE1 pLigature;
} KBDTABLES, * PKBDTABLES;

/* Constants that help table decoding */
#define WCH_NONE  0xf000
#define WCH_DEAD  0xf001
#define WCH_LGTR  0xf002

/* VK_TO_WCHARS attributes */
#define CAPLOK       0x01
#define SGCAPS       0x02
#define CAPLOKALTGR  0x04
#define KANALOK      0x08
#define GRPSELTAP    0x80

#define VK_ABNT_C1  0xC1
#define VK_ABNT_C2  0xC2

/* Useful scancodes */
#define SCANCODE_LSHIFT  0x2A
#define SCANCODE_RSHIFT  0x36
#define SCANCODE_CTRL    0x1D
#define SCANCODE_ALT     0x38

namespace moonlight_xbox_dx {
	extern KBDTABLES a1Layout;
	extern KBDTABLES a2Layout;
	extern KBDTABLES a3Layout;
	extern KBDTABLES alLayout;
	extern KBDTABLES armeLayout;
	extern KBDTABLES armwLayout;
	extern KBDTABLES azeLayout;
	extern KBDTABLES azelLayout;
	extern KBDTABLES beLayout;
	extern KBDTABLES bgaLayout;
	extern KBDTABLES bgtLayout;
	extern KBDTABLES blrLayout;
	extern KBDTABLES brLayout;
	extern KBDTABLES buLayout;
	extern KBDTABLES burLayout;
	extern KBDTABLES canLayout;
	extern KBDTABLES crLayout;
	extern KBDTABLES czLayout;
	extern KBDTABLES cz1Layout;
	extern KBDTABLES daLayout;
	extern KBDTABLES dvLayout;
	extern KBDTABLES eoLayout;
	extern KBDTABLES estLayout;
	extern KBDTABLES fcLayout;
	extern KBDTABLES fiLayout;
	extern KBDTABLES frLayout;
	extern KBDTABLES geoLayout;
	extern KBDTABLES gergLayout;
	extern KBDTABLES gneoLayout;
	extern KBDTABLES grLayout;
	extern KBDTABLES gr1Layout;
	extern KBDTABLES heLayout;
	extern KBDTABLES hebLayout;
	extern KBDTABLES huLayout;
	extern KBDTABLES icLayout;
	extern KBDTABLES inasaLayout;
	extern KBDTABLES inbenLayout;
	extern KBDTABLES indevLayout;
	extern KBDTABLES ingujLayout;
	extern KBDTABLES inmalLayout;
	extern KBDTABLES irLayout;
	extern KBDTABLES itLayout;
	extern KBDTABLES jpnLayout;
	extern KBDTABLES kazLayout;
	extern KBDTABLES korLayout;
	extern KBDTABLES laLayout;
	extern KBDTABLES lt1Layout;
	extern KBDTABLES lvLayout;
	extern KBDTABLES macLayout;
	extern KBDTABLES neLayout;
	extern KBDTABLES noLayout;
	extern KBDTABLES plLayout;
	extern KBDTABLES pl1Layout;
	extern KBDTABLES poLayout;
	extern KBDTABLES roLayout;
	extern KBDTABLES rostLayout;
	extern KBDTABLES ruLayout;
	extern KBDTABLES ru1Layout;
	extern KBDTABLES sfLayout;
	extern KBDTABLES sgLayout;
	extern KBDTABLES slLayout;
	extern KBDTABLES sl1Layout;
	extern KBDTABLES spLayout;
	extern KBDTABLES swLayout;
	extern KBDTABLES tatLayout;
	extern KBDTABLES th0Layout;
	extern KBDTABLES th1Layout;
	extern KBDTABLES th2Layout;
	extern KBDTABLES th3Layout;
	extern KBDTABLES tufLayout;
	extern KBDTABLES tuqLayout;
	extern KBDTABLES ukLayout;
	extern KBDTABLES urLayout;
	extern KBDTABLES ursLayout;
	extern KBDTABLES usLayout;
	extern KBDTABLES usaLayout;
	extern KBDTABLES uslLayout;
	extern KBDTABLES usrLayout;
	extern KBDTABLES usxLayout;
	extern KBDTABLES uzbLayout;
	extern KBDTABLES vntcLayout;
	extern KBDTABLES yccLayout;
	extern KBDTABLES yclLayout;
	extern std::map<std::string, KBDTABLES> keyboardLayouts;
}

