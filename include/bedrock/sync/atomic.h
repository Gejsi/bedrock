#ifndef BEDROCK_SYNC_ATOMIC_H
#define BEDROCK_SYNC_ATOMIC_H

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 201112L)
#error "Bedrock sync/atomic currently requires a C11-or-newer compiler."
#endif

#if defined(__STDC_NO_ATOMICS__)
#error "Bedrock sync/atomic requires C11 atomics; __STDC_NO_ATOMICS__ is defined."
#endif

#include <stdatomic.h>

#include <bedrock/base.h>

BR_EXTERN_C_BEGIN

/*
Bedrock's atomic surface is built on C11 atomics.

Compare-exchange follows C's expected-pointer contract (not a tuple return).
That is the clean portable choice in C without introducing a large typed wrapper
layer.

This header is an abstraction boundary, not a portability guarantee: the
current backend requires a compiler/target with usable C11 atomics.
*/

typedef memory_order br_atomic_memory_order;

enum {
  BR_ATOMIC_RELAXED = memory_order_relaxed,
  BR_ATOMIC_CONSUME = memory_order_consume,
  BR_ATOMIC_ACQUIRE = memory_order_acquire,
  BR_ATOMIC_RELEASE = memory_order_release,
  BR_ATOMIC_ACQ_REL = memory_order_acq_rel,
  BR_ATOMIC_SEQ_CST = memory_order_seq_cst,
};

#define BR_ATOMIC_INIT(value) ATOMIC_VAR_INIT(value)
#define br_atomic(T) _Atomic(T)

typedef br_atomic(bool) br_atomic_bool;
typedef br_atomic(int8_t) br_atomic_i8;
typedef br_atomic(int16_t) br_atomic_i16;
typedef br_atomic(int32_t) br_atomic_i32;
typedef br_atomic(int64_t) br_atomic_i64;
typedef br_atomic(uint8_t) br_atomic_u8;
typedef br_atomic(uint16_t) br_atomic_u16;
typedef br_atomic(uint32_t) br_atomic_u32;
typedef br_atomic(uint64_t) br_atomic_u64;
typedef br_atomic(ptrdiff_t) br_atomic_isize;
typedef br_atomic(size_t) br_atomic_usize;
typedef br_atomic(intptr_t) br_atomic_iptr;
typedef br_atomic(uintptr_t) br_atomic_uptr;

/*
Hint to the CPU that the current thread is in a short spin loop.
*/
void br_cpu_relax(void);

#define br_atomic_thread_fence(order) atomic_thread_fence((br_atomic_memory_order)(order))
#define br_atomic_signal_fence(order) atomic_signal_fence((br_atomic_memory_order)(order))

#define br_atomic_init(dst, value) atomic_init((dst), (value))

#define br_atomic_store(dst, value) atomic_store((dst), (value))
#define br_atomic_store_explicit(dst, value, order)                                                \
  atomic_store_explicit((dst), (value), (br_atomic_memory_order)(order))

#define br_atomic_load(dst) atomic_load((dst))
#define br_atomic_load_explicit(dst, order)                                                        \
  atomic_load_explicit((dst), (br_atomic_memory_order)(order))

#define br_atomic_add(dst, value) atomic_fetch_add((dst), (value))
#define br_atomic_add_explicit(dst, value, order)                                                  \
  atomic_fetch_add_explicit((dst), (value), (br_atomic_memory_order)(order))

#define br_atomic_sub(dst, value) atomic_fetch_sub((dst), (value))
#define br_atomic_sub_explicit(dst, value, order)                                                  \
  atomic_fetch_sub_explicit((dst), (value), (br_atomic_memory_order)(order))

#define br_atomic_and(dst, value) atomic_fetch_and((dst), (value))
#define br_atomic_and_explicit(dst, value, order)                                                  \
  atomic_fetch_and_explicit((dst), (value), (br_atomic_memory_order)(order))

#define br_atomic_or(dst, value) atomic_fetch_or((dst), (value))
#define br_atomic_or_explicit(dst, value, order)                                                   \
  atomic_fetch_or_explicit((dst), (value), (br_atomic_memory_order)(order))

#define br_atomic_xor(dst, value) atomic_fetch_xor((dst), (value))
#define br_atomic_xor_explicit(dst, value, order)                                                  \
  atomic_fetch_xor_explicit((dst), (value), (br_atomic_memory_order)(order))

#define br_atomic_exchange(dst, value) atomic_exchange((dst), (value))
#define br_atomic_exchange_explicit(dst, value, order)                                             \
  atomic_exchange_explicit((dst), (value), (br_atomic_memory_order)(order))

/*
Compare-exchange follows the C11 contract: `expected` points to the expected
old value, is updated on failure, and the return value reports success.
*/
#define br_atomic_compare_exchange_strong(dst, expected, desired)                                  \
  atomic_compare_exchange_strong((dst), (expected), (desired))

#define br_atomic_compare_exchange_strong_explicit(dst, expected, desired, success, failure)       \
  atomic_compare_exchange_strong_explicit((dst),                                                   \
                                          (expected),                                              \
                                          (desired),                                               \
                                          (br_atomic_memory_order)(success),                       \
                                          (br_atomic_memory_order)(failure))

#define br_atomic_compare_exchange_weak(dst, expected, desired)                                    \
  atomic_compare_exchange_weak((dst), (expected), (desired))

#define br_atomic_compare_exchange_weak_explicit(dst, expected, desired, success, failure)         \
  atomic_compare_exchange_weak_explicit((dst),                                                     \
                                        (expected),                                                \
                                        (desired),                                                 \
                                        (br_atomic_memory_order)(success),                         \
                                        (br_atomic_memory_order)(failure))

BR_EXTERN_C_END

#endif
