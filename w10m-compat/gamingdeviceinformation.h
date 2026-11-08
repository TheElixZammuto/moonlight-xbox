/*
 * W10M compat stub for <gamingdeviceinformation.h>.
 *
 * The real header (SDK 19041) declares GetGamingDeviceModelInformation which
 * is backed by api-ms-win-gaming-deviceinformation-l1-1-0.dll - an API set
 * that does not exist on Windows 10 Mobile 15254. Importing it would kill the
 * process at load time. On a phone the answer is always "not an Xbox", so this
 * stub returns exactly that, with no external API dependency.
 */
#if !defined(_MSC_VER) || _MSC_VER >= 1920
#error "w10m-compat/gamingdeviceinformation.h is only for MSVC v141 (19.16) and earlier"
#endif

#pragma once

#include <windows.h>

#ifndef _GAMING_DEVICE_MODEL_INFORMATION_DEFINED
#define _GAMING_DEVICE_MODEL_INFORMATION_DEFINED

typedef enum GAMING_DEVICE_VENDOR_ID
{
    GAMING_DEVICE_VENDOR_ID_NONE      = 0,
    GAMING_DEVICE_VENDOR_ID_MICROSOFT = 1,
    GAMING_DEVICE_VENDOR_ID_UNKNOWN   = 0xFFFFFFFF
} GAMING_DEVICE_VENDOR_ID;

typedef enum GAMING_DEVICE_DEVICE_ID
{
    GAMING_DEVICE_DEVICE_ID_NONE                   = 0,
    GAMING_DEVICE_DEVICE_ID_XBOX_ONE               = 0x768BAE26,
    GAMING_DEVICE_DEVICE_ID_XBOX_ONE_S             = 0x2A7361D9,
    GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X             = 0x5AD617C7,
    GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X_DEVKIT      = 0x10F7CDE3,
    /* Series S/X values as in SDK 20348; never reported here (W10M). */
    GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_S          = 0x1A2B3C4D,
    GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_S_DEVKIT   = 0x1A2B3C4E,
    GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X          = 0x1A2B3C4F,
    GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X_DEVKIT   = 0x1A2B3C50
} GAMING_DEVICE_DEVICE_ID;

typedef struct GAMING_DEVICE_MODEL_INFORMATION
{
    GAMING_DEVICE_VENDOR_ID vendorId;
    GAMING_DEVICE_DEVICE_ID deviceId;
} GAMING_DEVICE_MODEL_INFORMATION;

#ifndef __cplusplus
#define INLINE_STUB static
#else
#define INLINE_STUB inline
#endif

INLINE_STUB HRESULT WINAPI GetGamingDeviceModelInformation(GAMING_DEVICE_MODEL_INFORMATION* info)
{
    if (!info)
        return E_POINTER;
    info->vendorId = GAMING_DEVICE_VENDOR_ID_NONE;
    info->deviceId = GAMING_DEVICE_DEVICE_ID_NONE;
    return S_OK;
}

#endif /* _GAMING_DEVICE_MODEL_INFORMATION_DEFINED */
