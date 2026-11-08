/*
 * Minimal C11 <stdatomic.h> shim for MSVC v141 (19.16) and earlier.
 *
 * v141 has no C11 atomics header and no _Atomic/__c11_atomic_* builtins.
 * The project only uses 64-bit counters and one 32-bit flag, so map the
 * required operations onto the ARM interlocked intrinsics that v141 does
 * provide.  Memory orders are mapped to the closest _nf/_acq/_rel variants;
 * seq_cst/acq_rel use the full-barrier form.
 *
 * Only include this header for v141 or earlier.  v142/v143 have their own
 * <stdatomic.h> and must use that instead.
 */
#if !defined(_MSC_VER) || _MSC_VER >= 1920
#error "w10m-compat/stdatomic.h is only for MSVC v141 (19.16) and earlier"
#endif

#pragma once
#include <intrin.h>

/* v141 C mode has no C11 _Static_assert keyword. */
#ifndef _Static_assert
#define _Static_assert(expr, msg) \
    typedef char w10m_static_assert_##__LINE__[(expr) ? 1 : -1]
#endif

/* 64-bit interlocked operations require 8-byte alignment on non-x86. */
typedef __declspec(align(8)) struct {
    volatile __int64 value;
} atomic_ullong;

typedef volatile long atomic_flag;

typedef enum memory_order {
    memory_order_relaxed = 0,
    memory_order_consume = 1,
    memory_order_acquire = 2,
    memory_order_release = 3,
    memory_order_acq_rel = 4,
    memory_order_seq_cst = 5
} memory_order;

#define ATOMIC_FLAG_INIT 0

static __forceinline unsigned long long
w10m_atomic_load_ull(volatile __int64* p, memory_order order)
{
    switch (order) {
    case memory_order_relaxed:
        return (unsigned long long)_InterlockedCompareExchange64_nf(p, 0, 0);
    case memory_order_consume:
    case memory_order_acquire:
        return (unsigned long long)_InterlockedCompareExchange64_acq(p, 0, 0);
    default:
        return (unsigned long long)_InterlockedCompareExchange64(p, 0, 0);
    }
}

static __forceinline void
w10m_atomic_store_ull(volatile __int64* p, unsigned long long value, memory_order order)
{
    switch (order) {
    case memory_order_relaxed:
        (void)_InterlockedExchange64_nf(p, (__int64)value);
        break;
    case memory_order_release:
        (void)_InterlockedExchange64_rel(p, (__int64)value);
        break;
    default:
        (void)_InterlockedExchange64(p, (__int64)value);
        break;
    }
}

static __forceinline unsigned long long
w10m_atomic_fetch_add_ull(volatile __int64* p, unsigned long long value, memory_order order)
{
    switch (order) {
    case memory_order_relaxed:
        return (unsigned long long)_InterlockedExchangeAdd64_nf(p, (__int64)value);
    case memory_order_consume:
    case memory_order_acquire:
        return (unsigned long long)_InterlockedExchangeAdd64_acq(p, (__int64)value);
    case memory_order_release:
        return (unsigned long long)_InterlockedExchangeAdd64_rel(p, (__int64)value);
    default:
        return (unsigned long long)_InterlockedExchangeAdd64(p, (__int64)value);
    }
}

static __forceinline int
w10m_atomic_flag_test_and_set(volatile long* p, memory_order order)
{
    switch (order) {
    case memory_order_relaxed:
        return _InterlockedExchange_nf(p, 1) != 0;
    case memory_order_consume:
    case memory_order_acquire:
        return _InterlockedExchange_acq(p, 1) != 0;
    default:
        return _InterlockedExchange(p, 1) != 0;
    }
}

static __forceinline void
w10m_atomic_flag_clear(volatile long* p, memory_order order)
{
    switch (order) {
    case memory_order_relaxed:
        (void)_InterlockedExchange_nf(p, 0);
        break;
    case memory_order_release:
        (void)_InterlockedExchange_rel(p, 0);
        break;
    default:
        (void)_InterlockedExchange(p, 0);
        break;
    }
}

#define atomic_init(p, v) (((atomic_ullong*)(p))->value = (__int64)(v))

#define atomic_load(p) \
    w10m_atomic_load_ull((volatile __int64*)(p), memory_order_seq_cst)
#define atomic_load_explicit(p, o) \
    w10m_atomic_load_ull((volatile __int64*)(p), (o))

#define atomic_store(p, v) \
    w10m_atomic_store_ull((volatile __int64*)(p), (v), memory_order_seq_cst)
#define atomic_store_explicit(p, v, o) \
    w10m_atomic_store_ull((volatile __int64*)(p), (v), (o))

#define atomic_fetch_add(p, v) \
    w10m_atomic_fetch_add_ull((volatile __int64*)(p), (v), memory_order_seq_cst)
#define atomic_fetch_add_explicit(p, v, o) \
    w10m_atomic_fetch_add_ull((volatile __int64*)(p), (v), (o))

#define atomic_flag_test_and_set(p) \
    w10m_atomic_flag_test_and_set((volatile long*)(p), memory_order_seq_cst)
#define atomic_flag_test_and_set_explicit(p, o) \
    w10m_atomic_flag_test_and_set((volatile long*)(p), (o))

#define atomic_flag_clear(p) \
    w10m_atomic_flag_clear((volatile long*)(p), memory_order_seq_cst)
#define atomic_flag_clear_explicit(p, o) \
    w10m_atomic_flag_clear((volatile long*)(p), (o))
