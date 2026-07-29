#pragma once
#include <windows.h>

namespace moonlight_xbox_dx {

inline Platform::String^ BgColorToHex(Windows::UI::Color c)
{
    wchar_t buf[7];
    swprintf_s(buf, L"%02X%02X%02X", (unsigned)c.R, (unsigned)c.G, (unsigned)c.B);
    return ref new Platform::String(buf);
}

inline Windows::UI::Color BgHexToColor(Platform::String^ s, Windows::UI::Color fallback)
{
    if (s == nullptr || s->Length() != 6) return fallback;
    const wchar_t* p = s->Data();
    wchar_t buf[7]; wcsncpy_s(buf, p, 6); buf[6] = L'\0';
    wchar_t* end = nullptr;
    unsigned long v = wcstoul(buf, &end, 16);
    if (end != buf + 6) return fallback;
    Windows::UI::Color c;
    c.A = 255;
    c.R = (uint8_t)((v >> 16) & 0xFF);
    c.G = (uint8_t)((v >>  8) & 0xFF);
    c.B = (uint8_t)( v        & 0xFF);
    return c;
}

}
