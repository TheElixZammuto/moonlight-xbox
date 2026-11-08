/*
 * stdalign.h for MSVC under /std:c11|c17 (UCRT builds before 10.0.22621 do
 * not ship it). MSVC 19.4x implements _Alignas/_Alignof as keywords, so the
 * standard macros are trivial mappings.
 * aligned_alloc is intentionally NOT provided here - it belongs to stdlib.h
 * and ffmpeg detects its absence via configure.
 */
#if !defined(_MSC_VER) || _MSC_VER >= 1920
#error "w10m-compat/stdalign.h is only for MSVC v141 (19.16) and earlier"
#endif

#ifndef _STDALIGN_H
#define _STDALIGN_H

#define alignas _Alignas
#define alignof _Alignof
#define __alignas_is_defined 1
#define __alignof_is_defined 1

#endif /* _STDALIGN_H */
