/*
 * Bedrock - single-header distribution
 *
 * GENERATED FILE - DO NOT EDIT.
 * Regenerate with `make dist` (tools/amalgamate.py) from the modular tree
 * under include/ and src/. Edits here are overwritten and will fail
 * `make check-dist`.
 *
 * Usage:
 *   #include "bedrock.h"                       // declarations
 *   #define BEDROCK_IMPLEMENTATION             // in exactly one .c
 *   #include "bedrock.h"
 * or compile the companion dist/bedrock.c, which does the above for you.
 */

#ifndef BEDROCK_SINGLE_H
#define BEDROCK_SINGLE_H

/* ======================================================
   Declarations (always visible)
   ====================================================== */

/* ==== bedrock.h ==== */
#ifndef BEDROCK_H
#define BEDROCK_H

/* ==== bedrock/base.h ==== */
#ifndef BEDROCK_BASE_H
#define BEDROCK_BASE_H

#include <string.h>

/* ==== bedrock/types.h ==== */
#ifndef BEDROCK_TYPES_H
#define BEDROCK_TYPES_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef BEDROCK_NO_SHORT_TYPES
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef intptr_t iptr;
typedef uintptr_t uptr;

typedef ptrdiff_t isize;
typedef size_t usize;

typedef float f32;
typedef double f64;
#endif

#endif


#ifdef __cplusplus
#define BR_EXTERN_C_BEGIN extern "C" {
#define BR_EXTERN_C_END }
#else
#define BR_EXTERN_C_BEGIN
#define BR_EXTERN_C_END
#endif

#define BR_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define BR_CONCAT_INNER(a, b) a##b
#define BR_CONCAT(a, b) BR_CONCAT_INNER(a, b)
#define BR_DEFAULT_ALIGNMENT ((size_t)_Alignof(max_align_t))
#define BR_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#define BR_UNUSED(x) ((void)(x))

typedef enum br_status {
  BR_STATUS_OK = 0,
  BR_STATUS_INVALID_ARGUMENT,
  BR_STATUS_OUT_OF_MEMORY,
  BR_STATUS_NOT_SUPPORTED,
  BR_STATUS_EOF,
  BR_STATUS_UNEXPECTED_EOF,
  BR_STATUS_INVALID_STATE,
  BR_STATUS_SHORT_WRITE,
  BR_STATUS_SHORT_BUFFER,
  BR_STATUS_BUFFER_FULL,
  BR_STATUS_NO_PROGRESS,
  BR_STATUS_INVALID_ENCODING
} br_status;

static inline bool br_is_power_of_two_size(size_t value) {
  return value != 0u && (value & (value - 1u)) == 0u;
}

static inline size_t br_min_size(size_t a, size_t b) {
  return a < b ? a : b;
}

static inline size_t br_max_size(size_t a, size_t b) {
  return a > b ? a : b;
}

#endif

/* ==== bedrock/mem.h ==== */
#ifndef BEDROCK_MEM_H
#define BEDROCK_MEM_H

/* ==== bedrock/mem/alloc.h ==== */
#ifndef BEDROCK_MEM_ALLOC_H
#define BEDROCK_MEM_ALLOC_H


BR_EXTERN_C_BEGIN

typedef enum br_alloc_op {
  BR_ALLOC_OP_ALLOC = 0,
  BR_ALLOC_OP_ALLOC_UNINIT,
  BR_ALLOC_OP_RESIZE,
  BR_ALLOC_OP_RESIZE_UNINIT,
  BR_ALLOC_OP_FREE,
  BR_ALLOC_OP_RESET
} br_alloc_op;

typedef struct br_alloc_request {
  br_alloc_op op;
  void *ptr;
  size_t old_size;
  size_t size;
  size_t alignment;
} br_alloc_request;

typedef struct br_alloc_result {
  void *ptr;
  size_t size;
  br_status status;
} br_alloc_result;

typedef br_alloc_result (*br_allocator_fn)(void *ctx, const br_alloc_request *req);

typedef struct br_allocator {
  br_allocator_fn fn;
  void *ctx;
} br_allocator;

br_alloc_result br_allocator_call(br_allocator allocator, const br_alloc_request *req);

br_alloc_result br_allocator_alloc(br_allocator allocator, size_t size, size_t alignment);
br_alloc_result br_allocator_alloc_uninit(br_allocator allocator, size_t size, size_t alignment);
br_alloc_result br_allocator_resize(
  br_allocator allocator, void *ptr, size_t old_size, size_t new_size, size_t alignment);
br_alloc_result br_allocator_resize_uninit(
  br_allocator allocator, void *ptr, size_t old_size, size_t new_size, size_t alignment);
br_status br_allocator_free(br_allocator allocator, void *ptr, size_t old_size);
br_status br_allocator_reset(br_allocator allocator);

br_allocator br_allocator_heap(void);
br_allocator br_allocator_null(void);
br_allocator br_allocator_fail(void);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/arena.h ==== */
#ifndef BEDROCK_MEM_ARENA_H
#define BEDROCK_MEM_ARENA_H


BR_EXTERN_C_BEGIN

typedef struct br_arena {
  uint8_t *buffer;
  size_t capacity;
  size_t offset;
  size_t peak;
} br_arena;

typedef struct br_arena_mark {
  size_t offset;
} br_arena_mark;

void br_arena_init(br_arena *arena, void *buffer, size_t capacity);
void br_arena_reset(br_arena *arena);

br_arena_mark br_arena_mark_save(const br_arena *arena);
br_status br_arena_rewind(br_arena *arena, br_arena_mark mark);

br_alloc_result br_arena_alloc(br_arena *arena, size_t size, size_t alignment);
br_alloc_result br_arena_alloc_uninit(br_arena *arena, size_t size, size_t alignment);
br_allocator br_arena_allocator(br_arena *arena);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/compat_allocator.h ==== */
#ifndef BEDROCK_MEM_COMPAT_ALLOCATOR_H
#define BEDROCK_MEM_COMPAT_ALLOCATOR_H


BR_EXTERN_C_BEGIN

typedef struct br_compat_allocator {
  br_allocator parent;
} br_compat_allocator;

/*
Bedrock keeps Odin's compat allocator behavior close, with these intentional
C-side adaptations:
- explicit `parent` allocator defaults to heap when unset, instead of using
  Odin's ambient context allocator
- the wrapper only targets Bedrock's current alloc/free/resize/reset ABI, so
  there is no public generic query-features/query-info surface yet
- invalid pointers cannot be generically validated before reading the compat
  header, so ownership checking is still delegated to the parent allocator
*/

void br_compat_allocator_init(br_compat_allocator *compat, br_allocator parent);
br_allocator br_compat_allocator_allocator(br_compat_allocator *compat);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/mutex_allocator.h ==== */
#ifndef BEDROCK_MEM_MUTEX_ALLOCATOR_H
#define BEDROCK_MEM_MUTEX_ALLOCATOR_H

/* ==== bedrock/sync/primitives.h ==== */
#ifndef BEDROCK_SYNC_PRIMITIVES_H
#define BEDROCK_SYNC_PRIMITIVES_H

/* ==== bedrock/sync/primitives_atomic.h ==== */
#ifndef BEDROCK_SYNC_PRIMITIVES_ATOMIC_H
#define BEDROCK_SYNC_PRIMITIVES_ATOMIC_H

/* ==== bedrock/sync/futex.h ==== */
#ifndef BEDROCK_SYNC_FUTEX_H
#define BEDROCK_SYNC_FUTEX_H

/* ==== bedrock/sync/atomic.h ==== */
#ifndef BEDROCK_SYNC_ATOMIC_H
#define BEDROCK_SYNC_ATOMIC_H

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 201112L)
#error "Bedrock sync/atomic currently requires a C11-or-newer compiler."
#endif

#if defined(__STDC_NO_ATOMICS__)
#error "Bedrock sync/atomic requires C11 atomics; __STDC_NO_ATOMICS__ is defined."
#endif

#include <stdatomic.h>


BR_EXTERN_C_BEGIN

/*
Bedrock maps Odin's `core/sync/atomic` surface onto C11 atomics.

Most operations keep Odin's names and sequencing concepts, but compare-exchange
follows C's expected-pointer contract instead of Odin's tuple return. That is
the clean portable choice in C without introducing a large typed wrapper layer.

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

/* ==== bedrock/time/time.h ==== */
#ifndef BEDROCK_TIME_TIME_H
#define BEDROCK_TIME_TIME_H


BR_EXTERN_C_BEGIN

/*
Type representing duration, with nanosecond precision.
*/
typedef int64_t br_duration;

#define BR_NANOSECOND ((br_duration)1)
#define BR_MICROSECOND ((br_duration)(1000 * BR_NANOSECOND))
#define BR_MILLISECOND ((br_duration)(1000 * BR_MICROSECOND))
#define BR_SECOND ((br_duration)(1000 * BR_MILLISECOND))
#define BR_MINUTE ((br_duration)(60 * BR_SECOND))
#define BR_HOUR ((br_duration)(60 * BR_MINUTE))
#define BR_MIN_DURATION ((br_duration)INT64_MIN)
#define BR_MAX_DURATION ((br_duration)INT64_MAX)

/*
Specifies time since the Unix epoch, with nanosecond precision.
*/
typedef struct br_time {
  int64_t nsec;
} br_time;

/*
Type representing monotonic time, useful for measuring durations.
*/
typedef struct br_tick {
  int64_t nsec;
} br_tick;

bool br_time_is_supported(void);

br_time br_time_now(void);
void br_sleep(br_duration duration);
void br_yield(void);

br_tick br_tick_now(void);
br_tick br_tick_add(br_tick tick, br_duration duration);
br_duration br_tick_diff(br_tick start, br_tick end);
br_duration br_tick_since(br_tick start);

/*
Incrementally obtain durations since the last tick.

The `prev` pointer must point to a valid tick. If `*prev` is zero-initialized,
the returned duration is 0 and `*prev` is set to the current tick.
*/
br_duration br_tick_lap_time(br_tick *prev);

br_duration br_time_diff(br_time start, br_time end);
br_duration br_time_since(br_time start);

int64_t br_duration_nanoseconds(br_duration duration);
double br_duration_microseconds(br_duration duration);
double br_duration_milliseconds(br_duration duration);
double br_duration_seconds(br_duration duration);
double br_duration_minutes(br_duration duration);
double br_duration_hours(br_duration duration);

BR_EXTERN_C_END

#endif


BR_EXTERN_C_BEGIN

#if defined(__linux__) || defined(_WIN32) || (defined(__APPLE__) && defined(__MACH__)) ||          \
  defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define BR_SYNC_HAS_FUTEX 1
#else
#define BR_SYNC_HAS_FUTEX 0
#endif

/*
Odin models Futex as a distinct uint32_t. Bedrock keeps the same storage shape, but
uses an atomic uint32_t so callers can safely load/store the futex word in C.
*/
typedef br_atomic_u32 br_futex;

#define BR_FUTEX_INIT(value) BR_ATOMIC_INIT((uint32_t)(value))

/*
Sleep if the futex contains `expected`. The return value is Bedrock-specific:
Odin asserts backend failures, while C callers need a way to notice unsupported
or failed waits.
*/
bool br_futex_wait(br_futex *futex, uint32_t expected);
bool br_futex_wait_with_timeout(br_futex *futex, uint32_t expected, br_duration duration);
void br_futex_signal(br_futex *futex);
void br_futex_broadcast(br_futex *futex);

BR_EXTERN_C_END

#endif

/* ==== bedrock/sync/thread.h ==== */
#ifndef BEDROCK_SYNC_THREAD_H
#define BEDROCK_SYNC_THREAD_H


BR_EXTERN_C_BEGIN

typedef uint64_t br_thread_id;

#define BR_THREAD_ID_INVALID ((br_thread_id)0)

br_thread_id br_current_thread_id(void);

BR_EXTERN_C_END

#endif


BR_EXTERN_C_BEGIN

typedef enum br_atomic_mutex_state {
  BR_ATOMIC_MUTEX_UNLOCKED = 0,
  BR_ATOMIC_MUTEX_LOCKED,
  BR_ATOMIC_MUTEX_WAITING,
} br_atomic_mutex_state;

typedef size_t br_atomic_rw_mutex_state;

#define BR_ATOMIC_RW_MUTEX_STATE_IS_WRITING ((size_t)1 << (sizeof(size_t) * 8u - 1u))
#define BR_ATOMIC_RW_MUTEX_STATE_READER ((size_t)1)
#define BR_ATOMIC_RW_MUTEX_STATE_READER_MASK (~BR_ATOMIC_RW_MUTEX_STATE_IS_WRITING)

/*
An Atomic_Mutex is a mutual exclusion lock.
The zero value for an Atomic_Mutex is an unlocked mutex.

An Atomic_Mutex must not be copied after first use.

Odin's lower atomic primitives are zero-value-ready. Bedrock keeps that property
inside this layer and uses it as the storage for public primitives.
*/
typedef struct br_atomic_mutex {
  br_futex state;
} br_atomic_mutex;

typedef struct br_atomic_sema {
  br_futex count;
} br_atomic_sema;

/*
An Atomic_RW_Mutex is a reader/writer mutual exclusion lock.
The lock can be held by any arbitrary number of readers or a single writer.
The zero value for an Atomic_RW_Mutex is an unlocked mutex.

An Atomic_RW_Mutex must not be copied after first use.
*/
typedef struct br_atomic_rw_mutex {
  br_atomic_usize state;
  br_atomic_mutex mutex;
  br_atomic_sema sema;
} br_atomic_rw_mutex;

/*
An Atomic_Recursive_Mutex is a recursive mutual exclusion lock.
The zero value for an Atomic_Recursive_Mutex is an unlocked mutex.

An Atomic_Recursive_Mutex must not be copied after first use.

Odin stores a public Mutex here. Bedrock keeps the lower br_atomic_mutex inside
the atomic layer so this type remains independent from the public primitive
backend split.
*/
typedef struct br_atomic_recursive_mutex {
  br_atomic_u64 owner;
  uint32_t recursion;
  br_atomic_mutex mutex;
} br_atomic_recursive_mutex;

/*
Atomic_Cond implements a condition variable, a rendezvous point for threads
waiting for signalling the occurrence of an event.

An Atomic_Cond must not be copied after first use.
*/
typedef struct br_atomic_cond {
  br_futex state;
} br_atomic_cond;

#define BR_ATOMIC_MUTEX_INIT {.state = BR_FUTEX_INIT(BR_ATOMIC_MUTEX_UNLOCKED)}
#define BR_ATOMIC_SEMA_INIT(initial_count) {.count = BR_FUTEX_INIT(initial_count)}
#define BR_ATOMIC_RW_MUTEX_INIT                                                                    \
  {.state = BR_ATOMIC_INIT(0), .mutex = BR_ATOMIC_MUTEX_INIT, .sema = BR_ATOMIC_SEMA_INIT(0u)}
#define BR_ATOMIC_RECURSIVE_MUTEX_INIT                                                             \
  {.owner = BR_ATOMIC_INIT(BR_THREAD_ID_INVALID), .recursion = 0, .mutex = BR_ATOMIC_MUTEX_INIT}
#define BR_ATOMIC_COND_INIT {.state = BR_FUTEX_INIT(0u)}

void br_atomic_mutex_init(br_atomic_mutex *mutex);
void br_atomic_mutex_lock(br_atomic_mutex *mutex);
void br_atomic_mutex_unlock(br_atomic_mutex *mutex);
bool br_atomic_mutex_try_lock(br_atomic_mutex *mutex);

void br_atomic_rw_mutex_init(br_atomic_rw_mutex *rw);
void br_atomic_rw_mutex_lock(br_atomic_rw_mutex *rw);
void br_atomic_rw_mutex_unlock(br_atomic_rw_mutex *rw);
bool br_atomic_rw_mutex_try_lock(br_atomic_rw_mutex *rw);
void br_atomic_rw_mutex_shared_lock(br_atomic_rw_mutex *rw);
void br_atomic_rw_mutex_shared_unlock(br_atomic_rw_mutex *rw);
bool br_atomic_rw_mutex_try_shared_lock(br_atomic_rw_mutex *rw);

void br_atomic_recursive_mutex_init(br_atomic_recursive_mutex *mutex);
void br_atomic_recursive_mutex_lock(br_atomic_recursive_mutex *mutex);
void br_atomic_recursive_mutex_unlock(br_atomic_recursive_mutex *mutex);
bool br_atomic_recursive_mutex_try_lock(br_atomic_recursive_mutex *mutex);

void br_atomic_cond_init(br_atomic_cond *cond);
bool br_atomic_cond_wait(br_atomic_cond *cond, br_atomic_mutex *mutex);
bool br_atomic_cond_wait_with_timeout(br_atomic_cond *cond,
                                      br_atomic_mutex *mutex,
                                      br_duration duration);
void br_atomic_cond_signal(br_atomic_cond *cond);
void br_atomic_cond_broadcast(br_atomic_cond *cond);

void br_atomic_sema_init(br_atomic_sema *sema, uint32_t count);
void br_atomic_sema_post(br_atomic_sema *sema, uint32_t count);
void br_atomic_sema_wait(br_atomic_sema *sema);
bool br_atomic_sema_wait_with_timeout(br_atomic_sema *sema, br_duration duration);

BR_EXTERN_C_END

#endif


BR_EXTERN_C_BEGIN

/*
Bedrock keeps Odin's zero-value-ready sync model: the public primitives store
small atomic/futex-backed words instead of native objects that require hidden
construction. The init/destroy functions are compatibility reset/no-op helpers.
*/

typedef struct br_mutex {
  br_atomic_mutex impl;
} br_mutex;

typedef struct br_rw_mutex {
  br_atomic_rw_mutex impl;
} br_rw_mutex;

typedef struct br_recursive_mutex {
  br_atomic_recursive_mutex impl;
} br_recursive_mutex;

typedef struct br_cond {
  br_atomic_cond impl;
} br_cond;

typedef struct br_sema {
  br_atomic_sema impl;
} br_sema;

#define BR_MUTEX_INIT {.impl = BR_ATOMIC_MUTEX_INIT}
#define BR_RW_MUTEX_INIT {.impl = BR_ATOMIC_RW_MUTEX_INIT}
#define BR_RECURSIVE_MUTEX_INIT {.impl = BR_ATOMIC_RECURSIVE_MUTEX_INIT}
#define BR_COND_INIT {.impl = BR_ATOMIC_COND_INIT}
#define BR_SEMA_INIT(initial_count) {.impl = BR_ATOMIC_SEMA_INIT(initial_count)}

br_status br_mutex_init(br_mutex *mutex);
void br_mutex_destroy(br_mutex *mutex);
void br_mutex_lock(br_mutex *mutex);
void br_mutex_unlock(br_mutex *mutex);
bool br_mutex_try_lock(br_mutex *mutex);

br_status br_rw_mutex_init(br_rw_mutex *mutex);
void br_rw_mutex_destroy(br_rw_mutex *mutex);
void br_rw_mutex_lock(br_rw_mutex *mutex);
void br_rw_mutex_unlock(br_rw_mutex *mutex);
bool br_rw_mutex_try_lock(br_rw_mutex *mutex);
void br_rw_mutex_shared_lock(br_rw_mutex *mutex);
void br_rw_mutex_shared_unlock(br_rw_mutex *mutex);
bool br_rw_mutex_try_shared_lock(br_rw_mutex *mutex);

br_status br_recursive_mutex_init(br_recursive_mutex *mutex);
void br_recursive_mutex_destroy(br_recursive_mutex *mutex);
void br_recursive_mutex_lock(br_recursive_mutex *mutex);
void br_recursive_mutex_unlock(br_recursive_mutex *mutex);
bool br_recursive_mutex_try_lock(br_recursive_mutex *mutex);

br_status br_cond_init(br_cond *cond);
void br_cond_destroy(br_cond *cond);
void br_cond_wait(br_cond *cond, br_mutex *mutex);
bool br_cond_wait_with_timeout(br_cond *cond, br_mutex *mutex, br_duration duration);
void br_cond_signal(br_cond *cond);
void br_cond_broadcast(br_cond *cond);

br_status br_sema_init(br_sema *sema, uint32_t count);
void br_sema_destroy(br_sema *sema);
void br_sema_post(br_sema *sema, uint32_t count);
void br_sema_wait(br_sema *sema);
bool br_sema_wait_with_timeout(br_sema *sema, br_duration duration);

BR_EXTERN_C_END

#endif


BR_EXTERN_C_BEGIN

/*
Mutex allocator serializes every request forwarded to the backing allocator.
This mirrors Odin's `Mutex_Allocator`, with explicit backing selection instead
of an ambient context allocator.
*/
typedef struct br_mutex_allocator {
  br_allocator backing;
  br_mutex mutex;
} br_mutex_allocator;

void br_mutex_allocator_init(br_mutex_allocator *mutex_allocator, br_allocator backing);

/*
Return this mutex allocator as a generic allocator object.
*/
br_allocator br_mutex_allocator_as_allocator(br_mutex_allocator *mutex_allocator);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/dynamic_arena.h ==== */
#ifndef BEDROCK_MEM_DYNAMIC_ARENA_H
#define BEDROCK_MEM_DYNAMIC_ARENA_H


BR_EXTERN_C_BEGIN

#define BR_DYNAMIC_ARENA_DEFAULT_BLOCK_SIZE ((size_t)65536u)
#define BR_DYNAMIC_ARENA_DEFAULT_OUT_BAND_SIZE ((size_t)6554u)

typedef struct br_dynamic_arena {
  size_t block_size;
  size_t out_band_size;
  size_t minimum_alignment;
  void **unused_blocks;
  size_t unused_count;
  size_t unused_cap;
  void **used_blocks;
  size_t used_count;
  size_t used_cap;
  void **out_band_allocations;
  size_t out_band_count;
  size_t out_band_cap;
  void *current_block;
  uint8_t *current_pos;
  size_t bytes_left;
  br_allocator block_allocator;
  br_allocator array_allocator;
} br_dynamic_arena;

/*
Bedrock keeps Odin's dynamic arena behavior close, with these intentional
C-side adaptations:
- explicit `block_allocator` and `array_allocator` default to heap allocators
  when passed unset, instead of using Odin's ambient context allocator
- the direct allocation entry points (`br_dynamic_arena_alloc` and friends)
  allocate at the arena's `minimum_alignment`; per-request alignment is honored
  through the generic allocator adapter, where the effective alignment is
  `max(minimum_alignment, request)`
- the generic allocator adapter targets Bedrock's alloc/free/resize/reset ABI;
  `FREE` reports not-supported and `RESET` maps to `free_all`
- block cycling preserves the current block if acquiring the next block fails,
  instead of mutating state before the failing allocation completes
*/

br_status br_dynamic_arena_init(br_dynamic_arena *arena,
                                br_allocator block_allocator,
                                br_allocator array_allocator,
                                size_t block_size,
                                size_t out_band_size,
                                size_t minimum_alignment);
void br_dynamic_arena_destroy(br_dynamic_arena *arena);

void br_dynamic_arena_reset(br_dynamic_arena *arena);
void br_dynamic_arena_free_all(br_dynamic_arena *arena);

br_alloc_result br_dynamic_arena_alloc(br_dynamic_arena *arena, size_t size);
br_alloc_result br_dynamic_arena_alloc_uninit(br_dynamic_arena *arena, size_t size);
br_alloc_result
br_dynamic_arena_resize(br_dynamic_arena *arena, void *ptr, size_t old_size, size_t new_size);
br_alloc_result br_dynamic_arena_resize_uninit(br_dynamic_arena *arena,
                                               void *ptr,
                                               size_t old_size,
                                               size_t new_size);

br_allocator br_dynamic_arena_allocator(br_dynamic_arena *arena);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/buddy_allocator.h ==== */
#ifndef BEDROCK_MEM_BUDDY_ALLOCATOR_H
#define BEDROCK_MEM_BUDDY_ALLOCATOR_H


BR_EXTERN_C_BEGIN

typedef struct br_buddy_block br_buddy_block;

typedef struct br_buddy_allocator {
  br_buddy_block *head;
  br_buddy_block *tail;
  size_t alignment;
} br_buddy_allocator;

/*
Bedrock keeps Odin's buddy allocator behavior close, with these intentional
C-side adaptations:
- explicit `buffer` + `capacity` instead of Odin's `[]byte` backing slice
- initialization reports statuses instead of Odin's assertion-heavy diagnostics
- the generic allocator adapter targets Bedrock's current alloc/free/resize/reset
  ABI, not Odin's richer query-features/query-info modes
- all allocations use the allocator's fixed initialization alignment, so the
  generic allocator adapter ignores per-request alignments
*/

br_status
br_buddy_allocator_init(br_buddy_allocator *buddy, void *buffer, size_t capacity, size_t alignment);
void br_buddy_allocator_free_all(br_buddy_allocator *buddy);

br_alloc_result br_buddy_allocator_alloc(br_buddy_allocator *buddy, size_t size);
br_alloc_result br_buddy_allocator_alloc_uninit(br_buddy_allocator *buddy, size_t size);
br_status br_buddy_allocator_free(br_buddy_allocator *buddy, void *ptr);

br_allocator br_buddy_allocator_allocator(br_buddy_allocator *buddy);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/scratch.h ==== */
#ifndef BEDROCK_MEM_SCRATCH_H
#define BEDROCK_MEM_SCRATCH_H


BR_EXTERN_C_BEGIN

#define BR_SCRATCH_DEFAULT_BACKING_SIZE ((size_t)(4u * 1024u * 1024u))

typedef struct br_scratch_leaked_allocation {
  void *memory;
  size_t size;
} br_scratch_leaked_allocation;

typedef struct br_scratch {
  uint8_t *data;
  size_t capacity;
  size_t curr_offset;
  void *prev_allocation;
  void *prev_allocation_root;
  br_allocator backup_allocator;
  br_scratch_leaked_allocation *leaked_allocations;
  size_t leaked_count;
  size_t leaked_cap;
} br_scratch;

/*
Bedrock keeps Odin's current scratch allocator behavior close, with these
intentional C-side adaptations:
- no ambient context allocator/logger; explicit `backup_allocator` defaults to
  `br_allocator_heap()` when unset
- misuse reports statuses instead of Odin's assertion/panic-heavy diagnostics
- Bedrock does not emit Odin's warning log when the backup allocator is used
- copy paths use explicit `min(old_size, new_size)` semantics
*/

br_status br_scratch_init(br_scratch *scratch, size_t size, br_allocator backup_allocator);
void br_scratch_destroy(br_scratch *scratch);
void br_scratch_free_all(br_scratch *scratch);

br_alloc_result br_scratch_alloc(br_scratch *scratch, size_t size, size_t alignment);
br_alloc_result br_scratch_alloc_uninit(br_scratch *scratch, size_t size, size_t alignment);
br_status br_scratch_free(br_scratch *scratch, void *ptr);

br_alloc_result br_scratch_resize(
  br_scratch *scratch, void *ptr, size_t old_size, size_t new_size, size_t alignment);
br_alloc_result br_scratch_resize_uninit(
  br_scratch *scratch, void *ptr, size_t old_size, size_t new_size, size_t alignment);

br_allocator br_scratch_allocator(br_scratch *scratch);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/stack.h ==== */
#ifndef BEDROCK_MEM_STACK_H
#define BEDROCK_MEM_STACK_H


BR_EXTERN_C_BEGIN

typedef struct br_stack {
  uint8_t *data;
  size_t capacity;
  size_t prev_offset;
  size_t curr_offset;
  size_t peak_used;
} br_stack;

/*
Bedrock keeps Odin's stack allocator behavior close, with these intentional
C-side adaptations:
- explicit `buffer` + `capacity` instead of Odin's `[]byte` backing slice
- misuse reports statuses instead of Odin's panic-heavy diagnostics
- the allocator adapter targets Bedrock's current alloc/free/resize/reset ABI,
  not Odin's richer query-features/query-info modes
*/

void br_stack_init(br_stack *stack, void *buffer, size_t capacity);
void br_stack_free_all(br_stack *stack);

br_alloc_result br_stack_alloc(br_stack *stack, size_t size, size_t alignment);
br_alloc_result br_stack_alloc_uninit(br_stack *stack, size_t size, size_t alignment);
br_status br_stack_free(br_stack *stack, void *ptr);

br_alloc_result
br_stack_resize(br_stack *stack, void *ptr, size_t old_size, size_t new_size, size_t alignment);
br_alloc_result br_stack_resize_uninit(
  br_stack *stack, void *ptr, size_t old_size, size_t new_size, size_t alignment);

br_allocator br_stack_allocator(br_stack *stack);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/small_stack.h ==== */
#ifndef BEDROCK_MEM_SMALL_STACK_H
#define BEDROCK_MEM_SMALL_STACK_H


BR_EXTERN_C_BEGIN

typedef struct br_small_stack {
  uint8_t *data;
  size_t capacity;
  size_t offset;
  size_t peak_used;
} br_small_stack;

/*
Bedrock keeps Odin's small stack allocator behavior close, with these
intentional C-side adaptations:
- explicit `buffer` + `capacity` instead of Odin's `[]byte` backing slice
- misuse reports statuses instead of Odin's panic-heavy diagnostics
- the allocator adapter targets Bedrock's current alloc/free/resize/reset ABI,
  not Odin's richer query-features/query-info modes
- alignment is still expected to satisfy Bedrock's power-of-two allocator
  contract after Odin-style clamping to the small-stack maximum
*/

void br_small_stack_init(br_small_stack *stack, void *buffer, size_t capacity);
void br_small_stack_free_all(br_small_stack *stack);

br_alloc_result br_small_stack_alloc(br_small_stack *stack, size_t size, size_t alignment);
br_alloc_result br_small_stack_alloc_uninit(br_small_stack *stack, size_t size, size_t alignment);
br_status br_small_stack_free(br_small_stack *stack, void *ptr);

br_alloc_result br_small_stack_resize(
  br_small_stack *stack, void *ptr, size_t old_size, size_t new_size, size_t alignment);
br_alloc_result br_small_stack_resize_uninit(
  br_small_stack *stack, void *ptr, size_t old_size, size_t new_size, size_t alignment);

br_allocator br_small_stack_allocator(br_small_stack *stack);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/rollback_stack.h ==== */
#ifndef BEDROCK_MEM_ROLLBACK_STACK_H
#define BEDROCK_MEM_ROLLBACK_STACK_H


BR_EXTERN_C_BEGIN

/*
Reusable head blocks follow Odin's packed-header limit. Oversized singleton
allocations may still exceed this internally because they never chain previous
allocation offsets inside the same block.
*/
#define BR_ROLLBACK_STACK_DEFAULT_BLOCK_SIZE ((size_t)(4u * 1024u * 1024u))
#define BR_ROLLBACK_STACK_MAX_HEAD_BLOCK_SIZE ((size_t)0x7fffffffu)

typedef struct br_rollback_stack_block br_rollback_stack_block;

typedef struct br_rollback_stack {
  br_rollback_stack_block *head;
  size_t block_size;
  br_allocator block_allocator;
  bool head_owned;
} br_rollback_stack;

/*
Bedrock follows Odin's rollback stack allocator shape closely, but a few C-side
adaptations are intentional:
- initialization is split into explicit buffered and dynamic entry points
- invalid usage returns statuses instead of Odin's assertion-heavy diagnostics
- non-last resize fallback copies `min(old_size, new_size)` bytes to avoid
  relying on runtime copy helpers with implicit size semantics
*/

br_status br_rollback_stack_init_buffered(br_rollback_stack *stack, void *buffer, size_t capacity);
br_status br_rollback_stack_init_dynamic(br_rollback_stack *stack,
                                         size_t block_size,
                                         br_allocator block_allocator);

void br_rollback_stack_destroy(br_rollback_stack *stack);
void br_rollback_stack_reset(br_rollback_stack *stack);

br_alloc_result br_rollback_stack_alloc(br_rollback_stack *stack, size_t size, size_t alignment);
br_alloc_result
br_rollback_stack_alloc_uninit(br_rollback_stack *stack, size_t size, size_t alignment);
br_alloc_result br_rollback_stack_resize(
  br_rollback_stack *stack, void *ptr, size_t old_size, size_t new_size, size_t alignment);
br_alloc_result br_rollback_stack_resize_uninit(
  br_rollback_stack *stack, void *ptr, size_t old_size, size_t new_size, size_t alignment);
br_status br_rollback_stack_free(br_rollback_stack *stack, void *ptr);

br_allocator br_rollback_stack_allocator(br_rollback_stack *stack);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/tracking_allocator.h ==== */
#ifndef BEDROCK_MEM_TRACKING_ALLOCATOR_H
#define BEDROCK_MEM_TRACKING_ALLOCATOR_H


BR_EXTERN_C_BEGIN

typedef struct br_tracking_allocator_internal br_tracking_allocator_internal;

typedef struct br_tracking_allocator_entry {
  void *memory;
  size_t size;
  size_t alignment;
  br_alloc_op op;
  br_status status;
} br_tracking_allocator_entry;

typedef struct br_tracking_allocator_bad_free {
  void *memory;
  size_t size;
} br_tracking_allocator_bad_free;

typedef struct br_tracking_allocator_stats {
  size_t total_memory_allocated;
  size_t total_allocation_count;
  size_t total_memory_freed;
  size_t total_free_count;
  size_t peak_memory_allocated;
  size_t current_memory_allocated;
  size_t live_allocation_count;
} br_tracking_allocator_stats;

typedef void (*br_tracking_allocator_bad_free_fn)(void *ctx,
                                                  const br_tracking_allocator_bad_free *bad_free);

typedef struct br_tracking_allocator {
  br_allocator backing;
  br_allocator internals;
  br_mutex mutex;
  /*
  Dense live allocation list kept for leak inspection and reporting.
  Lookup uses a private pointer index instead of scanning this array linearly.
  */
  br_tracking_allocator_entry *entries;
  size_t entry_count;
  size_t entry_cap;
  br_tracking_allocator_bad_free *bad_frees;
  size_t bad_free_count;
  size_t bad_free_cap;
  br_tracking_allocator_stats stats;
  br_tracking_allocator_bad_free_fn bad_free_fn;
  void *bad_free_ctx;
  bool clear_on_reset;
  br_tracking_allocator_internal *internal;
} br_tracking_allocator;

/*
Bedrock's tracking allocator is intentionally smaller than Odin's current one:
- private pointer index instead of exposing Odin's allocation_map directly
- `clear_on_reset` is configured explicitly because Bedrock's allocator ABI
  does not yet expose Odin-style feature queries
- bad frees are recorded by default instead of trapping
- no source-location tracking yet
*/

void br_tracking_allocator_init(br_tracking_allocator *tracking,
                                br_allocator backing,
                                br_allocator internals);
void br_tracking_allocator_destroy(br_tracking_allocator *tracking);

/* Clears live allocation and bad-free records but keeps cumulative totals. */
void br_tracking_allocator_clear(br_tracking_allocator *tracking);

/* Resets both live tracking state and cumulative totals. */
void br_tracking_allocator_reset(br_tracking_allocator *tracking);

br_allocator br_tracking_allocator_allocator(br_tracking_allocator *tracking);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/virtual.h ==== */
#ifndef BEDROCK_MEM_VIRTUAL_H
#define BEDROCK_MEM_VIRTUAL_H


BR_EXTERN_C_BEGIN

typedef struct br_vm_region {
  uint8_t *data;
  size_t size;
} br_vm_region;

typedef struct br_vm_region_result {
  br_vm_region value;
  br_status status;
} br_vm_region_result;

typedef struct br_vm_mapped_file {
  uint8_t *data;
  size_t size;
} br_vm_mapped_file;

typedef enum br_vm_map_file_error {
  BR_VM_MAP_FILE_ERROR_NONE = 0,
  BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT,
  BR_VM_MAP_FILE_ERROR_OPEN_FAILURE,
  BR_VM_MAP_FILE_ERROR_STAT_FAILURE,
  BR_VM_MAP_FILE_ERROR_NEGATIVE_SIZE,
  BR_VM_MAP_FILE_ERROR_TOO_LARGE_SIZE,
  BR_VM_MAP_FILE_ERROR_MAP_FAILURE
} br_vm_map_file_error;

typedef struct br_vm_mapped_file_result {
  br_vm_mapped_file value;
  br_vm_map_file_error error;
} br_vm_mapped_file_result;

typedef uint32_t br_vm_protect_flags;
typedef uint32_t br_vm_map_file_flags;

enum {
  BR_VM_PROTECT_READ = 1u << 0,
  BR_VM_PROTECT_WRITE = 1u << 1,
  BR_VM_PROTECT_EXECUTE = 1u << 2
};

#define BR_VM_PROTECT_NONE ((br_vm_protect_flags)0u)

enum { BR_VM_MAP_FILE_READ = 1u << 0, BR_VM_MAP_FILE_WRITE = 1u << 1 };

/* Returns 0 when Bedrock does not have a VM backend for the current platform. */
size_t br_vm_page_size(void);

/*
Reserves a page-backed region without making it readable or writable yet on
platforms that support that distinction.
*/
br_vm_region_result br_vm_reserve(size_t size);

/* Reserve and immediately commit a region. */
br_vm_region_result br_vm_reserve_commit(size_t size);

/* Commit previously reserved pages so they can be read and written. */
br_status br_vm_commit(void *ptr, size_t size);

/* Decommit previously committed pages while keeping the reservation alive. */
void br_vm_decommit(void *ptr, size_t size);

/* Release an entire reserved region back to the operating system. */
void br_vm_release(void *ptr, size_t size);

/*
Change page protection flags for an already reserved region. Bedrock currently
exposes the raw primitive but not Odin's higher-level overflow-protected memory
block flags yet.
*/
bool br_vm_protect(void *ptr, size_t size, br_vm_protect_flags flags);

/*
This currently maps from a filesystem path rather than an already-open file
handle. That is a Bedrock v1 simplification; Odin exposes both path and file
entry points.
*/
br_vm_mapped_file_result br_vm_map_file(const char *path, br_vm_map_file_flags flags);
void br_vm_unmap_file(br_vm_mapped_file mapping);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/virtual_arena.h ==== */
#ifndef BEDROCK_MEM_VIRTUAL_ARENA_H
#define BEDROCK_MEM_VIRTUAL_ARENA_H


BR_EXTERN_C_BEGIN

typedef struct br_virtual_arena_block br_virtual_arena_block;
typedef uint32_t br_virtual_arena_flags;

enum { BR_VIRTUAL_ARENA_FLAG_OVERFLOW_PROTECTION = 1u << 0 };

typedef enum br_virtual_arena_kind {
  BR_VIRTUAL_ARENA_KIND_NONE = 0,
  BR_VIRTUAL_ARENA_KIND_GROWING,
  BR_VIRTUAL_ARENA_KIND_STATIC
} br_virtual_arena_kind;

typedef struct br_virtual_arena {
  br_virtual_arena_kind kind;
  br_virtual_arena_block *curr_block;
  size_t total_used;
  size_t total_reserved;
  size_t default_commit_size;
  size_t minimum_block_size;
  br_virtual_arena_flags flags;
  size_t temp_count;
  br_mutex mutex;
} br_virtual_arena;

typedef struct br_virtual_arena_mark {
  br_virtual_arena_block *block;
  size_t used;
} br_virtual_arena_mark;

typedef struct br_virtual_arena_temp {
  br_virtual_arena *arena;
  br_virtual_arena_block *block;
  size_t used;
} br_virtual_arena_temp;

typedef struct br_virtual_arena_temp_result {
  br_virtual_arena_temp value;
  br_status status;
} br_virtual_arena_temp_result;

#define BR_VIRTUAL_ARENA_DEFAULT_STATIC_COMMIT_SIZE ((size_t)1048576u)
#define BR_VIRTUAL_ARENA_DEFAULT_GROWING_COMMIT_SIZE ((size_t)8388608u)
#define BR_VIRTUAL_ARENA_DEFAULT_GROWING_MINIMUM_BLOCK_SIZE                                        \
  BR_VIRTUAL_ARENA_DEFAULT_STATIC_COMMIT_SIZE

#if UINTPTR_MAX > UINT32_MAX
#define BR_VIRTUAL_ARENA_DEFAULT_STATIC_RESERVE_SIZE ((size_t)1073741824u)
#else
#define BR_VIRTUAL_ARENA_DEFAULT_STATIC_RESERVE_SIZE ((size_t)134217728u)
#endif

/*
Bedrock v1 only exposes Odin's virtual growing/static variants here. Buffer
arenas already exist separately as `br_arena`, so this layer stays focused on
virtual-memory-backed arenas. Like Odin's current implementation, the virtual
arena embeds a mutex and serializes public arena mutation operations. Overflow
protection is available as a trailing guard page via `flags`; Bedrock does not
currently expose Odin's broader memory-block flag surface directly.
*/

void br_virtual_arena_init(br_virtual_arena *arena);

/*
Callers may prefill `default_commit_size` and `minimum_block_size` on a zeroed
arena before initialization, matching Odin's growth-policy style.
*/
br_status br_virtual_arena_init_growing(br_virtual_arena *arena, size_t reserved);
br_status
br_virtual_arena_init_static(br_virtual_arena *arena, size_t reserved, size_t commit_size);

void br_virtual_arena_reset(br_virtual_arena *arena);
void br_virtual_arena_destroy(br_virtual_arena *arena);

br_virtual_arena_mark br_virtual_arena_mark_save(br_virtual_arena *arena);
br_status br_virtual_arena_rewind(br_virtual_arena *arena, br_virtual_arena_mark mark);

/*
These helpers mirror Odin's Arena_Temp workflow, but Bedrock reports misuse
through statuses instead of asserting because the C API should not abort by
default on ownership mistakes.
*/
br_virtual_arena_temp_result br_virtual_arena_temp_begin(br_virtual_arena *arena);
br_status br_virtual_arena_temp_end(br_virtual_arena_temp temp);
br_status br_virtual_arena_temp_ignore(br_virtual_arena_temp temp);
br_status br_virtual_arena_check_temp(br_virtual_arena *arena);

br_alloc_result br_virtual_arena_alloc(br_virtual_arena *arena, size_t size, size_t alignment);
br_alloc_result
br_virtual_arena_alloc_uninit(br_virtual_arena *arena, size_t size, size_t alignment);
br_allocator br_virtual_arena_allocator(br_virtual_arena *arena);

BR_EXTERN_C_END

#endif

/* ==== bedrock/mem/virtual_arena_util.h ==== */
#ifndef BEDROCK_MEM_VIRTUAL_ARENA_UTIL_H
#define BEDROCK_MEM_VIRTUAL_ARENA_UTIL_H


BR_EXTERN_C_BEGIN

typedef struct br_virtual_arena_make_result {
  void *data;
  size_t len;
  br_status status;
} br_virtual_arena_make_result;

/*
This header is the C analogue of Odin's `virtual/arena_util.odin`: a typed
convenience layer over virtual-arena allocation. Because C has no `typeid`
overloads, Bedrock exposes raw helpers plus typed macros that assign directly
into caller variables and return `br_status`.
*/

br_alloc_result br_virtual_arena_new_raw(br_virtual_arena *arena, size_t size, size_t alignment);
br_alloc_result
br_virtual_arena_clone_raw(br_virtual_arena *arena, const void *src, size_t size, size_t alignment);
br_virtual_arena_make_result
br_virtual_arena_make_raw(br_virtual_arena *arena, size_t elem_size, size_t len, size_t alignment);

static inline br_status br__virtual_arena_assign_new(void **out_ptr, br_alloc_result result) {
  if (out_ptr != NULL) {
    *out_ptr = result.ptr;
  }

  return result.status;
}

static inline br_status br__virtual_arena_assign_make(void **out_ptr,
                                                      size_t *out_len,
                                                      br_virtual_arena_make_result result) {
  if (out_ptr != NULL) {
    *out_ptr = result.data;
  }
  if (out_len != NULL) {
    *out_len = result.status == BR_STATUS_OK ? result.len : 0u;
  }

  return result.status;
}

#define br_virtual_arena_new(arena, Type, out_ptr)                                                 \
  br__virtual_arena_assign_new(                                                                    \
    (void **)(void *)&(out_ptr),                                                                   \
    br_virtual_arena_new_raw((arena), sizeof(Type), (size_t)_Alignof(Type)))

#define br_virtual_arena_new_aligned(arena, Type, alignment, out_ptr)                              \
  br__virtual_arena_assign_new((void **)(void *)&(out_ptr),                                        \
                               br_virtual_arena_new_raw((arena), sizeof(Type), (alignment)))

#define br_virtual_arena_new_clone(arena, Type, value, out_ptr)                                    \
  br__virtual_arena_assign_new(                                                                    \
    (void **)(void *)&(out_ptr),                                                                   \
    br_virtual_arena_clone_raw(                                                                    \
      (arena), ((const Type[]){(value)}), sizeof(Type), (size_t)_Alignof(Type)))

#define br_virtual_arena_make_slice(arena, Type, len, out_ptr, out_len)                            \
  br__virtual_arena_assign_make(                                                                   \
    (void **)(void *)&(out_ptr),                                                                   \
    &(out_len),                                                                                    \
    br_virtual_arena_make_raw((arena), sizeof(Type), (len), (size_t)_Alignof(Type)))

#define br_virtual_arena_make_aligned(arena, Type, len, alignment, out_ptr, out_len)               \
  br__virtual_arena_assign_make(                                                                   \
    (void **)(void *)&(out_ptr),                                                                   \
    &(out_len),                                                                                    \
    br_virtual_arena_make_raw((arena), sizeof(Type), (len), (alignment)))

#define br_virtual_arena_make_multi_pointer(arena, Type, len, out_ptr)                             \
  br__virtual_arena_assign_make(                                                                   \
    (void **)(void *)&(out_ptr),                                                                   \
    NULL,                                                                                          \
    br_virtual_arena_make_raw((arena), sizeof(Type), (len), (size_t)_Alignof(Type)))

BR_EXTERN_C_END

#endif


#endif

/* ==== bedrock/bytes.h ==== */
#ifndef BEDROCK_BYTES_H
#define BEDROCK_BYTES_H

/* ==== bedrock/bytes/bytes.h ==== */
#ifndef BEDROCK_BYTES_BYTES_H
#define BEDROCK_BYTES_BYTES_H


BR_EXTERN_C_BEGIN

typedef struct br_bytes {
  uint8_t *data;
  size_t len;
} br_bytes;

typedef struct br_bytes_view {
  const uint8_t *data;
  size_t len;
} br_bytes_view;

typedef struct br_bytes_result {
  br_bytes value;
  br_status status;
} br_bytes_result;

typedef struct br_bytes_view_list {
  br_bytes_view *data;
  size_t len;
} br_bytes_view_list;

typedef struct br_bytes_view_list_result {
  br_bytes_view_list value;
  br_status status;
} br_bytes_view_list_result;

typedef struct br_bytes_rewrite_result {
  br_bytes_view value;
  br_bytes owned;
  bool allocated;
  br_status status;
} br_bytes_rewrite_result;

#define BR_BYTES_LIT(s) br_bytes_view_make((const void *)(s), sizeof(s) - 1u)

static inline br_bytes br_bytes_make(void *data, size_t len) {
  br_bytes bytes;

  bytes.data = (uint8_t *)data;
  bytes.len = len;
  return bytes;
}

static inline br_bytes_view br_bytes_view_make(const void *data, size_t len) {
  br_bytes_view bytes;

  bytes.data = (const uint8_t *)data;
  bytes.len = len;
  return bytes;
}

static inline br_bytes_view br_bytes_view_from_bytes(br_bytes bytes) {
  return br_bytes_view_make(bytes.data, bytes.len);
}

static inline br_bytes_view br_bytes_view_from_cstr(const char *s) {
  return br_bytes_view_make(s, s != NULL ? strlen(s) : 0u);
}

br_status br_bytes_free(br_bytes bytes, br_allocator allocator);
br_status br_bytes_view_list_free(br_bytes_view_list list, br_allocator allocator);
br_status br_bytes_rewrite_free(br_bytes_rewrite_result result, br_allocator allocator);
br_bytes_result br_bytes_clone(br_bytes_view src, br_allocator allocator);

int32_t br_bytes_compare(br_bytes_view lhs, br_bytes_view rhs);
bool br_bytes_equal(br_bytes_view lhs, br_bytes_view rhs);
bool br_bytes_has_prefix(br_bytes_view s, br_bytes_view prefix);
bool br_bytes_has_suffix(br_bytes_view s, br_bytes_view suffix);
bool br_bytes_contains(br_bytes_view s, br_bytes_view needle);
bool br_bytes_contains_any(br_bytes_view s, br_bytes_view chars);

ptrdiff_t br_bytes_index_byte(br_bytes_view s, uint8_t byte_value);
ptrdiff_t br_bytes_last_index_byte(br_bytes_view s, uint8_t byte_value);
ptrdiff_t br_bytes_index(br_bytes_view s, br_bytes_view needle);
ptrdiff_t br_bytes_last_index(br_bytes_view s, br_bytes_view needle);
ptrdiff_t br_bytes_index_any(br_bytes_view s, br_bytes_view chars);
ptrdiff_t br_bytes_count(br_bytes_view s, br_bytes_view needle);

br_bytes_view br_bytes_truncate_to_byte(br_bytes_view s, uint8_t byte_value);
br_bytes_view br_bytes_trim_prefix(br_bytes_view s, br_bytes_view prefix);
br_bytes_view br_bytes_trim_suffix(br_bytes_view s, br_bytes_view suffix);

br_bytes_result br_bytes_join(const br_bytes_view *parts,
                              size_t part_count,
                              br_bytes_view sep,
                              br_allocator allocator);
br_bytes_result
br_bytes_concat(const br_bytes_view *parts, size_t part_count, br_allocator allocator);
br_bytes_result br_bytes_repeat(br_bytes_view s, size_t count, br_allocator allocator);

/*
Split `s` around separator `sep`.

Like Odin's `bytes.split` family, an empty separator splits on UTF-8 rune
boundaries rather than being rejected.
*/
br_bytes_view_list_result
br_bytes_split(br_bytes_view s, br_bytes_view sep, br_allocator allocator);
br_bytes_view_list_result
br_bytes_split_n(br_bytes_view s, br_bytes_view sep, ptrdiff_t n, br_allocator allocator);
br_bytes_view_list_result
br_bytes_split_after(br_bytes_view s, br_bytes_view sep, br_allocator allocator);
br_bytes_view_list_result
br_bytes_split_after_n(br_bytes_view s, br_bytes_view sep, ptrdiff_t n, br_allocator allocator);

/*
Replace up to `n` occurrences of `old_bytes` with `new_bytes`.

If no rewrite is needed, `allocated` will be false and `value` will alias the
original input. If a rewrite allocates, `allocated` will be true, `owned` will
hold the allocation, and `value` will view that owned storage.

When `old_bytes` is empty, replacements are inserted at UTF-8 rune boundaries,
following Odin's `bytes.replace` semantics.
*/
br_bytes_rewrite_result br_bytes_replace(br_bytes_view s,
                                         br_bytes_view old_bytes,
                                         br_bytes_view new_bytes,
                                         ptrdiff_t n,
                                         br_allocator allocator);
br_bytes_rewrite_result br_bytes_replace_all(br_bytes_view s,
                                             br_bytes_view old_bytes,
                                             br_bytes_view new_bytes,
                                             br_allocator allocator);
br_bytes_rewrite_result
br_bytes_remove(br_bytes_view s, br_bytes_view key, ptrdiff_t n, br_allocator allocator);
br_bytes_rewrite_result
br_bytes_remove_all(br_bytes_view s, br_bytes_view key, br_allocator allocator);

BR_EXTERN_C_END

#endif

/* ==== bedrock/bytes/buffer.h ==== */
#ifndef BEDROCK_BYTES_BUFFER_H
#define BEDROCK_BYTES_BUFFER_H

/* ==== bedrock/io/io.h ==== */
#ifndef BEDROCK_IO_IO_H
#define BEDROCK_IO_IO_H

/* ==== bedrock/unicode/utf8.h ==== */
#ifndef BEDROCK_UNICODE_UTF8_H
#define BEDROCK_UNICODE_UTF8_H


BR_EXTERN_C_BEGIN

typedef int32_t br_rune;

#define BR_RUNE_ERROR ((br_rune)0xfffd)
#define BR_RUNE_SELF ((br_rune)0x80)
#define BR_RUNE_BOM ((br_rune)0xfeff)
#define BR_RUNE_EOF ((br_rune) - 1)
#define BR_RUNE_MAX ((br_rune)0x10ffff)
#define BR_UTF8_MAX 4

typedef struct br_utf8_decode_result {
  br_rune value;
  size_t width;
} br_utf8_decode_result;

typedef struct br_utf8_encode_result {
  uint8_t bytes[BR_UTF8_MAX];
  size_t len;
} br_utf8_encode_result;

/*
Encode `value` as UTF-8.

This follows Odin's `encode_rune`: invalid values, including surrogate code
points and values above the Unicode maximum, are encoded as the replacement
rune.
*/
br_utf8_encode_result br_utf8_encode(br_rune value);

/*
Decode the first rune in `s`.

This follows Odin's `decode_rune`: invalid encodings decode as the replacement
rune with width 1, and an empty input decodes as the replacement rune with
width 0.
*/
br_utf8_decode_result br_utf8_decode(br_bytes_view s);

/*
Decode the last rune in `s`.

This follows Odin's `decode_last_rune`: invalid trailing encodings decode as
the replacement rune with width 1, and an empty input decodes as the
replacement rune with width 0.
*/
br_utf8_decode_result br_utf8_decode_last(br_bytes_view s);

/*
Report whether `value` is a valid Unicode scalar value.
*/
bool br_utf8_valid_rune(br_rune value);

/*
Report whether `s` is entirely valid UTF-8.
*/
bool br_utf8_valid(br_bytes_view s);

/*
Report whether `byte_value` can begin a UTF-8 encoded rune.
*/
bool br_utf8_rune_start(uint8_t byte_value);

/*
Count runes in `s`.

Like Odin's `rune_count`, invalid or truncated encodings count as width-1 error
runes rather than being skipped.
*/
size_t br_utf8_rune_count(br_bytes_view s);

/*
Return the number of bytes required to encode `value`, or `-1` if `value` is
not a valid Unicode scalar value.
*/
int32_t br_utf8_rune_size(br_rune value);

/*
Report whether `s` begins with a complete UTF-8 encoding.

Like Odin's `full_rune`, invalid encodings are considered complete because they
decode as a width-1 replacement rune.
*/
bool br_utf8_full_rune(br_bytes_view s);

BR_EXTERN_C_END

#endif


BR_EXTERN_C_BEGIN

/*
Shared byte-count plus status result used by Bedrock IO operations.
*/
typedef struct br_io_result {
  size_t count;
  br_status status;
} br_io_result;

/*
Shared 64-bit integer plus status result used by stream procedures.
*/
typedef struct br_i64_result {
  int64_t value;
  br_status status;
} br_i64_result;

/*
Shared seek result used by Bedrock IO operations.
*/
typedef struct br_io_seek_result {
  int64_t offset;
  br_status status;
} br_io_seek_result;

/*
Shared size result used by Bedrock IO operations.
*/
typedef struct br_io_size_result {
  int64_t size;
  br_status status;
} br_io_size_result;

typedef struct br_io_byte_result {
  uint8_t value;
  br_status status;
} br_io_byte_result;

typedef struct br_io_rune_result {
  br_rune value;
  size_t width;
  br_status status;
} br_io_rune_result;

/*
Seek origin shared by byte and string readers and generic streams.
*/
typedef enum br_seek_from {
  BR_SEEK_FROM_START = 0,
  BR_SEEK_FROM_CURRENT,
  BR_SEEK_FROM_END
} br_seek_from;

/*
Stream operations follow Odin's single stream proc shape. Unsupported modes
return `BR_STATUS_NOT_SUPPORTED`.
*/
typedef enum br_io_mode {
  BR_IO_MODE_CLOSE = 0,
  BR_IO_MODE_FLUSH,
  BR_IO_MODE_READ,
  BR_IO_MODE_READ_AT,
  BR_IO_MODE_WRITE,
  BR_IO_MODE_WRITE_AT,
  BR_IO_MODE_SEEK,
  BR_IO_MODE_SIZE,
  BR_IO_MODE_DESTROY,
  BR_IO_MODE_QUERY,
  BR_IO_MODE_COUNT
} br_io_mode;

typedef uint64_t br_io_mode_set;
BR_STATIC_ASSERT(BR_IO_MODE_COUNT <= 64, "br_io_mode_set must fit all io mode bits");

typedef struct br_io_query_result {
  br_io_mode_set modes;
  br_status status;
} br_io_query_result;

typedef br_i64_result (*br_stream_proc)(
  void *context, br_io_mode mode, void *data, size_t data_len, int64_t offset, br_seek_from whence);

typedef struct br_stream {
  br_stream_proc procedure;
  void *context;
} br_stream;

typedef br_stream br_reader;
typedef br_stream br_writer;
typedef br_stream br_closer;
typedef br_stream br_flusher;
typedef br_stream br_seeker;
typedef br_stream br_reader_at;
typedef br_stream br_writer_at;

static inline br_io_result br_io_result_make(size_t count, br_status status) {
  br_io_result result;

  result.count = count;
  result.status = status;
  return result;
}

static inline br_i64_result br_i64_result_make(int64_t value, br_status status) {
  br_i64_result result;

  result.value = value;
  result.status = status;
  return result;
}

static inline br_io_seek_result br_io_seek_result_make(int64_t offset, br_status status) {
  br_io_seek_result result;

  result.offset = offset;
  result.status = status;
  return result;
}

static inline br_io_size_result br_io_size_result_make(int64_t size, br_status status) {
  br_io_size_result result;

  result.size = size;
  result.status = status;
  return result;
}

static inline br_io_byte_result br_io_byte_result_make(uint8_t value, br_status status) {
  br_io_byte_result result;

  result.value = value;
  result.status = status;
  return result;
}

static inline br_io_rune_result
br_io_rune_result_make(br_rune value, size_t width, br_status status) {
  br_io_rune_result result;

  result.value = value;
  result.width = width;
  result.status = status;
  return result;
}

static inline br_io_query_result br_io_query_result_make(br_io_mode_set modes, br_status status) {
  br_io_query_result result;

  result.modes = modes;
  result.status = status;
  return result;
}

static inline br_io_mode_set br_io_mode_bit(br_io_mode mode) {
  return ((br_io_mode_set)1u) << (unsigned)mode;
}

static inline br_i64_result br_stream_query_utility(br_io_mode_set modes) {
  return br_i64_result_make((int64_t)modes, BR_STATUS_OK);
}

static inline br_stream br_stream_make(void *context, br_stream_proc procedure) {
  br_stream stream;

  stream.procedure = procedure;
  stream.context = context;
  return stream;
}

static inline bool br_stream_is_valid(br_stream stream) {
  return stream.procedure != NULL;
}

static inline br_reader br_reader_make(void *context, br_stream_proc procedure) {
  return br_stream_make(context, procedure);
}

static inline br_writer br_writer_make(void *context, br_stream_proc procedure) {
  return br_stream_make(context, procedure);
}

static inline br_seeker br_seeker_make(void *context, br_stream_proc procedure) {
  return br_stream_make(context, procedure);
}

static inline bool br_reader_is_valid(br_reader reader) {
  return br_stream_is_valid(reader);
}

static inline bool br_writer_is_valid(br_writer writer) {
  return br_stream_is_valid(writer);
}

static inline bool br_seeker_is_valid(br_seeker seeker) {
  return br_stream_is_valid(seeker);
}

/*
Read bytes using a generic stream.
*/
br_io_result br_read(br_stream stream, void *dst, size_t dst_len);

/*
Read until at least `min_len` bytes have been copied into `dst`.

If `dst_len` is smaller than `min_len`, `BR_STATUS_SHORT_BUFFER` is returned.
If EOF happens after some bytes but before `min_len` bytes are read,
`BR_STATUS_UNEXPECTED_EOF` is returned.
*/
br_io_result br_read_at_least(br_stream stream, void *dst, size_t dst_len, size_t min_len);

/*
Read exactly `dst_len` bytes into `dst`.
*/
br_io_result br_read_full(br_stream stream, void *dst, size_t dst_len);

/*
Write bytes using a generic stream.
*/
br_io_result br_write(br_stream stream, const void *src, size_t src_len);

/*
Write until at least `min_len` bytes from `src` have been accepted.

If `src_len` is smaller than `min_len`, `BR_STATUS_SHORT_BUFFER` is returned.
*/
br_io_result br_write_at_least(br_stream stream, const void *src, size_t src_len, size_t min_len);

/*
Write exactly `src_len` bytes from `src`.
*/
br_io_result br_write_full(br_stream stream, const void *src, size_t src_len);

/*
Read from an explicit offset. If the stream does not implement `READ_AT`,
Bedrock falls back to `SEEK + READ + SEEK`.
*/
br_io_result br_read_at(br_stream stream, void *dst, size_t dst_len, int64_t offset);

/*
Write to an explicit offset. If the stream does not implement `WRITE_AT`,
Bedrock falls back to `SEEK + WRITE + SEEK`.
*/
br_io_result br_write_at(br_stream stream, const void *src, size_t src_len, int64_t offset);

/*
Seek using a generic stream.
*/
br_io_seek_result br_seek(br_stream stream, int64_t offset, br_seek_from whence);

/*
Close, flush, or destroy a generic stream.
*/
br_status br_close(br_stream stream);
br_status br_flush(br_stream stream);
br_status br_destroy(br_stream stream);

/*
Return the size of a generic stream. If `SIZE` is unsupported and `SEEK`
exists, Bedrock falls back to querying the current offset and end offset.
*/
br_io_size_result br_size(br_stream stream);

/*
Query supported stream modes.
*/
br_io_query_result br_query(br_stream stream);

/*
Read and return one byte from a generic stream.
*/
br_io_byte_result br_read_byte(br_stream stream);

/*
Write one byte to a generic stream.
*/
br_status br_write_byte(br_stream stream, uint8_t value);

/*
Read and decode one UTF-8 rune from a generic stream.

The reported width is the number of bytes consumed from the stream. For
malformed multi-byte prefixes this may be larger than the decoder's logical
replacement-rune width, because generic streams cannot necessarily unread or
peek ahead.
*/
br_io_rune_result br_read_rune(br_stream stream);

/*
Write one rune to a generic stream encoded as UTF-8.
*/
br_io_result br_write_rune(br_stream stream, br_rune value);

/*
Copy all remaining bytes from `src` to `dst` through an internal stack buffer.
EOF from the source is treated as a successful end-of-copy.
*/
br_i64_result br_copy(br_stream dst, br_stream src);

/*
Copy all remaining bytes from `src` to `dst` using caller-provided scratch
storage. `buffer` must be non-NULL and `buffer_len` must be greater than zero.
EOF from the source is treated as a successful end-of-copy.
*/
br_i64_result br_copy_buffer(br_stream dst, br_stream src, void *buffer, size_t buffer_len);

/*
Compatibility wrappers around the old split-trait helper names.
*/
static inline br_io_result br_reader_read(br_reader reader, void *dst, size_t dst_len) {
  return br_read(reader, dst, dst_len);
}

static inline br_io_result br_writer_write(br_writer writer, const void *src, size_t src_len) {
  return br_write(writer, src, src_len);
}

static inline br_io_seek_result
br_seeker_seek(br_seeker seeker, int64_t offset, br_seek_from whence) {
  return br_seek(seeker, offset, whence);
}

BR_EXTERN_C_END

#endif


BR_EXTERN_C_BEGIN

typedef struct br_byte_buffer {
  uint8_t *data;
  size_t len;
  size_t cap;
  size_t off;
  br_allocator allocator;
  bool can_unread_byte;
} br_byte_buffer;

typedef br_io_result br_byte_buffer_io_result;

typedef struct br_byte_buffer_byte_result {
  uint8_t value;
  br_status status;
} br_byte_buffer_byte_result;

void br_byte_buffer_init(br_byte_buffer *buffer, br_allocator allocator);
br_status br_byte_buffer_init_copy(br_byte_buffer *buffer,
                                   br_bytes_view initial_data,
                                   br_allocator allocator);
void br_byte_buffer_destroy(br_byte_buffer *buffer);
void br_byte_buffer_reset(br_byte_buffer *buffer);

bool br_byte_buffer_is_empty(const br_byte_buffer *buffer);
size_t br_byte_buffer_len(const br_byte_buffer *buffer);
size_t br_byte_buffer_capacity(const br_byte_buffer *buffer);
br_bytes_view br_byte_buffer_view(const br_byte_buffer *buffer);

br_status br_byte_buffer_reserve(br_byte_buffer *buffer, size_t additional);
br_status br_byte_buffer_truncate(br_byte_buffer *buffer, size_t n);

br_byte_buffer_io_result br_byte_buffer_write(br_byte_buffer *buffer, br_bytes_view src);
br_status br_byte_buffer_write_byte(br_byte_buffer *buffer, uint8_t byte_value);

br_bytes_view br_byte_buffer_next(br_byte_buffer *buffer, size_t n);
br_byte_buffer_io_result br_byte_buffer_read(br_byte_buffer *buffer, void *dst, size_t dst_len);
br_byte_buffer_byte_result br_byte_buffer_read_byte(br_byte_buffer *buffer);
br_status br_byte_buffer_unread_byte(br_byte_buffer *buffer);

/*
Expose this buffer through the generic stream interface.
*/
br_stream br_byte_buffer_as_stream(br_byte_buffer *buffer);

static inline br_reader br_byte_buffer_as_reader(br_byte_buffer *buffer) {
  return br_byte_buffer_as_stream(buffer);
}

static inline br_writer br_byte_buffer_as_writer(br_byte_buffer *buffer) {
  return br_byte_buffer_as_stream(buffer);
}

BR_EXTERN_C_END

#endif

/* ==== bedrock/bytes/reader.h ==== */
#ifndef BEDROCK_BYTES_READER_H
#define BEDROCK_BYTES_READER_H


BR_EXTERN_C_BEGIN

/*
A Reader is a read-only byte cursor over an existing byte slice.

This follows the shape of Odin's `bytes.Reader`: it does not own the source
memory, it tracks a current read index, and it supports random-access reads
without changing the main cursor.
*/
typedef struct br_byte_reader {
  br_bytes_view source;
  int64_t index;
} br_byte_reader;

typedef br_io_result br_byte_reader_io_result;

typedef struct br_byte_reader_byte_result {
  uint8_t value;
  br_status status;
} br_byte_reader_byte_result;

typedef br_io_seek_result br_byte_reader_seek_result;

/*
Initialize a reader to consume `source`.

The reader does not copy or own `source`; the caller must keep the underlying
memory alive for the lifetime of the reader.
*/
void br_byte_reader_init(br_byte_reader *reader, br_bytes_view source);

/*
Reset a reader back to the beginning of its current source.
*/
void br_byte_reader_reset(br_byte_reader *reader);

/*
Return the unread portion of the source as a view.
*/
br_bytes_view br_byte_reader_view(const br_byte_reader *reader);

/*
Return the number of unread bytes remaining.

Like Odin's `reader_length`, this clamps at zero when the cursor is already at
or beyond the end of the source.
*/
size_t br_byte_reader_len(const br_byte_reader *reader);

/*
Return the total byte size of the underlying source.
*/
size_t br_byte_reader_size(const br_byte_reader *reader);

/*
Read into `dst` from the current cursor and advance the cursor by the number of
bytes copied.

This mirrors Odin's `reader_read`: reaching the end after a partial read does
not itself produce EOF. EOF is reported only when no bytes can be read.
*/
br_byte_reader_io_result br_byte_reader_read(br_byte_reader *reader, void *dst, size_t dst_len);

/*
Read from an explicit `offset` without changing the main cursor.

This mirrors Odin's `reader_read_at`: a short read at the end of the source
returns the bytes that were copied and also reports EOF.
*/
br_byte_reader_io_result
br_byte_reader_read_at(const br_byte_reader *reader, void *dst, size_t dst_len, int64_t offset);

/*
Read one byte and advance the cursor by one.
*/
br_byte_reader_byte_result br_byte_reader_read_byte(br_byte_reader *reader);

/*
Move the cursor back by one byte.

This deliberately follows Odin's `reader_unread_byte` behavior: it is valid
whenever the current index is greater than zero, not only immediately after a
successful `read_byte`.
*/
br_status br_byte_reader_unread_byte(br_byte_reader *reader);

/*
Seek relative to the start, current cursor, or end of the source.

Like Odin's `reader_seek`, seeking past the end is allowed. Only negative final
offsets are rejected.
*/
br_byte_reader_seek_result
br_byte_reader_seek(br_byte_reader *reader, int64_t offset, br_seek_from whence);

/*
Expose this reader through the generic stream interface.
*/
br_stream br_byte_reader_as_stream(br_byte_reader *reader);

static inline br_reader br_byte_reader_as_reader(br_byte_reader *reader) {
  return br_byte_reader_as_stream(reader);
}

static inline br_seeker br_byte_reader_as_seeker(br_byte_reader *reader) {
  return br_byte_reader_as_stream(reader);
}

BR_EXTERN_C_END

#endif


#endif

/* ==== bedrock/unicode.h ==== */
#ifndef BEDROCK_UNICODE_H
#define BEDROCK_UNICODE_H


#endif

/* ==== bedrock/strings.h ==== */
#ifndef BEDROCK_STRINGS_H
#define BEDROCK_STRINGS_H

/* ==== bedrock/strings/strings.h ==== */
#ifndef BEDROCK_STRINGS_STRINGS_H
#define BEDROCK_STRINGS_STRINGS_H


BR_EXTERN_C_BEGIN

typedef struct br_string {
  char *data;
  size_t len;
} br_string;

typedef struct br_string_view {
  const char *data;
  size_t len;
} br_string_view;

typedef struct br_string_result {
  br_string value;
  br_status status;
} br_string_result;

typedef struct br_string_view_list {
  br_string_view *data;
  size_t len;
} br_string_view_list;

typedef struct br_string_view_list_result {
  br_string_view_list value;
  br_status status;
} br_string_view_list_result;

typedef struct br_string_rewrite_result {
  br_string_view value;
  br_string owned;
  bool allocated;
  br_status status;
} br_string_rewrite_result;

#define BR_STR_LIT(s) br_string_view_make((s), sizeof(s) - 1u)

static inline br_string br_string_make(void *data, size_t len) {
  br_string string;

  string.data = (char *)data;
  string.len = len;
  return string;
}

static inline br_string_view br_string_view_make(const void *data, size_t len) {
  br_string_view string;

  string.data = (const char *)data;
  string.len = len;
  return string;
}

static inline br_string_view br_string_view_from_string(br_string string) {
  return br_string_view_make(string.data, string.len);
}

static inline br_string_view br_string_view_from_cstr(const char *s) {
  return br_string_view_make(s, s != NULL ? strlen(s) : 0u);
}

static inline br_bytes_view br_string_view_as_bytes(br_string_view s) {
  return br_bytes_view_make(s.data, s.len);
}

/*
Free an owned string allocation.
*/
br_status br_string_free(br_string string, br_allocator allocator);
br_status br_string_view_list_free(br_string_view_list list, br_allocator allocator);
br_status br_string_rewrite_free(br_string_rewrite_result result, br_allocator allocator);

/*
Clone `s` into an owned string allocation.

This follows the shape of Odin's `strings.clone`, but keeps allocation explicit
instead of using an implicit context allocator.
*/
br_string_result br_string_clone(br_string_view s, br_allocator allocator);

/*
Compare two strings lexicographically.
*/
int32_t br_string_compare(br_string_view lhs, br_string_view rhs);

/*
Return whether two strings have the same bytes.
*/
bool br_string_equal(br_string_view lhs, br_string_view rhs);

/*
Return whether `s` begins with `prefix`.
*/
bool br_string_has_prefix(br_string_view s, br_string_view prefix);

/*
Return whether `s` ends with `suffix`.
*/
bool br_string_has_suffix(br_string_view s, br_string_view suffix);

/*
Return whether `needle` occurs within `s`.
*/
bool br_string_contains(br_string_view s, br_string_view needle);
bool br_string_contains_any(br_string_view s, br_string_view chars);

/*
Return whether Unicode scalar value `value` occurs within `s`.

This follows Odin's `contains_rune` and walks the string as UTF-8 rather than
scanning raw bytes.
*/
bool br_string_contains_rune(br_string_view s, br_rune value);

/*
Report whether `s` is valid UTF-8.
*/
bool br_string_valid(br_string_view s);

/*
Return the byte offset of `needle` within `s`, or `-1` if it is absent.
*/
ptrdiff_t br_string_index(br_string_view s, br_string_view needle);
ptrdiff_t br_string_index_byte(br_string_view s, uint8_t byte_value);

/*
Return the byte offset of the first occurrence of rune `value`, or `-1` if it
is absent.
*/
ptrdiff_t br_string_index_rune(br_string_view s, br_rune value);
ptrdiff_t br_string_last_index(br_string_view s, br_string_view needle);
ptrdiff_t br_string_last_index_byte(br_string_view s, uint8_t byte_value);
ptrdiff_t br_string_index_any(br_string_view s, br_string_view chars);
ptrdiff_t br_string_last_index_any(br_string_view s, br_string_view chars);
size_t br_string_count(br_string_view s, br_string_view needle);

/*
Return the number of UTF-8 runes in `s`.
*/
size_t br_string_rune_count(br_string_view s);

br_string_result br_string_join(const br_string_view *parts,
                                size_t part_count,
                                br_string_view sep,
                                br_allocator allocator);
br_string_result
br_string_concat(const br_string_view *parts, size_t part_count, br_allocator allocator);
br_string_result br_string_repeat(br_string_view s, size_t count, br_allocator allocator);

/*
Split `s` around separator `sep`.

Unlike the current byte-oriented `bytes` layer, an empty separator follows
Odin's string behavior and splits on UTF-8 rune boundaries.
*/
br_string_view_list_result
br_string_split(br_string_view s, br_string_view sep, br_allocator allocator);
br_string_view_list_result
br_string_split_n(br_string_view s, br_string_view sep, ptrdiff_t n, br_allocator allocator);
br_string_view_list_result
br_string_split_after(br_string_view s, br_string_view sep, br_allocator allocator);
br_string_view_list_result
br_string_split_after_n(br_string_view s, br_string_view sep, ptrdiff_t n, br_allocator allocator);

/*
Replace up to `n` occurrences of `old_string` with `new_string`.

If no rewrite is needed, `allocated` will be false and `value` will alias the
original input. If a rewrite allocates, `allocated` will be true, `owned` will
hold the allocation, and `value` will view that owned storage.

When `old_string` is empty, replacements are inserted at UTF-8 rune
boundaries, following Odin's string semantics.
*/
br_string_rewrite_result br_string_replace(br_string_view s,
                                           br_string_view old_string,
                                           br_string_view new_string,
                                           ptrdiff_t n,
                                           br_allocator allocator);
br_string_rewrite_result br_string_replace_all(br_string_view s,
                                               br_string_view old_string,
                                               br_string_view new_string,
                                               br_allocator allocator);
br_string_rewrite_result
br_string_remove(br_string_view s, br_string_view key, ptrdiff_t n, br_allocator allocator);
br_string_rewrite_result
br_string_remove_all(br_string_view s, br_string_view key, br_allocator allocator);

/*
Truncate `s` at the first occurrence of byte `byte_value`.

If `byte_value` does not occur, the original string view is returned.
*/
br_string_view br_string_truncate_to_byte(br_string_view s, uint8_t byte_value);

/*
Truncate `s` at the first occurrence of rune `value`.

If `value` does not occur, the original string view is returned.
*/
br_string_view br_string_truncate_to_rune(br_string_view s, br_rune value);

/*
Trim `prefix` from the start of `s` when present.
*/
br_string_view br_string_trim_prefix(br_string_view s, br_string_view prefix);

/*
Trim `suffix` from the end of `s` when present.
*/
br_string_view br_string_trim_suffix(br_string_view s, br_string_view suffix);

BR_EXTERN_C_END

#endif

/* ==== bedrock/strings/builder.h ==== */
#ifndef BEDROCK_STRINGS_BUILDER_H
#define BEDROCK_STRINGS_BUILDER_H


BR_EXTERN_C_BEGIN

/*
A dynamic byte buffer for building UTF-8 strings.

This follows the broad role of Odin's `strings.Builder`, but the C version
keeps allocation and ownership explicit. A builder is either heap-backed and
growable, or backed by a caller-provided fixed buffer.
*/
typedef struct br_string_builder {
  char *data;
  size_t len;
  size_t cap;
  br_allocator allocator;
  bool owns_storage;
} br_string_builder;

typedef br_io_result br_string_builder_io_result;

typedef struct br_string_builder_byte_result {
  uint8_t value;
  br_status status;
} br_string_builder_byte_result;

typedef struct br_string_builder_rune_result {
  br_rune value;
  size_t width;
  br_status status;
} br_string_builder_rune_result;

/*
Initialize an empty heap-backed builder.
*/
void br_string_builder_init(br_string_builder *builder, br_allocator allocator);

/*
Initialize a heap-backed builder with at least `initial_capacity` bytes.
*/
br_status br_string_builder_init_with_capacity(br_string_builder *builder,
                                               size_t initial_capacity,
                                               br_allocator allocator);

/*
Initialize a builder over caller-provided storage.

This follows the intent of Odin's `builder_from_bytes`: the builder does not
own the storage and cannot grow past `backing_len`.
*/
void br_string_builder_init_with_backing(br_string_builder *builder,
                                         void *backing,
                                         size_t backing_len);

/*
Release any owned storage and reset the builder to empty.
*/
void br_string_builder_destroy(br_string_builder *builder);

/*
Clear the builder contents without releasing capacity.
*/
void br_string_builder_reset(br_string_builder *builder);

/*
Return whether the builder currently holds no bytes.
*/
bool br_string_builder_is_empty(const br_string_builder *builder);

/*
Return the current byte length of the builder contents.
*/
size_t br_string_builder_len(const br_string_builder *builder);

/*
Return the total byte capacity of the current backing storage.
*/
size_t br_string_builder_capacity(const br_string_builder *builder);

/*
Return the remaining writable space before another growth would be required.
*/
size_t br_string_builder_space(const br_string_builder *builder);

/*
Return the current contents as a non-owning string view.
*/
br_string_view br_string_builder_view(const br_string_builder *builder);

/*
Ensure that at least `additional` more bytes can be appended.
*/
br_status br_string_builder_reserve(br_string_builder *builder, size_t additional);

/*
Truncate the builder to exactly `n` bytes.
*/
br_status br_string_builder_truncate(br_string_builder *builder, size_t n);

/*
Clone the current builder contents into an owned string allocation.
*/
br_string_result br_string_builder_clone(const br_string_builder *builder, br_allocator allocator);

/*
Append `src` to the builder.
*/
br_string_builder_io_result br_string_builder_write(br_string_builder *builder, br_string_view src);

/*
Append one byte to the builder.
*/
br_status br_string_builder_write_byte(br_string_builder *builder, uint8_t byte_value);

/*
Append one rune to the builder encoded as UTF-8.
*/
br_string_builder_io_result br_string_builder_write_rune(br_string_builder *builder, br_rune value);

/*
Pop and return the last byte from the builder.
*/
br_string_builder_byte_result br_string_builder_pop_byte(br_string_builder *builder);

/*
Pop and return the last UTF-8 rune from the builder.

Invalid trailing encodings follow the UTF-8 decoder behavior and pop as a
replacement rune of width 1.
*/
br_string_builder_rune_result br_string_builder_pop_rune(br_string_builder *builder);

/*
Expose this builder through the generic stream interface.
*/
br_stream br_string_builder_as_stream(br_string_builder *builder);

static inline br_writer br_string_builder_as_writer(br_string_builder *builder) {
  return br_string_builder_as_stream(builder);
}

BR_EXTERN_C_END

#endif

/* ==== bedrock/strings/reader.h ==== */
#ifndef BEDROCK_STRINGS_READER_H
#define BEDROCK_STRINGS_READER_H


BR_EXTERN_C_BEGIN

/*
A Reader is a read-only byte and rune cursor over an existing UTF-8 string.

This follows the shape of Odin's `strings.Reader`: it does not own the source
memory, it tracks a current byte index, and it supports both byte-wise and
rune-wise reads over the same input.
*/
typedef struct br_string_reader {
  br_string_view source;
  int64_t index;
  int64_t prev_rune;
} br_string_reader;

typedef br_io_result br_string_reader_io_result;

typedef struct br_string_reader_byte_result {
  uint8_t value;
  br_status status;
} br_string_reader_byte_result;

typedef struct br_string_reader_rune_result {
  br_rune value;
  size_t width;
  br_status status;
} br_string_reader_rune_result;

typedef br_io_seek_result br_string_reader_seek_result;

/*
Initialize a reader to consume `source`.

The reader does not copy or own `source`; the caller must keep the underlying
memory alive for the lifetime of the reader.
*/
void br_string_reader_init(br_string_reader *reader, br_string_view source);

/*
Reset a reader back to the beginning of its current source.
*/
void br_string_reader_reset(br_string_reader *reader);

/*
Return the unread portion of the source as a view.
*/
br_string_view br_string_reader_view(const br_string_reader *reader);

/*
Return the number of unread bytes remaining.

Like Odin's `reader_length`, this clamps at zero when the cursor is already at
or beyond the end of the source.
*/
size_t br_string_reader_len(const br_string_reader *reader);

/*
Return the total byte size of the underlying source.
*/
size_t br_string_reader_size(const br_string_reader *reader);

/*
Read into `dst` from the current cursor and advance the cursor by the number of
bytes copied.

This mirrors Odin's `reader_read`: reaching the end after a partial read does
not itself produce EOF. EOF is reported only when no bytes can be read.
*/
br_string_reader_io_result
br_string_reader_read(br_string_reader *reader, void *dst, size_t dst_len);

/*
Read from an explicit `offset` without changing the main cursor.

This mirrors Odin's `reader_read_at`: a short read at the end of the source
returns the bytes that were copied and also reports EOF.
*/
br_string_reader_io_result
br_string_reader_read_at(const br_string_reader *reader, void *dst, size_t dst_len, int64_t offset);

/*
Read one byte and advance the cursor by one.
*/
br_string_reader_byte_result br_string_reader_read_byte(br_string_reader *reader);

/*
Move the cursor back by one byte.

This follows Odin's `reader_unread_byte`: it is valid whenever the current
index is greater than zero.
*/
br_status br_string_reader_unread_byte(br_string_reader *reader);

/*
Read one UTF-8 rune and advance the cursor by the encoded rune width.

Invalid encodings follow the underlying UTF-8 decoder behavior and are returned
as the replacement rune with width 1.
*/
br_string_reader_rune_result br_string_reader_read_rune(br_string_reader *reader);

/*
Move the cursor back to the start of the last rune returned by `read_rune`.

Like Odin's `reader_unread_rune`, this is only valid immediately after a
successful `read_rune` that has not been invalidated by another operation.
*/
br_status br_string_reader_unread_rune(br_string_reader *reader);

/*
Seek relative to the start, current cursor, or end of the source.

Like Odin's `reader_seek`, seeking past the end is allowed. Only negative final
offsets are rejected.
*/
br_string_reader_seek_result
br_string_reader_seek(br_string_reader *reader, int64_t offset, br_seek_from whence);

/*
Expose this reader through the generic stream interface.
*/
br_stream br_string_reader_as_stream(br_string_reader *reader);

static inline br_reader br_string_reader_as_reader(br_string_reader *reader) {
  return br_string_reader_as_stream(reader);
}

static inline br_seeker br_string_reader_as_seeker(br_string_reader *reader) {
  return br_string_reader_as_stream(reader);
}

BR_EXTERN_C_END

#endif


#endif

/* ==== bedrock/io.h ==== */
#ifndef BEDROCK_IO_H
#define BEDROCK_IO_H


#endif

/* ==== bedrock/bufio.h ==== */
#ifndef BEDROCK_BUFIO_H
#define BEDROCK_BUFIO_H

/* ==== bedrock/bufio/lookahead_reader.h ==== */
#ifndef BEDROCK_BUFIO_LOOKAHEAD_READER_H
#define BEDROCK_BUFIO_LOOKAHEAD_READER_H

/* ==== bedrock/bufio/common.h ==== */
#ifndef BEDROCK_BUFIO_COMMON_H
#define BEDROCK_BUFIO_COMMON_H


BR_EXTERN_C_BEGIN

enum {
  BR_BUFIO_DEFAULT_BUFFER_SIZE = 4096u,
  BR_BUFIO_MIN_BUFFER_SIZE = 16u,
  BR_BUFIO_DEFAULT_MAX_CONSECUTIVE_EMPTY_READS = 128u,
  BR_BUFIO_DEFAULT_MAX_CONSECUTIVE_EMPTY_WRITES = 128u
};

BR_EXTERN_C_END

#endif


BR_EXTERN_C_BEGIN

typedef struct br_bufio_lookahead_reader {
  uint8_t *buf;
  size_t cap;
  size_t n;
  br_stream source;
  br_allocator allocator;
  bool owns_storage;
} br_bufio_lookahead_reader;

typedef struct br_bufio_lookahead_reader_peek_result {
  br_bytes_view value;
  br_status status;
} br_bufio_lookahead_reader_peek_result;

/*
Initialize a heap-backed lookahead reader using the default buffer size.

Unlike `br_bufio_reader`, the lookahead reader keeps the exact requested
capacity instead of clamping it to a minimum.
*/
br_status br_bufio_lookahead_reader_init(br_bufio_lookahead_reader *reader,
                                         br_stream source,
                                         br_allocator allocator);

/*
Initialize a heap-backed lookahead reader with an exact buffer size.
*/
br_status br_bufio_lookahead_reader_init_with_size(br_bufio_lookahead_reader *reader,
                                                   br_stream source,
                                                   size_t size,
                                                   br_allocator allocator);

/*
Initialize a lookahead reader over caller-provided storage.
*/
br_status br_bufio_lookahead_reader_init_with_buffer(br_bufio_lookahead_reader *reader,
                                                     br_stream source,
                                                     void *buffer,
                                                     size_t buffer_len);

void br_bufio_lookahead_reader_destroy(br_bufio_lookahead_reader *reader);
void br_bufio_lookahead_reader_reset(br_bufio_lookahead_reader *reader, br_stream source);

/*
Return the currently populated lookahead bytes.
*/
br_bytes_view br_bufio_lookahead_reader_buffer(const br_bufio_lookahead_reader *reader);

/*
Return a view holding at most `n` bytes.

If the underlying stream ends before `n` bytes are available, the returned
status is `BR_STATUS_EOF`, matching Odin's lookahead reader semantics.
`BR_STATUS_BUFFER_FULL` means `n` exceeds the fixed lookahead capacity.
*/
br_bufio_lookahead_reader_peek_result
br_bufio_lookahead_reader_peek(br_bufio_lookahead_reader *reader, size_t n);

/*
Populate and return the full lookahead buffer.
*/
br_bufio_lookahead_reader_peek_result
br_bufio_lookahead_reader_peek_all(br_bufio_lookahead_reader *reader);

/*
Drop the first `n` populated bytes.
*/
br_status br_bufio_lookahead_reader_consume(br_bufio_lookahead_reader *reader, size_t n);

/*
Drop all populated bytes.
*/
br_status br_bufio_lookahead_reader_consume_all(br_bufio_lookahead_reader *reader);

BR_EXTERN_C_END

#endif

/* ==== bedrock/bufio/read_writer.h ==== */
#ifndef BEDROCK_BUFIO_READ_WRITER_H
#define BEDROCK_BUFIO_READ_WRITER_H

/* ==== bedrock/bufio/reader.h ==== */
#ifndef BEDROCK_BUFIO_READER_H
#define BEDROCK_BUFIO_READER_H


BR_EXTERN_C_BEGIN

typedef struct br_bufio_reader {
  uint8_t *buf;
  size_t cap;
  size_t r;
  size_t w;
  br_stream source;
  br_allocator allocator;
  br_status err;
  bool owns_storage;
  ptrdiff_t last_byte;
  ptrdiff_t last_rune_size;
  size_t max_consecutive_empty_reads;
} br_bufio_reader;

typedef br_io_result br_bufio_reader_io_result;
typedef br_io_byte_result br_bufio_reader_byte_result;
typedef br_io_rune_result br_bufio_reader_rune_result;

typedef struct br_bufio_reader_peek_result {
  br_bytes_view value;
  br_status status;
} br_bufio_reader_peek_result;

typedef br_bufio_reader_peek_result br_bufio_reader_slice_result;

/*
Initialize a heap-backed buffered reader using the default buffer size.
*/
br_status br_bufio_reader_init(br_bufio_reader *reader, br_stream source, br_allocator allocator);

/*
Initialize a heap-backed buffered reader with an explicit buffer size.
*/
br_status br_bufio_reader_init_with_size(br_bufio_reader *reader,
                                         br_stream source,
                                         size_t size,
                                         br_allocator allocator);

/*
Initialize a buffered reader over caller-provided storage.
*/
br_status br_bufio_reader_init_with_buffer(br_bufio_reader *reader,
                                           br_stream source,
                                           void *buffer,
                                           size_t buffer_len);
void br_bufio_reader_destroy(br_bufio_reader *reader);
void br_bufio_reader_reset(br_bufio_reader *reader, br_stream source);

size_t br_bufio_reader_size(const br_bufio_reader *reader);
size_t br_bufio_reader_buffered(const br_bufio_reader *reader);

/*
Return the next `n` bytes without advancing the reader.

Like Odin's `bufio.reader_peek`, the returned view becomes invalid after the
next buffered reader operation. If fewer than `n` bytes are returned, `status`
explains why the result is short.
*/
br_bufio_reader_peek_result br_bufio_reader_peek(br_bufio_reader *reader, size_t n);

/*
Skip the next `n` bytes and report how many bytes were actually discarded.
*/
br_bufio_reader_io_result br_bufio_reader_discard(br_bufio_reader *reader, size_t n);

/*
Read into `dst`.

This follows Odin's `bufio.reader_read`: bytes come from at most one read of
the underlying stream, so a successful short read is allowed.
*/
br_bufio_reader_io_result br_bufio_reader_read(br_bufio_reader *reader, void *dst, size_t dst_len);
br_bufio_reader_byte_result br_bufio_reader_read_byte(br_bufio_reader *reader);
br_status br_bufio_reader_unread_byte(br_bufio_reader *reader);

/*
Read one UTF-8 rune from the buffered reader.

Invalid encodings consume one byte and return `BR_RUNE_ERROR` with width 1,
matching Odin's `bufio.reader_read_rune`.
*/
br_bufio_reader_rune_result br_bufio_reader_read_rune(br_bufio_reader *reader);
br_status br_bufio_reader_unread_rune(br_bufio_reader *reader);

/*
Read until `delim` is found and return a view into the internal buffer.

Like Odin's `bufio.reader_read_slice`, the returned view is invalidated by the
next buffered reader operation. `BR_STATUS_BUFFER_FULL` means the current
buffer filled before the delimiter was found.
*/
br_bufio_reader_slice_result br_bufio_reader_read_slice(br_bufio_reader *reader, uint8_t delim);

/*
Read until `delim` is found and return an owned byte slice.
*/
br_bytes_result
br_bufio_reader_read_bytes(br_bufio_reader *reader, uint8_t delim, br_allocator allocator);

/*
Read until `delim` is found and return an owned string.
*/
br_string_result
br_bufio_reader_read_string(br_bufio_reader *reader, uint8_t delim, br_allocator allocator);

/*
Write all remaining buffered and source bytes into `sink`.
*/
br_i64_result br_bufio_reader_write_to(br_bufio_reader *reader, br_stream sink);

/*
Expose this buffered reader through the generic stream interface.
*/
br_stream br_bufio_reader_as_stream(br_bufio_reader *reader);

static inline br_reader br_bufio_reader_as_reader(br_bufio_reader *reader) {
  return br_bufio_reader_as_stream(reader);
}

BR_EXTERN_C_END

#endif

/* ==== bedrock/bufio/writer.h ==== */
#ifndef BEDROCK_BUFIO_WRITER_H
#define BEDROCK_BUFIO_WRITER_H


BR_EXTERN_C_BEGIN

typedef struct br_bufio_writer {
  uint8_t *buf;
  size_t cap;
  size_t n;
  br_stream sink;
  br_allocator allocator;
  br_status err;
  bool owns_storage;
  size_t max_consecutive_empty_writes;
} br_bufio_writer;

typedef br_io_result br_bufio_writer_io_result;

/*
Initialize a heap-backed buffered writer using the default buffer size.
*/
br_status br_bufio_writer_init(br_bufio_writer *writer, br_stream sink, br_allocator allocator);

/*
Initialize a heap-backed buffered writer with an explicit buffer size.
*/
br_status br_bufio_writer_init_with_size(br_bufio_writer *writer,
                                         br_stream sink,
                                         size_t size,
                                         br_allocator allocator);

/*
Initialize a buffered writer over caller-provided storage.
*/
br_status br_bufio_writer_init_with_buffer(br_bufio_writer *writer,
                                           br_stream sink,
                                           void *buffer,
                                           size_t buffer_len);
void br_bufio_writer_destroy(br_bufio_writer *writer);
void br_bufio_writer_reset(br_bufio_writer *writer, br_stream sink);

size_t br_bufio_writer_size(const br_bufio_writer *writer);
size_t br_bufio_writer_available(const br_bufio_writer *writer);
size_t br_bufio_writer_buffered(const br_bufio_writer *writer);

/*
Flush buffered bytes into the underlying sink.
*/
br_status br_bufio_writer_flush(br_bufio_writer *writer);

/*
Write `src` into the buffered writer.

Like Odin's `bufio.writer_write`, this may bypass the internal buffer when the
buffer is empty and `src` is larger than the remaining buffer space.
*/
br_bufio_writer_io_result
br_bufio_writer_write(br_bufio_writer *writer, const void *src, size_t src_len);
br_status br_bufio_writer_write_byte(br_bufio_writer *writer, uint8_t value);
br_bufio_writer_io_result br_bufio_writer_write_rune(br_bufio_writer *writer, br_rune value);
br_bufio_writer_io_result br_bufio_writer_write_string(br_bufio_writer *writer, br_string_view s);

/*
Read from `source` into the buffered writer until EOF or error.

Unlike Odin's fast path, Bedrock currently always stages through the buffer
because generic streams do not expose a `READ_FROM` specialization mode yet.
*/
br_i64_result br_bufio_writer_read_from(br_bufio_writer *writer, br_stream source);

/*
Expose this buffered writer through the generic stream interface.
*/
br_stream br_bufio_writer_as_stream(br_bufio_writer *writer);

static inline br_writer br_bufio_writer_as_writer(br_bufio_writer *writer) {
  return br_bufio_writer_as_stream(writer);
}

static inline br_flusher br_bufio_writer_as_flusher(br_bufio_writer *writer) {
  return br_bufio_writer_as_stream(writer);
}

BR_EXTERN_C_END

#endif


BR_EXTERN_C_BEGIN

typedef struct br_bufio_read_writer {
  br_bufio_reader *reader;
  br_bufio_writer *writer;
} br_bufio_read_writer;

/*
Pair a buffered reader and writer into one stream-like value.
*/
void br_bufio_read_writer_init(br_bufio_read_writer *read_writer,
                               br_bufio_reader *reader,
                               br_bufio_writer *writer);

br_stream br_bufio_read_writer_as_stream(br_bufio_read_writer *read_writer);

BR_EXTERN_C_END

#endif


#endif

/* ==== bedrock/sync.h ==== */
#ifndef BEDROCK_SYNC_H
#define BEDROCK_SYNC_H

/* ==== bedrock/sync/extended.h ==== */
#ifndef BEDROCK_SYNC_EXTENDED_H
#define BEDROCK_SYNC_EXTENDED_H


BR_EXTERN_C_BEGIN

typedef struct br_wait_group {
  int32_t counter;
  br_mutex mutex;
  br_cond cond;
} br_wait_group;

typedef struct br_barrier {
  br_mutex mutex;
  br_cond cond;
  int32_t index;
  int32_t generation;
  int32_t thread_count;
} br_barrier;

typedef void (*br_once_fn)(void *ctx);
typedef void (*br_once_fn0)(void);

typedef struct br_once {
  br_mutex mutex;
  br_atomic_bool done;
} br_once;

/*
Represents a synchronization primitive that releases one waiting thread when
signalled, then resets automatically.
*/
typedef struct br_auto_reset_event {
  /*
  status ==  0: Event is reset and no threads are waiting.
  status ==  1: Event is signalled.
  status == -N: Event is reset and N threads are waiting.
  */
  br_atomic_i32 status;
  br_sema sema;
} br_auto_reset_event;

/*
A parker is a single-consumer token used to block a thread until another thread
makes the token available.
*/
typedef struct br_parker {
  br_futex state;
} br_parker;

/*
A one-shot event stays signalled once made available and wakes every waiter.
*/
typedef struct br_one_shot_event {
  br_futex state;
} br_one_shot_event;

typedef struct br_ticket_mutex {
  br_atomic_u32 ticket;
  br_atomic_u32 serving;
} br_ticket_mutex;

#define BR_WAIT_GROUP_INIT {.counter = 0, .mutex = BR_MUTEX_INIT, .cond = BR_COND_INIT}

#define BR_ONCE_INIT {.mutex = BR_MUTEX_INIT, .done = BR_ATOMIC_INIT(false)}

#define BR_AUTO_RESET_EVENT_INIT {.status = BR_ATOMIC_INIT(0), .sema = BR_SEMA_INIT(0u)}

#define BR_PARKER_INIT {.state = BR_FUTEX_INIT(0u)}

#define BR_ONE_SHOT_EVENT_INIT {.state = BR_FUTEX_INIT(0u)}

#define BR_TICKET_MUTEX_INIT                                                                       \
  {.ticket = BR_ATOMIC_INIT((uint32_t)0), .serving = BR_ATOMIC_INIT((uint32_t)0)}

br_status br_wait_group_init(br_wait_group *wg);
void br_wait_group_destroy(br_wait_group *wg);
void br_wait_group_add(br_wait_group *wg, int32_t delta);
void br_wait_group_done(br_wait_group *wg);
void br_wait_group_wait(br_wait_group *wg);
bool br_wait_group_wait_with_timeout(br_wait_group *wg, br_duration duration);

br_status br_barrier_init(br_barrier *barrier, int32_t thread_count);
void br_barrier_destroy(br_barrier *barrier);
bool br_barrier_wait(br_barrier *barrier);

br_status br_once_init(br_once *once);
void br_once_destroy(br_once *once);
void br_once_do(br_once *once, br_once_fn fn, void *ctx);
void br_once_do0(br_once *once, br_once_fn0 fn);

br_status br_auto_reset_event_init(br_auto_reset_event *event);
void br_auto_reset_event_destroy(br_auto_reset_event *event);
void br_auto_reset_event_signal(br_auto_reset_event *event);
void br_auto_reset_event_wait(br_auto_reset_event *event);

void br_parker_init(br_parker *parker);
void br_parker_park(br_parker *parker);
bool br_parker_park_with_timeout(br_parker *parker, br_duration duration);
void br_parker_unpark(br_parker *parker);

void br_one_shot_event_init(br_one_shot_event *event);
void br_one_shot_event_wait(br_one_shot_event *event);
void br_one_shot_event_signal(br_one_shot_event *event);

void br_ticket_mutex_init(br_ticket_mutex *mutex);
void br_ticket_mutex_lock(br_ticket_mutex *mutex);
void br_ticket_mutex_unlock(br_ticket_mutex *mutex);

BR_EXTERN_C_END

#endif

/* ==== bedrock/sync/sync_util.h ==== */
#ifndef BEDROCK_SYNC_SYNC_UTIL_H
#define BEDROCK_SYNC_SYNC_UTIL_H


#define br_lock(lock_ptr)                                                                          \
  _Generic((lock_ptr),                                                                             \
    br_mutex *: br_mutex_lock,                                                                     \
    br_rw_mutex *: br_rw_mutex_lock,                                                               \
    br_recursive_mutex *: br_recursive_mutex_lock,                                                 \
    br_ticket_mutex *: br_ticket_mutex_lock)(lock_ptr)

#define br_unlock(lock_ptr)                                                                        \
  _Generic((lock_ptr),                                                                             \
    br_mutex *: br_mutex_unlock,                                                                   \
    br_rw_mutex *: br_rw_mutex_unlock,                                                             \
    br_recursive_mutex *: br_recursive_mutex_unlock,                                               \
    br_ticket_mutex *: br_ticket_mutex_unlock)(lock_ptr)

#define br_try_lock(lock_ptr)                                                                      \
  _Generic((lock_ptr),                                                                             \
    br_mutex *: br_mutex_try_lock,                                                                 \
    br_rw_mutex *: br_rw_mutex_try_lock,                                                           \
    br_recursive_mutex *: br_recursive_mutex_try_lock)(lock_ptr)

#define br_shared_lock(lock_ptr) br_rw_mutex_shared_lock(lock_ptr)
#define br_shared_unlock(lock_ptr) br_rw_mutex_shared_unlock(lock_ptr)
#define br_try_shared_lock(lock_ptr) br_rw_mutex_try_shared_lock(lock_ptr)

/*
Odin's `guard` helpers rely on `defer`. Bedrock keeps the intent but exposes
them as scoped block macros because C has no equivalent built-in construct.
*/
#define br_guard(lock_ptr)                                                                         \
  for (bool BR_CONCAT(_br_guard_once_, __LINE__) = (br_lock((lock_ptr)), true);                    \
       BR_CONCAT(_br_guard_once_, __LINE__);                                                       \
       br_unlock((lock_ptr)), BR_CONCAT(_br_guard_once_, __LINE__) = false)

#define br_shared_guard(lock_ptr)                                                                  \
  for (bool BR_CONCAT(_br_shared_guard_once_, __LINE__) = (br_shared_lock((lock_ptr)), true);      \
       BR_CONCAT(_br_shared_guard_once_, __LINE__);                                                \
       br_shared_unlock((lock_ptr)), BR_CONCAT(_br_shared_guard_once_, __LINE__) = false)

#endif


#endif

/* ==== bedrock/time.h ==== */
#ifndef BEDROCK_TIME_H
#define BEDROCK_TIME_H


#endif

/* ==== bedrock/encoding.h ==== */
#ifndef BEDROCK_ENCODING_H
#define BEDROCK_ENCODING_H

/* ==== bedrock/encoding/encoding.h ==== */
#ifndef BEDROCK_ENCODING_ENCODING_H
#define BEDROCK_ENCODING_ENCODING_H


BR_EXTERN_C_BEGIN

/*
Result of an allocating decode.

`value` is owned by the caller and must be freed with `br_bytes_free`. On any
failure it is empty (`{NULL, 0}`); decoders that allocate before detecting a
fault free that scratch buffer before returning, so a non-OK status never hands
back a live allocation.

`error_offset` is the index of the first offending input byte when `status` is
`BR_STATUS_INVALID_ENCODING`, the count of input bytes consumed before the input
ran out when `BR_STATUS_UNEXPECTED_EOF`, and `0` (meaningless) otherwise.
*/
typedef struct br_decode_result {
  br_bytes value;
  size_t error_offset;
  br_status status;
} br_decode_result;

/*
Result of an into-buffer or to-stream decode.

`count` is the number of bytes written to the destination buffer or writer.
`error_offset` follows the same convention as `br_decode_result`.
*/
typedef struct br_decode_into_result {
  size_t count;
  size_t error_offset;
  br_status status;
} br_decode_into_result;

BR_EXTERN_C_END

#endif

/* ==== bedrock/encoding/hex.h ==== */
#ifndef BEDROCK_ENCODING_HEX_H
#define BEDROCK_ENCODING_HEX_H


BR_EXTERN_C_BEGIN

/*
Letter case for hex encoding. Decoding is always case-insensitive.
*/
typedef enum br_hex_case { BR_HEX_LOWER = 0, BR_HEX_UPPER } br_hex_case;

/*
Number of encoded bytes produced for `src_len` input bytes (`src_len * 2`).
*/
static inline size_t br_hex_encoded_len(size_t src_len) {
  return src_len * 2u;
}

/*
Number of decoded bytes produced by a well-formed `encoded_len`-byte input
(`encoded_len / 2`). Odd inputs are malformed; the decoders report that.
*/
static inline size_t br_hex_decoded_len(size_t encoded_len) {
  return encoded_len / 2u;
}

/*
Encode `src` as a hex sequence into a freshly allocated buffer.

The returned `value` is owned by the caller and must be freed with
`br_bytes_free`. Encoding cannot fail on content; only `BR_STATUS_OUT_OF_MEMORY`
(allocation failure) is possible. Empty input yields empty owned bytes with
`BR_STATUS_OK`.
*/
br_bytes_result br_hex_encode(br_bytes_view src, br_hex_case letter_case, br_allocator allocator);

/*
Encode `src` as a hex sequence into the caller buffer `dst` of capacity
`dst_cap`.

Returns the number of bytes written in `count`. `BR_STATUS_SHORT_BUFFER` if
`dst_cap` is smaller than `br_hex_encoded_len(src.len)`; `BR_STATUS_INVALID_ARGUMENT`
if `dst` is NULL while output is required.
*/
br_io_result
br_hex_encode_into(br_bytes_view src, br_hex_case letter_case, uint8_t *dst, size_t dst_cap);

/*
Encode `src` as a hex sequence to the writer `w`.

Returns the number of bytes written in `count`. Propagates the writer's status
on a short or failed write.
*/
br_io_result br_hex_encode_to_writer(br_bytes_view src, br_hex_case letter_case, br_writer w);

/*
Decode the hex sequence `src` into a freshly allocated buffer.

The returned `value` is owned by the caller and must be freed with
`br_bytes_free`. Decoding is case-insensitive. Odd-length input is malformed:
`BR_STATUS_INVALID_ENCODING` with `error_offset = src.len - 1`. Any byte outside
`[0-9a-fA-F]` yields `BR_STATUS_INVALID_ENCODING` with `error_offset` at that
byte's index. On any failure the scratch buffer is freed before returning, so
`value` is always empty when `status` is not `BR_STATUS_OK` (this fixes Odin's
decode-leak wart).
*/
br_decode_result br_hex_decode(br_bytes_view src, br_allocator allocator);

/*
Decode the hex sequence `src` into the caller buffer `dst` of capacity
`dst_cap`.

Returns the number of bytes written in `count`. Malformed input is reported the
same way as `br_hex_decode` (odd length, or a bad byte, both
`BR_STATUS_INVALID_ENCODING` with `error_offset`). `BR_STATUS_SHORT_BUFFER` if
`dst_cap` is smaller than `br_hex_decoded_len(src.len)`;
`BR_STATUS_INVALID_ARGUMENT` if `dst` is NULL while output is required.
*/
br_decode_into_result br_hex_decode_into(br_bytes_view src, uint8_t *dst, size_t dst_cap);

/*
Decode one byte from a two-character hex sequence, optionally prefixed with
`0x` or `0X`, e.g. `"23"`, `"0x23"`, or `"0X23"` all decode to `0x23`.

Returns the decoded byte in `value`. `BR_STATUS_INVALID_ENCODING` if, after
stripping any prefix, the input is not exactly two `[0-9a-fA-F]` characters.
*/
br_io_byte_result br_hex_decode_sequence(br_bytes_view seq);

BR_EXTERN_C_END

#endif

/* ==== bedrock/encoding/endian.h ==== */
#ifndef BEDROCK_ENCODING_ENDIAN_H
#define BEDROCK_ENCODING_ENDIAN_H

#include <string.h>


BR_EXTERN_C_BEGIN

/*
Fixed-width integer and float get/put with explicit byte order.

Integer paths assemble and disassemble values with shifts over individual
bytes, so they are agnostic to the host's endianness and never perform a wide
unaligned load -- there are no strict-aliasing or alignment concerns by
construction. Each byte is widened to the result type before it is shifted, so
the high byte never overflows a signed `int`. Float paths bit-cast through
`memcpy`, the portable, strict-aliasing-clean way to reinterpret between a
float and its bit pattern.

No `f16`: C11 has no `_Float16`, so 16-bit float support is deferred behind a
future feature gate (see spec/modules/encoding.md).

Each width offers four entry points, e.g. for `u16`:
- `br_endian_get_u16` / `br_endian_put_u16` -- bounds-checked over a
  `br_bytes_view` / `br_bytes`; return false when the buffer is too small (or a
  required pointer is NULL) without reading or writing any bytes.
- `br_endian_get_u16_unchecked` / `br_endian_put_u16_unchecked` -- fast paths
  over a raw pointer; the caller guarantees at least the width in bytes.
*/

BR_STATIC_ASSERT(sizeof(float) == 4, "br_endian f32 paths require a 32-bit float");
BR_STATIC_ASSERT(sizeof(double) == 8, "br_endian f64 paths require a 64-bit double");

typedef enum br_byte_order { BR_BYTE_ORDER_LITTLE = 0, BR_BYTE_ORDER_BIG } br_byte_order;

static inline br_byte_order br_byte_order_native(void) {
  uint16_t probe = 1u;
  uint8_t first;

  memcpy(&first, &probe, sizeof(first));
  return first != 0 ? BR_BYTE_ORDER_LITTLE : BR_BYTE_ORDER_BIG;
}

/* --- Unchecked get: caller guarantees the pointer spans at least the width. */

static inline uint16_t br_endian_get_u16_unchecked(const uint8_t *p, br_byte_order order) {
  if (order == BR_BYTE_ORDER_LITTLE) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
  }
  return (uint16_t)((uint16_t)p[1] | ((uint16_t)p[0] << 8));
}

static inline uint32_t br_endian_get_u32_unchecked(const uint8_t *p, br_byte_order order) {
  if (order == BR_BYTE_ORDER_LITTLE) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
  }
  return (uint32_t)p[3] | ((uint32_t)p[2] << 8) | ((uint32_t)p[1] << 16) | ((uint32_t)p[0] << 24);
}

static inline uint64_t br_endian_get_u64_unchecked(const uint8_t *p, br_byte_order order) {
  if (order == BR_BYTE_ORDER_LITTLE) {
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
  }
  return (uint64_t)p[7] | ((uint64_t)p[6] << 8) | ((uint64_t)p[5] << 16) | ((uint64_t)p[4] << 24) |
         ((uint64_t)p[3] << 32) | ((uint64_t)p[2] << 40) | ((uint64_t)p[1] << 48) |
         ((uint64_t)p[0] << 56);
}

static inline int16_t br_endian_get_i16_unchecked(const uint8_t *p, br_byte_order order) {
  return (int16_t)br_endian_get_u16_unchecked(p, order);
}

static inline int32_t br_endian_get_i32_unchecked(const uint8_t *p, br_byte_order order) {
  return (int32_t)br_endian_get_u32_unchecked(p, order);
}

static inline int64_t br_endian_get_i64_unchecked(const uint8_t *p, br_byte_order order) {
  return (int64_t)br_endian_get_u64_unchecked(p, order);
}

static inline float br_endian_get_f32_unchecked(const uint8_t *p, br_byte_order order) {
  uint32_t raw = br_endian_get_u32_unchecked(p, order);
  float value;

  memcpy(&value, &raw, sizeof(value));
  return value;
}

static inline double br_endian_get_f64_unchecked(const uint8_t *p, br_byte_order order) {
  uint64_t raw = br_endian_get_u64_unchecked(p, order);
  double value;

  memcpy(&value, &raw, sizeof(value));
  return value;
}

/* --- Unchecked put: caller guarantees the pointer spans at least the width. */

static inline void br_endian_put_u16_unchecked(uint8_t *p, br_byte_order order, uint16_t v) {
  if (order == BR_BYTE_ORDER_LITTLE) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
  } else {
    p[1] = (uint8_t)(v & 0xFFu);
    p[0] = (uint8_t)((v >> 8) & 0xFFu);
  }
}

static inline void br_endian_put_u32_unchecked(uint8_t *p, br_byte_order order, uint32_t v) {
  if (order == BR_BYTE_ORDER_LITTLE) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
  } else {
    p[3] = (uint8_t)(v & 0xFFu);
    p[2] = (uint8_t)((v >> 8) & 0xFFu);
    p[1] = (uint8_t)((v >> 16) & 0xFFu);
    p[0] = (uint8_t)((v >> 24) & 0xFFu);
  }
}

static inline void br_endian_put_u64_unchecked(uint8_t *p, br_byte_order order, uint64_t v) {
  if (order == BR_BYTE_ORDER_LITTLE) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
    p[4] = (uint8_t)((v >> 32) & 0xFFu);
    p[5] = (uint8_t)((v >> 40) & 0xFFu);
    p[6] = (uint8_t)((v >> 48) & 0xFFu);
    p[7] = (uint8_t)((v >> 56) & 0xFFu);
  } else {
    p[7] = (uint8_t)(v & 0xFFu);
    p[6] = (uint8_t)((v >> 8) & 0xFFu);
    p[5] = (uint8_t)((v >> 16) & 0xFFu);
    p[4] = (uint8_t)((v >> 24) & 0xFFu);
    p[3] = (uint8_t)((v >> 32) & 0xFFu);
    p[2] = (uint8_t)((v >> 40) & 0xFFu);
    p[1] = (uint8_t)((v >> 48) & 0xFFu);
    p[0] = (uint8_t)((v >> 56) & 0xFFu);
  }
}

static inline void br_endian_put_i16_unchecked(uint8_t *p, br_byte_order order, int16_t v) {
  br_endian_put_u16_unchecked(p, order, (uint16_t)v);
}

static inline void br_endian_put_i32_unchecked(uint8_t *p, br_byte_order order, int32_t v) {
  br_endian_put_u32_unchecked(p, order, (uint32_t)v);
}

static inline void br_endian_put_i64_unchecked(uint8_t *p, br_byte_order order, int64_t v) {
  br_endian_put_u64_unchecked(p, order, (uint64_t)v);
}

static inline void br_endian_put_f32_unchecked(uint8_t *p, br_byte_order order, float v) {
  uint32_t raw;

  memcpy(&raw, &v, sizeof(raw));
  br_endian_put_u32_unchecked(p, order, raw);
}

static inline void br_endian_put_f64_unchecked(uint8_t *p, br_byte_order order, double v) {
  uint64_t raw;

  memcpy(&raw, &v, sizeof(raw));
  br_endian_put_u64_unchecked(p, order, raw);
}

/* --- Bounds-checked get over a view; false (no read) if fewer than N bytes. */

static inline bool br_endian_get_u16(br_bytes_view b, br_byte_order order, uint16_t *out) {
  if (out == NULL || b.data == NULL || b.len < 2u) {
    return false;
  }
  *out = br_endian_get_u16_unchecked(b.data, order);
  return true;
}

static inline bool br_endian_get_u32(br_bytes_view b, br_byte_order order, uint32_t *out) {
  if (out == NULL || b.data == NULL || b.len < 4u) {
    return false;
  }
  *out = br_endian_get_u32_unchecked(b.data, order);
  return true;
}

static inline bool br_endian_get_u64(br_bytes_view b, br_byte_order order, uint64_t *out) {
  if (out == NULL || b.data == NULL || b.len < 8u) {
    return false;
  }
  *out = br_endian_get_u64_unchecked(b.data, order);
  return true;
}

static inline bool br_endian_get_i16(br_bytes_view b, br_byte_order order, int16_t *out) {
  uint16_t v;

  if (out == NULL || !br_endian_get_u16(b, order, &v)) {
    return false;
  }
  *out = (int16_t)v;
  return true;
}

static inline bool br_endian_get_i32(br_bytes_view b, br_byte_order order, int32_t *out) {
  uint32_t v;

  if (out == NULL || !br_endian_get_u32(b, order, &v)) {
    return false;
  }
  *out = (int32_t)v;
  return true;
}

static inline bool br_endian_get_i64(br_bytes_view b, br_byte_order order, int64_t *out) {
  uint64_t v;

  if (out == NULL || !br_endian_get_u64(b, order, &v)) {
    return false;
  }
  *out = (int64_t)v;
  return true;
}

static inline bool br_endian_get_f32(br_bytes_view b, br_byte_order order, float *out) {
  uint32_t v;

  if (out == NULL || !br_endian_get_u32(b, order, &v)) {
    return false;
  }
  memcpy(out, &v, sizeof(*out));
  return true;
}

static inline bool br_endian_get_f64(br_bytes_view b, br_byte_order order, double *out) {
  uint64_t v;

  if (out == NULL || !br_endian_get_u64(b, order, &v)) {
    return false;
  }
  memcpy(out, &v, sizeof(*out));
  return true;
}

/* --- Bounds-checked put; false (no write) if the destination is too small. */

static inline bool br_endian_put_u16(br_bytes dst, br_byte_order order, uint16_t v) {
  if (dst.data == NULL || dst.len < 2u) {
    return false;
  }
  br_endian_put_u16_unchecked(dst.data, order, v);
  return true;
}

static inline bool br_endian_put_u32(br_bytes dst, br_byte_order order, uint32_t v) {
  if (dst.data == NULL || dst.len < 4u) {
    return false;
  }
  br_endian_put_u32_unchecked(dst.data, order, v);
  return true;
}

static inline bool br_endian_put_u64(br_bytes dst, br_byte_order order, uint64_t v) {
  if (dst.data == NULL || dst.len < 8u) {
    return false;
  }
  br_endian_put_u64_unchecked(dst.data, order, v);
  return true;
}

static inline bool br_endian_put_i16(br_bytes dst, br_byte_order order, int16_t v) {
  return br_endian_put_u16(dst, order, (uint16_t)v);
}

static inline bool br_endian_put_i32(br_bytes dst, br_byte_order order, int32_t v) {
  return br_endian_put_u32(dst, order, (uint32_t)v);
}

static inline bool br_endian_put_i64(br_bytes dst, br_byte_order order, int64_t v) {
  return br_endian_put_u64(dst, order, (uint64_t)v);
}

static inline bool br_endian_put_f32(br_bytes dst, br_byte_order order, float v) {
  uint32_t raw;

  memcpy(&raw, &v, sizeof(raw));
  return br_endian_put_u32(dst, order, raw);
}

static inline bool br_endian_put_f64(br_bytes dst, br_byte_order order, double v) {
  uint64_t raw;

  memcpy(&raw, &v, sizeof(raw));
  return br_endian_put_u64(dst, order, raw);
}

BR_EXTERN_C_END

#endif


#endif


#endif

#ifdef BEDROCK_IMPLEMENTATION

/* Feature-test macros hoisted above every #include (H1). */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

/* ==== src/mem/virtual/internal.h ==== */
#ifndef BEDROCK_MEM_VIRTUAL_INTERNAL_H
#define BEDROCK_MEM_VIRTUAL_INTERNAL_H


#if defined(_WIN32)
#define BR__VM_TARGET_WINDOWS 1
#elif defined(__linux__)
#define BR__VM_TARGET_LINUX 1
#elif defined(__APPLE__)
#define BR__VM_TARGET_DARWIN 1
#define BR__VM_TARGET_POSIX 1
#elif defined(__FreeBSD__)
#define BR__VM_TARGET_FREEBSD 1
#define BR__VM_TARGET_POSIX 1
#elif defined(__NetBSD__)
#define BR__VM_TARGET_NETBSD 1
#define BR__VM_TARGET_POSIX 1
#elif defined(__OpenBSD__)
#define BR__VM_TARGET_OPENBSD 1
#define BR__VM_TARGET_POSIX 1
#else
#define BR__VM_TARGET_OTHER 1
#endif

struct br_virtual_arena_block {
  struct br_virtual_arena_block *prev;
  u8 *base;
  usize used;
  usize committed;
  usize reserved;
};

typedef struct br__vm_platform_memory_block {
  br_virtual_arena_block block;
  usize committed_total;
  usize reserved_total;
} br__vm_platform_memory_block;

typedef iptr br__vm_native_file;

br_vm_region_result br__vm_region_result(u8 *data, usize size, br_status status);
br_vm_mapped_file_result
br__vm_mapped_file_result(u8 *data, usize size, br_vm_map_file_error error);

usize br__vm_cached_page_size(void);
usize br__vm_platform_page_size_query(void);

br_vm_region_result br__vm_platform_reserve(usize size);
br_status br__vm_platform_commit(void *ptr, usize size);
void br__vm_platform_decommit(void *ptr, usize size);
void br__vm_platform_release(void *ptr, usize size);
bool br__vm_platform_protect(void *ptr, usize size, br_vm_protect_flags flags);

br_vm_mapped_file_result
br__vm_platform_map_open_file(br__vm_native_file file, usize size, br_vm_map_file_flags flags);
void br__vm_platform_unmap_file(br_vm_mapped_file mapping);

br__vm_platform_memory_block *
br__vm_platform_memory_alloc(usize to_commit, usize to_reserve, br_status *status);
void br__vm_platform_memory_free(br__vm_platform_memory_block *block);
br_status br__vm_platform_memory_commit(br__vm_platform_memory_block *block, usize to_commit);

#endif

/* ==== src/bufio/lookahead_reader.c ==== */

static br_bufio_lookahead_reader_peek_result
br__bufio_lookahead_reader_peek_result(br_bytes_view value, br_status status) {
  br_bufio_lookahead_reader_peek_result result;

  result.value = value;
  result.status = status;
  return result;
}

static void br__bufio_lookahead_reader_clear_state(br_bufio_lookahead_reader *reader,
                                                   br_stream source) {
  reader->source = source;
  reader->n = 0u;
}

br_status br_bufio_lookahead_reader_init(br_bufio_lookahead_reader *reader,
                                         br_stream source,
                                         br_allocator allocator) {
  return br_bufio_lookahead_reader_init_with_size(
    reader, source, BR_BUFIO_DEFAULT_BUFFER_SIZE, allocator);
}

br_status br_bufio_lookahead_reader_init_with_size(br_bufio_lookahead_reader *reader,
                                                   br_stream source,
                                                   usize size,
                                                   br_allocator allocator) {
  br_alloc_result alloc;

  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  memset(reader, 0, sizeof(*reader));
  if (size > 0u) {
    alloc = br_allocator_alloc_uninit(allocator, size, 1u);
    if (alloc.status != BR_STATUS_OK) {
      return alloc.status;
    }

    reader->buf = alloc.ptr;
    reader->owns_storage = true;
  }

  reader->cap = size;
  reader->allocator = allocator;
  br__bufio_lookahead_reader_clear_state(reader, source);
  return BR_STATUS_OK;
}

br_status br_bufio_lookahead_reader_init_with_buffer(br_bufio_lookahead_reader *reader,
                                                     br_stream source,
                                                     void *buffer,
                                                     usize buffer_len) {
  if (reader == NULL || (buffer == NULL && buffer_len > 0u)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  memset(reader, 0, sizeof(*reader));
  reader->buf = buffer;
  reader->cap = buffer_len;
  reader->allocator = br_allocator_null();
  reader->owns_storage = false;
  br__bufio_lookahead_reader_clear_state(reader, source);
  return BR_STATUS_OK;
}

void br_bufio_lookahead_reader_destroy(br_bufio_lookahead_reader *reader) {
  if (reader == NULL) {
    return;
  }

  if (reader->owns_storage && reader->buf != NULL) {
    (void)br_allocator_free(reader->allocator, reader->buf, reader->cap);
  }

  memset(reader, 0, sizeof(*reader));
}

void br_bufio_lookahead_reader_reset(br_bufio_lookahead_reader *reader, br_stream source) {
  if (reader == NULL) {
    return;
  }

  br__bufio_lookahead_reader_clear_state(reader, source);
}

br_bytes_view br_bufio_lookahead_reader_buffer(const br_bufio_lookahead_reader *reader) {
  if (reader == NULL || reader->n == 0u) {
    return br_bytes_view_make(NULL, 0u);
  }

  return br_bytes_view_make(reader->buf, reader->n);
}

br_bufio_lookahead_reader_peek_result
br_bufio_lookahead_reader_peek(br_bufio_lookahead_reader *reader, usize n) {
  br_status status;

  if (reader == NULL) {
    return br__bufio_lookahead_reader_peek_result(br_bytes_view_make(NULL, 0u),
                                                  BR_STATUS_INVALID_ARGUMENT);
  }
  if (n > reader->cap) {
    return br__bufio_lookahead_reader_peek_result(br_bytes_view_make(NULL, 0u),
                                                  BR_STATUS_BUFFER_FULL);
  }

  status = BR_STATUS_OK;
  if (reader->n < n) {
    br_io_result result;

    result = br_read_at_least(
      reader->source, reader->buf + reader->n, reader->cap - reader->n, n - reader->n);
    reader->n += result.count;
    if (result.status == BR_STATUS_UNEXPECTED_EOF) {
      status = BR_STATUS_EOF;
    } else {
      status = result.status;
    }
  }

  if (n > reader->n) {
    n = reader->n;
  }
  return br__bufio_lookahead_reader_peek_result(br_bytes_view_make(reader->buf, n), status);
}

br_bufio_lookahead_reader_peek_result
br_bufio_lookahead_reader_peek_all(br_bufio_lookahead_reader *reader) {
  if (reader == NULL) {
    return br__bufio_lookahead_reader_peek_result(br_bytes_view_make(NULL, 0u),
                                                  BR_STATUS_INVALID_ARGUMENT);
  }

  return br_bufio_lookahead_reader_peek(reader, reader->cap);
}

br_status br_bufio_lookahead_reader_consume(br_bufio_lookahead_reader *reader, usize n) {
  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (n > reader->n) {
    return BR_STATUS_SHORT_BUFFER;
  }
  if (n == 0u) {
    return BR_STATUS_OK;
  }

  memmove(reader->buf, reader->buf + n, reader->n - n);
  reader->n -= n;
  return BR_STATUS_OK;
}

br_status br_bufio_lookahead_reader_consume_all(br_bufio_lookahead_reader *reader) {
  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  reader->n = 0u;
  return BR_STATUS_OK;
}

/* ==== src/bufio/read_writer.c ==== */

void br_bufio_read_writer_init(br_bufio_read_writer *read_writer,
                               br_bufio_reader *reader,
                               br_bufio_writer *writer) {
  if (read_writer == NULL) {
    return;
  }

  read_writer->reader = reader;
  read_writer->writer = writer;
}

static br_i64_result br__bufio_read_writer_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  br_bufio_read_writer *read_writer;
  br_io_mode_set modes;
  br_bufio_reader_io_result read_result;
  br_bufio_writer_io_result write_result;
  br_status status;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  read_writer = (br_bufio_read_writer *)context;
  switch (mode) {
    case BR_IO_MODE_FLUSH:
      status = br_bufio_writer_flush(read_writer->writer);
      return br_i64_result_make(0, status);
    case BR_IO_MODE_READ:
      read_result = br_bufio_reader_read(read_writer->reader, data, data_len);
      return br_i64_result_make((i64)read_result.count, read_result.status);
    case BR_IO_MODE_WRITE:
      write_result = br_bufio_writer_write(read_writer->writer, data, data_len);
      return br_i64_result_make((i64)write_result.count, write_result.status);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_FLUSH) | br_io_mode_bit(BR_IO_MODE_READ) |
              br_io_mode_bit(BR_IO_MODE_WRITE);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_bufio_read_writer_as_stream(br_bufio_read_writer *read_writer) {
  return br_stream_make(read_writer, br__bufio_read_writer_stream_proc);
}

/* ==== src/bufio/reader.c ==== */

static br_bufio_reader_peek_result br__bufio_reader_peek_result(br_bytes_view value,
                                                                br_status status) {
  br_bufio_reader_peek_result result;

  result.value = value;
  result.status = status;
  return result;
}

static void br__bufio_reader_clear_state(br_bufio_reader *reader, br_stream source) {
  reader->source = source;
  reader->r = 0u;
  reader->w = 0u;
  reader->err = BR_STATUS_OK;
  reader->last_byte = -1;
  reader->last_rune_size = -1;
}

static void br__bufio_reader_transfer_to_front(br_bufio_reader *reader) {
  usize unread;

  if (reader->r == 0u) {
    return;
  }

  unread = reader->w - reader->r;
  if (unread > 0u) {
    memmove(reader->buf, reader->buf + reader->r, unread);
  }
  reader->r = 0u;
  reader->w = unread;
}

static br_status br__bufio_reader_fill(br_bufio_reader *reader) {
  usize attempts;

  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (reader->r > 0u) {
    br__bufio_reader_transfer_to_front(reader);
  }
  if (reader->w >= reader->cap) {
    reader->err = BR_STATUS_BUFFER_FULL;
    return BR_STATUS_OK;
  }

  attempts = reader->max_consecutive_empty_reads;
  if (attempts == 0u) {
    attempts = BR_BUFIO_DEFAULT_MAX_CONSECUTIVE_EMPTY_READS;
  }

  while (attempts > 0u) {
    br_io_result read_result;

    read_result = br_read(reader->source, reader->buf + reader->w, reader->cap - reader->w);
    if (read_result.count > 0u) {
      reader->w += read_result.count;
      if (read_result.status != BR_STATUS_OK) {
        reader->err = read_result.status;
      }
      return BR_STATUS_OK;
    }
    if (read_result.status != BR_STATUS_OK) {
      reader->err = read_result.status;
      return BR_STATUS_OK;
    }

    attempts -= 1u;
  }

  reader->err = BR_STATUS_NO_PROGRESS;
  return BR_STATUS_OK;
}

static br_status br__bufio_reader_consume_err(br_bufio_reader *reader) {
  br_status err;

  err = reader->err;
  reader->err = BR_STATUS_OK;
  return err;
}

static br_bytes br__bufio_take_byte_buffer(br_byte_buffer *buffer) {
  br_bytes bytes;

  bytes = br_bytes_make(buffer->data, buffer->len);
  buffer->data = NULL;
  buffer->len = 0u;
  buffer->cap = 0u;
  buffer->off = 0u;
  buffer->allocator = br_allocator_null();
  buffer->can_unread_byte = false;
  return bytes;
}

static br_i64_result br__bufio_reader_write_buffer(br_bufio_reader *reader, br_stream sink) {
  usize available;
  br_io_result result;

  available = br_bufio_reader_buffered(reader);
  result = br_write(sink, reader->buf + reader->r, available);
  reader->r += result.count;
  /*
  Odin's io.write already reports short writes as errors. Bedrock's generic
  br_write is looser, so bufio normalizes a short successful chunk here.
  */
  if (result.count < available && result.status == BR_STATUS_OK) {
    result.status = BR_STATUS_SHORT_WRITE;
  }
  return br_i64_result_make((i64)result.count, result.status);
}

br_status br_bufio_reader_init(br_bufio_reader *reader, br_stream source, br_allocator allocator) {
  return br_bufio_reader_init_with_size(reader, source, BR_BUFIO_DEFAULT_BUFFER_SIZE, allocator);
}

br_status br_bufio_reader_init_with_size(br_bufio_reader *reader,
                                         br_stream source,
                                         usize size,
                                         br_allocator allocator) {
  br_alloc_result alloc;

  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (size < BR_BUFIO_MIN_BUFFER_SIZE) {
    size = BR_BUFIO_MIN_BUFFER_SIZE;
  }

  memset(reader, 0, sizeof(*reader));
  alloc = br_allocator_alloc_uninit(allocator, size, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return alloc.status;
  }

  reader->buf = alloc.ptr;
  reader->cap = size;
  reader->allocator = allocator;
  reader->owns_storage = true;
  reader->max_consecutive_empty_reads = BR_BUFIO_DEFAULT_MAX_CONSECUTIVE_EMPTY_READS;
  br__bufio_reader_clear_state(reader, source);
  return BR_STATUS_OK;
}

br_status br_bufio_reader_init_with_buffer(br_bufio_reader *reader,
                                           br_stream source,
                                           void *buffer,
                                           usize buffer_len) {
  if (reader == NULL || buffer == NULL || buffer_len == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  memset(reader, 0, sizeof(*reader));
  reader->buf = buffer;
  reader->cap = buffer_len;
  reader->allocator = br_allocator_null();
  reader->owns_storage = false;
  reader->max_consecutive_empty_reads = BR_BUFIO_DEFAULT_MAX_CONSECUTIVE_EMPTY_READS;
  br__bufio_reader_clear_state(reader, source);
  return BR_STATUS_OK;
}

void br_bufio_reader_destroy(br_bufio_reader *reader) {
  if (reader == NULL) {
    return;
  }

  if (reader->owns_storage && reader->buf != NULL) {
    (void)br_allocator_free(reader->allocator, reader->buf, reader->cap);
  }

  memset(reader, 0, sizeof(*reader));
}

void br_bufio_reader_reset(br_bufio_reader *reader, br_stream source) {
  if (reader == NULL) {
    return;
  }

  br__bufio_reader_clear_state(reader, source);
}

usize br_bufio_reader_size(const br_bufio_reader *reader) {
  return reader != NULL ? reader->cap : 0u;
}

usize br_bufio_reader_buffered(const br_bufio_reader *reader) {
  if (reader == NULL || reader->w < reader->r) {
    return 0u;
  }

  return reader->w - reader->r;
}

br_bufio_reader_peek_result br_bufio_reader_peek(br_bufio_reader *reader, usize n) {
  usize available;
  br_status status;

  if (reader == NULL) {
    return br__bufio_reader_peek_result(br_bytes_view_make(NULL, 0u), BR_STATUS_INVALID_ARGUMENT);
  }

  reader->last_byte = -1;
  reader->last_rune_size = -1;

  while (br_bufio_reader_buffered(reader) < n && br_bufio_reader_buffered(reader) < reader->cap &&
         reader->err == BR_STATUS_OK) {
    br_status fill_status;

    fill_status = br__bufio_reader_fill(reader);
    if (fill_status != BR_STATUS_OK) {
      return br__bufio_reader_peek_result(br_bytes_view_make(NULL, 0u), fill_status);
    }
  }

  available = br_bufio_reader_buffered(reader);
  if (n > reader->cap) {
    return br__bufio_reader_peek_result(br_bytes_view_make(reader->buf + reader->r, available),
                                        BR_STATUS_BUFFER_FULL);
  }

  status = BR_STATUS_OK;
  if (available < n) {
    n = available;
    status = br__bufio_reader_consume_err(reader);
    if (status == BR_STATUS_OK) {
      status = BR_STATUS_BUFFER_FULL;
    }
  }

  return br__bufio_reader_peek_result(br_bytes_view_make(reader->buf + reader->r, n), status);
}

br_bufio_reader_io_result br_bufio_reader_discard(br_bufio_reader *reader, usize n) {
  usize remaining;
  usize discarded;

  if (reader == NULL) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (n == 0u) {
    return br_io_result_make(0u, BR_STATUS_OK);
  }

  reader->last_byte = -1;
  reader->last_rune_size = -1;
  remaining = n;
  discarded = 0u;
  for (;;) {
    usize skip;

    skip = br_bufio_reader_buffered(reader);
    if (skip == 0u) {
      if (reader->err != BR_STATUS_OK) {
        return br_io_result_make(discarded, br__bufio_reader_consume_err(reader));
      }
      if (br__bufio_reader_fill(reader) != BR_STATUS_OK) {
        return br_io_result_make(discarded, BR_STATUS_INVALID_STATE);
      }
      skip = br_bufio_reader_buffered(reader);
      if (skip == 0u && reader->err != BR_STATUS_OK) {
        return br_io_result_make(discarded, br__bufio_reader_consume_err(reader));
      }
    }

    skip = br_min_size(skip, remaining);
    reader->r += skip;
    discarded += skip;
    remaining -= skip;
    if (remaining == 0u) {
      return br_io_result_make(discarded, BR_STATUS_OK);
    }
  }
}

br_bufio_reader_io_result br_bufio_reader_read(br_bufio_reader *reader, void *dst, usize dst_len) {
  br_io_result read_result;
  usize copied;

  if (reader == NULL || (dst == NULL && dst_len > 0u)) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_len == 0u) {
    if (br_bufio_reader_buffered(reader) > 0u) {
      return br_io_result_make(0u, BR_STATUS_OK);
    }
    return br_io_result_make(0u, br__bufio_reader_consume_err(reader));
  }

  if (reader->r == reader->w) {
    if (reader->err != BR_STATUS_OK) {
      return br_io_result_make(0u, br__bufio_reader_consume_err(reader));
    }

    if (dst_len >= reader->cap) {
      read_result = br_read(reader->source, dst, dst_len);
      if (read_result.count > 0u) {
        reader->last_byte = ((const u8 *)dst)[read_result.count - 1u];
        reader->last_rune_size = -1;
      }
      return read_result;
    }

    reader->r = 0u;
    reader->w = 0u;
    read_result = br_read(reader->source, reader->buf, reader->cap);
    if (read_result.count == 0u) {
      return br_io_result_make(0u, read_result.status);
    }
    reader->w = read_result.count;
    if (read_result.status != BR_STATUS_OK) {
      reader->err = read_result.status;
    }
  }

  copied = br_min_size(dst_len, reader->w - reader->r);
  memcpy(dst, reader->buf + reader->r, copied);
  reader->r += copied;
  reader->last_byte = reader->buf[reader->r - 1u];
  reader->last_rune_size = -1;
  return br_io_result_make(copied, BR_STATUS_OK);
}

br_bufio_reader_byte_result br_bufio_reader_read_byte(br_bufio_reader *reader) {
  u8 byte_value;

  if (reader == NULL) {
    return br_io_byte_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  reader->last_rune_size = -1;
  while (reader->r == reader->w) {
    if (reader->err != BR_STATUS_OK) {
      return br_io_byte_result_make(0u, br__bufio_reader_consume_err(reader));
    }
    if (br__bufio_reader_fill(reader) != BR_STATUS_OK) {
      return br_io_byte_result_make(0u, BR_STATUS_INVALID_STATE);
    }
  }

  byte_value = reader->buf[reader->r];
  reader->r += 1u;
  reader->last_byte = byte_value;
  return br_io_byte_result_make(byte_value, BR_STATUS_OK);
}

br_status br_bufio_reader_unread_byte(br_bufio_reader *reader) {
  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (reader->last_byte < 0 || (reader->r == 0u && reader->w > 0u) || reader->cap == 0u) {
    return BR_STATUS_INVALID_STATE;
  }

  if (reader->r > 0u) {
    reader->r -= 1u;
  } else {
    reader->w = 1u;
  }
  reader->buf[reader->r] = (u8)reader->last_byte;
  reader->last_byte = -1;
  reader->last_rune_size = -1;
  return BR_STATUS_OK;
}

br_bufio_reader_rune_result br_bufio_reader_read_rune(br_bufio_reader *reader) {
  br_rune value;
  usize size;

  if (reader == NULL) {
    return br_io_rune_result_make(0, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  while (reader->r + BR_UTF8_MAX > reader->w &&
         !br_utf8_full_rune(br_bytes_view_make(reader->buf + reader->r, reader->w - reader->r)) &&
         reader->err == BR_STATUS_OK && br_bufio_reader_buffered(reader) < reader->cap) {
    if (br__bufio_reader_fill(reader) != BR_STATUS_OK) {
      return br_io_rune_result_make(0, 0u, BR_STATUS_INVALID_STATE);
    }
  }

  reader->last_rune_size = -1;
  if (reader->r == reader->w) {
    return br_io_rune_result_make(0, 0u, br__bufio_reader_consume_err(reader));
  }

  value = (br_rune)reader->buf[reader->r];
  size = 1u;
  if (value >= BR_RUNE_SELF) {
    br_utf8_decode_result decoded;

    decoded = br_utf8_decode(br_bytes_view_make(reader->buf + reader->r, reader->w - reader->r));
    value = decoded.value;
    size = decoded.width;
  }

  reader->r += size;
  reader->last_byte = reader->buf[reader->r - 1u];
  reader->last_rune_size = (isize)size;
  return br_io_rune_result_make(value, size, BR_STATUS_OK);
}

br_status br_bufio_reader_unread_rune(br_bufio_reader *reader) {
  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (reader->last_rune_size < 0 || reader->r < (usize)reader->last_rune_size) {
    return BR_STATUS_INVALID_STATE;
  }

  reader->r -= (usize)reader->last_rune_size;
  reader->last_byte = -1;
  reader->last_rune_size = -1;
  return BR_STATUS_OK;
}

br_bufio_reader_slice_result br_bufio_reader_read_slice(br_bufio_reader *reader, u8 delim) {
  usize search_from;

  if (reader == NULL) {
    return br__bufio_reader_peek_result(br_bytes_view_make(NULL, 0u), BR_STATUS_INVALID_ARGUMENT);
  }

  search_from = 0u;
  for (;;) {
    isize index;
    br_bytes_view line;
    br_status status;

    index = br_bytes_index_byte(br_bytes_view_make(reader->buf + reader->r + search_from,
                                                   reader->w - reader->r - search_from),
                                delim);
    if (index >= 0) {
      usize len;

      len = search_from + (usize)index + 1u;
      line = br_bytes_view_make(reader->buf + reader->r, len);
      reader->r += len;
      if (len > 0u) {
        reader->last_byte = line.data[len - 1u];
        reader->last_rune_size = -1;
      }
      return br__bufio_reader_peek_result(line, BR_STATUS_OK);
    }

    if (reader->err != BR_STATUS_OK) {
      line = br_bytes_view_make(reader->buf + reader->r, reader->w - reader->r);
      reader->r = reader->w;
      status = br__bufio_reader_consume_err(reader);
      if (line.len > 0u) {
        reader->last_byte = line.data[line.len - 1u];
        reader->last_rune_size = -1;
      }
      return br__bufio_reader_peek_result(line, status);
    }

    if (br_bufio_reader_buffered(reader) >= reader->cap) {
      line = br_bytes_view_make(reader->buf, reader->w);
      reader->r = reader->w;
      if (line.len > 0u) {
        reader->last_byte = line.data[line.len - 1u];
        reader->last_rune_size = -1;
      }
      return br__bufio_reader_peek_result(line, BR_STATUS_BUFFER_FULL);
    }

    search_from = br_bufio_reader_buffered(reader);
    if (br__bufio_reader_fill(reader) != BR_STATUS_OK) {
      return br__bufio_reader_peek_result(br_bytes_view_make(NULL, 0u), BR_STATUS_INVALID_STATE);
    }
  }
}

br_bytes_result
br_bufio_reader_read_bytes(br_bufio_reader *reader, u8 delim, br_allocator allocator) {
  br_byte_buffer buffer;
  br_status init_status;
  br_status final_status;

  if (reader == NULL) {
    return (br_bytes_result){br_bytes_make(NULL, 0u), BR_STATUS_INVALID_ARGUMENT};
  }

  br_byte_buffer_init(&buffer, allocator);
  final_status = BR_STATUS_OK;
  for (;;) {
    br_bufio_reader_slice_result slice_result;
    br_byte_buffer_io_result write_result;

    slice_result = br_bufio_reader_read_slice(reader, delim);
    if (slice_result.value.len > 0u) {
      write_result = br_byte_buffer_write(&buffer, slice_result.value);
      if (write_result.status != BR_STATUS_OK) {
        br_byte_buffer_destroy(&buffer);
        return (br_bytes_result){br_bytes_make(NULL, 0u), write_result.status};
      }
    }

    if (slice_result.status == BR_STATUS_BUFFER_FULL) {
      continue;
    }

    final_status = slice_result.status;
    break;
  }

  init_status = final_status;
  return (br_bytes_result){br__bufio_take_byte_buffer(&buffer), init_status};
}

br_string_result
br_bufio_reader_read_string(br_bufio_reader *reader, u8 delim, br_allocator allocator) {
  br_bytes_result bytes_result;

  bytes_result = br_bufio_reader_read_bytes(reader, delim, allocator);
  return (br_string_result){br_string_make(bytes_result.value.data, bytes_result.value.len),
                            bytes_result.status};
}

br_i64_result br_bufio_reader_write_to(br_bufio_reader *reader, br_stream sink) {
  br_i64_result written_result;
  i64 total;

  if (reader == NULL) {
    return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }

  written_result = br__bufio_reader_write_buffer(reader, sink);
  total = written_result.value;
  if (written_result.status != BR_STATUS_OK) {
    return br_i64_result_make(total, written_result.status);
  }

  if (br_bufio_reader_buffered(reader) < reader->cap) {
    br_status fill_status;

    fill_status = br__bufio_reader_fill(reader);
    if (fill_status != BR_STATUS_OK) {
      return br_i64_result_make(total, fill_status);
    }
  }

  while (reader->r < reader->w) {
    br_status fill_status;

    written_result = br__bufio_reader_write_buffer(reader, sink);
    total += written_result.value;
    if (written_result.status != BR_STATUS_OK) {
      return br_i64_result_make(total, written_result.status);
    }

    fill_status = br__bufio_reader_fill(reader);
    if (fill_status != BR_STATUS_OK) {
      return br_i64_result_make(total, fill_status);
    }
  }

  if (reader->err == BR_STATUS_EOF) {
    reader->err = BR_STATUS_OK;
  }

  return br_i64_result_make(total, br__bufio_reader_consume_err(reader));
}

static br_i64_result br__bufio_reader_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  br_bufio_reader *reader;
  br_io_mode_set modes;
  br_bufio_reader_io_result io_result;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  reader = (br_bufio_reader *)context;
  switch (mode) {
    case BR_IO_MODE_READ:
      io_result = br_bufio_reader_read(reader, data, data_len);
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_DESTROY:
      br_bufio_reader_destroy(reader);
      return br_i64_result_make(0, BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_READ) | br_io_mode_bit(BR_IO_MODE_DESTROY);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_bufio_reader_as_stream(br_bufio_reader *reader) {
  return br_stream_make(reader, br__bufio_reader_stream_proc);
}

/* ==== src/bufio/writer.c ==== */

static void br__bufio_writer_clear_state(br_bufio_writer *writer, br_stream sink) {
  writer->sink = sink;
  writer->n = 0u;
  writer->err = BR_STATUS_OK;
}

br_status br_bufio_writer_init(br_bufio_writer *writer, br_stream sink, br_allocator allocator) {
  return br_bufio_writer_init_with_size(writer, sink, BR_BUFIO_DEFAULT_BUFFER_SIZE, allocator);
}

br_status br_bufio_writer_init_with_size(br_bufio_writer *writer,
                                         br_stream sink,
                                         usize size,
                                         br_allocator allocator) {
  br_alloc_result alloc;

  if (writer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (size < BR_BUFIO_MIN_BUFFER_SIZE) {
    size = BR_BUFIO_MIN_BUFFER_SIZE;
  }

  memset(writer, 0, sizeof(*writer));
  alloc = br_allocator_alloc_uninit(allocator, size, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return alloc.status;
  }

  writer->buf = alloc.ptr;
  writer->cap = size;
  writer->allocator = allocator;
  writer->owns_storage = true;
  br__bufio_writer_clear_state(writer, sink);
  return BR_STATUS_OK;
}

br_status br_bufio_writer_init_with_buffer(br_bufio_writer *writer,
                                           br_stream sink,
                                           void *buffer,
                                           usize buffer_len) {
  if (writer == NULL || buffer == NULL || buffer_len == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  memset(writer, 0, sizeof(*writer));
  writer->buf = buffer;
  writer->cap = buffer_len;
  writer->allocator = br_allocator_null();
  writer->owns_storage = false;
  br__bufio_writer_clear_state(writer, sink);
  return BR_STATUS_OK;
}

void br_bufio_writer_destroy(br_bufio_writer *writer) {
  if (writer == NULL) {
    return;
  }

  if (writer->owns_storage && writer->buf != NULL) {
    (void)br_allocator_free(writer->allocator, writer->buf, writer->cap);
  }

  memset(writer, 0, sizeof(*writer));
}

void br_bufio_writer_reset(br_bufio_writer *writer, br_stream sink) {
  if (writer == NULL) {
    return;
  }

  br__bufio_writer_clear_state(writer, sink);
}

usize br_bufio_writer_size(const br_bufio_writer *writer) {
  return writer != NULL ? writer->cap : 0u;
}

usize br_bufio_writer_available(const br_bufio_writer *writer) {
  if (writer == NULL || writer->cap < writer->n) {
    return 0u;
  }

  return writer->cap - writer->n;
}

usize br_bufio_writer_buffered(const br_bufio_writer *writer) {
  return writer != NULL ? writer->n : 0u;
}

br_status br_bufio_writer_flush(br_bufio_writer *writer) {
  br_io_result result;

  if (writer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (writer->err != BR_STATUS_OK) {
    return writer->err;
  }
  if (writer->n == 0u) {
    return BR_STATUS_OK;
  }

  result = br_write(writer->sink, writer->buf, writer->n);
  if (result.count > writer->n) {
    writer->err = BR_STATUS_INVALID_STATE;
    return BR_STATUS_INVALID_STATE;
  }
  if (result.count < writer->n && result.status == BR_STATUS_OK) {
    result.status = BR_STATUS_SHORT_WRITE;
  }
  if (result.status != BR_STATUS_OK) {
    if (result.count > 0u && result.count < writer->n) {
      memmove(writer->buf, writer->buf + result.count, writer->n - result.count);
    }
    writer->n -= result.count;
    writer->err = result.status;
    return result.status;
  }

  writer->n = 0u;
  return BR_STATUS_OK;
}

br_bufio_writer_io_result
br_bufio_writer_write(br_bufio_writer *writer, const void *src, usize src_len) {
  const u8 *cursor;
  usize written;

  if (writer == NULL || (src == NULL && src_len > 0u)) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (writer->err != BR_STATUS_OK) {
    return br_io_result_make(0u, writer->err);
  }

  cursor = src;
  written = 0u;
  while (src_len > br_bufio_writer_available(writer) && writer->err == BR_STATUS_OK) {
    usize moved;

    if (br_bufio_writer_buffered(writer) == 0u) {
      br_io_result result;

      result = br_write(writer->sink, cursor, src_len);
      if (result.count > src_len) {
        writer->err = BR_STATUS_INVALID_STATE;
        break;
      }
      if (result.count < src_len && result.status == BR_STATUS_OK) {
        writer->err = BR_STATUS_SHORT_WRITE;
      } else if (result.status != BR_STATUS_OK) {
        writer->err = result.status;
      }

      written += result.count;
      cursor += result.count;
      src_len -= result.count;
      if (writer->err != BR_STATUS_OK) {
        break;
      }
    } else {
      moved = br_bufio_writer_available(writer);
      memcpy(writer->buf + writer->n, cursor, moved);
      writer->n += moved;
      written += moved;
      cursor += moved;
      src_len -= moved;
      (void)br_bufio_writer_flush(writer);
    }
  }

  if (writer->err != BR_STATUS_OK) {
    return br_io_result_make(written, writer->err);
  }

  if (src_len > 0u) {
    memcpy(writer->buf + writer->n, cursor, src_len);
    writer->n += src_len;
    written += src_len;
  }
  return br_io_result_make(written, BR_STATUS_OK);
}

br_status br_bufio_writer_write_byte(br_bufio_writer *writer, u8 value) {
  if (writer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (writer->err != BR_STATUS_OK) {
    return writer->err;
  }
  if (br_bufio_writer_available(writer) == 0u && br_bufio_writer_flush(writer) != BR_STATUS_OK) {
    return writer->err;
  }

  writer->buf[writer->n] = value;
  writer->n += 1u;
  return BR_STATUS_OK;
}

br_bufio_writer_io_result br_bufio_writer_write_rune(br_bufio_writer *writer, br_rune value) {
  br_utf8_encode_result encoded;

  if (writer == NULL) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  if (value < BR_RUNE_SELF) {
    br_status status;

    status = br_bufio_writer_write_byte(writer, (u8)value);
    if (status != BR_STATUS_OK) {
      return br_io_result_make(0u, status);
    }
    return br_io_result_make(1u, BR_STATUS_OK);
  }
  if (writer->err != BR_STATUS_OK) {
    return br_io_result_make(0u, writer->err);
  }

  if (br_bufio_writer_available(writer) < BR_UTF8_MAX) {
    br_status status;

    status = br_bufio_writer_flush(writer);
    if (status != BR_STATUS_OK) {
      return br_io_result_make(0u, status);
    }
    if (br_bufio_writer_available(writer) < BR_UTF8_MAX) {
      encoded = br_utf8_encode(value);
      return br_bufio_writer_write(writer, encoded.bytes, encoded.len);
    }
  }

  encoded = br_utf8_encode(value);
  memcpy(writer->buf + writer->n, encoded.bytes, encoded.len);
  writer->n += encoded.len;
  return br_io_result_make(encoded.len, BR_STATUS_OK);
}

br_bufio_writer_io_result br_bufio_writer_write_string(br_bufio_writer *writer, br_string_view s) {
  return br_bufio_writer_write(writer, s.data, s.len);
}

br_i64_result br_bufio_writer_read_from(br_bufio_writer *writer, br_stream source) {
  i64 total;
  br_status status;

  if (writer == NULL) {
    return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }
  if (writer->err != BR_STATUS_OK) {
    return br_i64_result_make(0, writer->err);
  }

  /*
  Odin can bypass buffering when the underlying writer exposes io.read_from.
  Bedrock's current stream API has no READ_FROM specialization mode, so this
  helper always stages through the buffered writer itself.
  */
  total = 0;
  status = BR_STATUS_OK;
  for (;;) {
    usize empty_limit;
    usize empty_reads;

    if (br_bufio_writer_available(writer) == 0u) {
      status = br_bufio_writer_flush(writer);
      if (status != BR_STATUS_OK) {
        return br_i64_result_make(total, status);
      }
    }

    empty_limit = writer->max_consecutive_empty_writes;
    if (empty_limit == 0u) {
      empty_limit = BR_BUFIO_DEFAULT_MAX_CONSECUTIVE_EMPTY_WRITES;
    }

    empty_reads = 0u;
    for (;;) {
      br_io_result read_result;

      read_result = br_read(source, writer->buf + writer->n, br_bufio_writer_available(writer));
      if (read_result.count > 0u || read_result.status != BR_STATUS_OK) {
        writer->n += read_result.count;
        total += (i64)read_result.count;
        status = read_result.status;
        break;
      }

      empty_reads += 1u;
      if (empty_reads >= empty_limit) {
        return br_i64_result_make(total, BR_STATUS_NO_PROGRESS);
      }
    }

    if (status != BR_STATUS_OK) {
      break;
    }
  }

  if (status == BR_STATUS_EOF) {
    if (br_bufio_writer_available(writer) == 0u) {
      status = br_bufio_writer_flush(writer);
    } else {
      status = BR_STATUS_OK;
    }
  }

  return br_i64_result_make(total, status);
}

static br_i64_result br__bufio_writer_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  br_bufio_writer *writer;
  br_io_mode_set modes;
  br_bufio_writer_io_result io_result;
  br_status status;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  writer = (br_bufio_writer *)context;
  switch (mode) {
    case BR_IO_MODE_FLUSH:
      status = br_bufio_writer_flush(writer);
      return br_i64_result_make(0, status);
    case BR_IO_MODE_WRITE:
      io_result = br_bufio_writer_write(writer, data, data_len);
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_DESTROY:
      br_bufio_writer_destroy(writer);
      return br_i64_result_make(0, BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_FLUSH) | br_io_mode_bit(BR_IO_MODE_WRITE) |
              br_io_mode_bit(BR_IO_MODE_DESTROY);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_bufio_writer_as_stream(br_bufio_writer *writer) {
  return br_stream_make(writer, br__bufio_writer_stream_proc);
}

/* ==== src/bytes/buffer.c ==== */

enum { BR__BYTE_BUFFER_MIN_CAPACITY = 64 };

static br_byte_buffer_io_result br__byte_buffer_io_result(usize count, br_status status) {
  br_byte_buffer_io_result result;

  result.count = count;
  result.status = status;
  return result;
}

static br_byte_buffer_byte_result br__byte_buffer_byte_result(u8 value, br_status status) {
  br_byte_buffer_byte_result result;

  result.value = value;
  result.status = status;
  return result;
}

static void br__byte_buffer_clear_state(br_byte_buffer *buffer) {
  buffer->len = 0u;
  buffer->off = 0u;
  buffer->can_unread_byte = false;
}

static usize br__byte_buffer_unread_len(const br_byte_buffer *buffer) {
  return buffer->len - buffer->off;
}

static void br__byte_buffer_compact(br_byte_buffer *buffer) {
  usize unread_len;

  if (buffer == NULL || buffer->off == 0u) {
    return;
  }

  unread_len = br__byte_buffer_unread_len(buffer);
  if (unread_len > 0u) {
    memmove(buffer->data, buffer->data + buffer->off, unread_len);
  }
  buffer->off = 0u;
  buffer->len = unread_len;
}

static br_status br__byte_buffer_grow(br_byte_buffer *buffer, usize additional) {
  br_alloc_result resized;
  usize unread_len;
  usize target_len;
  usize new_cap;

  if (buffer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (additional == 0u) {
    return BR_STATUS_OK;
  }

  unread_len = br__byte_buffer_unread_len(buffer);
  if (unread_len == 0u && buffer->off != 0u) {
    br__byte_buffer_clear_state(buffer);
  }

  if (additional <= buffer->cap - buffer->len) {
    return BR_STATUS_OK;
  }

  if (br__byte_buffer_unread_len(buffer) + additional < br__byte_buffer_unread_len(buffer)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  unread_len = br__byte_buffer_unread_len(buffer);
  target_len = unread_len + additional;

  if (target_len <= buffer->cap) {
    br__byte_buffer_compact(buffer);
    return BR_STATUS_OK;
  }

  new_cap = buffer->cap;
  if (new_cap < BR__BYTE_BUFFER_MIN_CAPACITY) {
    new_cap = BR__BYTE_BUFFER_MIN_CAPACITY;
  }
  while (new_cap < target_len) {
    usize doubled = new_cap * 2u;
    if (doubled <= new_cap) {
      new_cap = target_len;
      break;
    }
    new_cap = doubled;
  }

  if (buffer->data == NULL) {
    resized = br_allocator_alloc_uninit(buffer->allocator, new_cap, 1u);
    if (resized.status != BR_STATUS_OK) {
      return resized.status;
    }
  } else {
    br__byte_buffer_compact(buffer);
    resized = br_allocator_resize_uninit(buffer->allocator, buffer->data, buffer->cap, new_cap, 1u);
    if (resized.status != BR_STATUS_OK) {
      return resized.status;
    }
  }

  buffer->data = resized.ptr;
  buffer->cap = new_cap;
  return BR_STATUS_OK;
}

void br_byte_buffer_init(br_byte_buffer *buffer, br_allocator allocator) {
  if (buffer == NULL) {
    return;
  }

  buffer->data = NULL;
  buffer->cap = 0u;
  buffer->allocator = allocator;
  br__byte_buffer_clear_state(buffer);
}

br_status br_byte_buffer_init_copy(br_byte_buffer *buffer,
                                   br_bytes_view initial_data,
                                   br_allocator allocator) {
  br_alloc_result alloc;

  if (buffer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  br_byte_buffer_init(buffer, allocator);
  if (initial_data.len == 0u) {
    return BR_STATUS_OK;
  }

  alloc = br_allocator_alloc_uninit(allocator, initial_data.len, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return alloc.status;
  }

  memcpy(alloc.ptr, initial_data.data, initial_data.len);
  buffer->data = alloc.ptr;
  buffer->len = initial_data.len;
  buffer->cap = initial_data.len;
  return BR_STATUS_OK;
}

void br_byte_buffer_destroy(br_byte_buffer *buffer) {
  if (buffer == NULL) {
    return;
  }

  if (buffer->data != NULL) {
    (void)br_allocator_free(buffer->allocator, buffer->data, buffer->cap);
  }

  buffer->data = NULL;
  buffer->cap = 0u;
  br__byte_buffer_clear_state(buffer);
}

void br_byte_buffer_reset(br_byte_buffer *buffer) {
  if (buffer == NULL) {
    return;
  }

  br__byte_buffer_clear_state(buffer);
}

bool br_byte_buffer_is_empty(const br_byte_buffer *buffer) {
  return buffer == NULL || br__byte_buffer_unread_len(buffer) == 0u;
}

usize br_byte_buffer_len(const br_byte_buffer *buffer) {
  if (buffer == NULL) {
    return 0u;
  }

  return br__byte_buffer_unread_len(buffer);
}

usize br_byte_buffer_capacity(const br_byte_buffer *buffer) {
  return buffer != NULL ? buffer->cap : 0u;
}

br_bytes_view br_byte_buffer_view(const br_byte_buffer *buffer) {
  if (buffer == NULL || br__byte_buffer_unread_len(buffer) == 0u) {
    return br_bytes_view_make(NULL, 0u);
  }

  return br_bytes_view_make(buffer->data + buffer->off, br__byte_buffer_unread_len(buffer));
}

br_status br_byte_buffer_reserve(br_byte_buffer *buffer, usize additional) {
  return br__byte_buffer_grow(buffer, additional);
}

br_status br_byte_buffer_truncate(br_byte_buffer *buffer, usize n) {
  if (buffer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (n > br__byte_buffer_unread_len(buffer)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  buffer->len = buffer->off + n;
  buffer->can_unread_byte = false;
  if (n == 0u && buffer->off == buffer->len) {
    br__byte_buffer_clear_state(buffer);
  }
  return BR_STATUS_OK;
}

br_byte_buffer_io_result br_byte_buffer_write(br_byte_buffer *buffer, br_bytes_view src) {
  br_status status;

  if (buffer == NULL) {
    return br__byte_buffer_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (src.len == 0u) {
    buffer->can_unread_byte = false;
    return br__byte_buffer_io_result(0u, BR_STATUS_OK);
  }

  status = br__byte_buffer_grow(buffer, src.len);
  if (status != BR_STATUS_OK) {
    return br__byte_buffer_io_result(0u, status);
  }

  memcpy(buffer->data + buffer->len, src.data, src.len);
  buffer->len += src.len;
  buffer->can_unread_byte = false;
  return br__byte_buffer_io_result(src.len, BR_STATUS_OK);
}

br_status br_byte_buffer_write_byte(br_byte_buffer *buffer, u8 byte_value) {
  br_status status;

  if (buffer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  status = br__byte_buffer_grow(buffer, 1u);
  if (status != BR_STATUS_OK) {
    return status;
  }

  buffer->data[buffer->len] = byte_value;
  buffer->len += 1u;
  buffer->can_unread_byte = false;
  return BR_STATUS_OK;
}

br_bytes_view br_byte_buffer_next(br_byte_buffer *buffer, usize n) {
  usize unread_len;
  br_bytes_view result;

  if (buffer == NULL) {
    return br_bytes_view_make(NULL, 0u);
  }

  unread_len = br__byte_buffer_unread_len(buffer);
  if (n > unread_len) {
    n = unread_len;
  }

  result = br_bytes_view_make(buffer->data + buffer->off, n);
  buffer->off += n;
  buffer->can_unread_byte = false;
  if (buffer->off == buffer->len) {
    br__byte_buffer_clear_state(buffer);
  }
  return result;
}

br_byte_buffer_io_result br_byte_buffer_read(br_byte_buffer *buffer, void *dst, usize dst_len) {
  usize unread_len;
  usize count;

  if (buffer == NULL || (dst == NULL && dst_len > 0u)) {
    return br__byte_buffer_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_len == 0u) {
    return br__byte_buffer_io_result(0u, BR_STATUS_OK);
  }

  unread_len = br__byte_buffer_unread_len(buffer);
  if (unread_len == 0u) {
    br__byte_buffer_clear_state(buffer);
    return br__byte_buffer_io_result(0u, BR_STATUS_EOF);
  }

  count = br_min_size(unread_len, dst_len);
  memcpy(dst, buffer->data + buffer->off, count);
  buffer->off += count;
  buffer->can_unread_byte = count > 0u;
  return br__byte_buffer_io_result(count, BR_STATUS_OK);
}

br_byte_buffer_byte_result br_byte_buffer_read_byte(br_byte_buffer *buffer) {
  u8 byte_value;

  if (buffer == NULL) {
    return br__byte_buffer_byte_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (br__byte_buffer_unread_len(buffer) == 0u) {
    br__byte_buffer_clear_state(buffer);
    return br__byte_buffer_byte_result(0u, BR_STATUS_EOF);
  }

  byte_value = buffer->data[buffer->off];
  buffer->off += 1u;
  buffer->can_unread_byte = true;
  if (buffer->off == buffer->len) {
    buffer->off = buffer->len;
  }
  return br__byte_buffer_byte_result(byte_value, BR_STATUS_OK);
}

br_status br_byte_buffer_unread_byte(br_byte_buffer *buffer) {
  if (buffer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (!buffer->can_unread_byte || buffer->off == 0u) {
    return BR_STATUS_INVALID_STATE;
  }

  buffer->off -= 1u;
  buffer->can_unread_byte = false;
  return BR_STATUS_OK;
}

static br_i64_result br__byte_buffer_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  br_byte_buffer *buffer;
  br_byte_buffer_io_result io_result;
  br_io_mode_set modes;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  buffer = (br_byte_buffer *)context;
  switch (mode) {
    case BR_IO_MODE_READ:
      io_result = br_byte_buffer_read(buffer, data, data_len);
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_WRITE:
      io_result = br_byte_buffer_write(buffer, br_bytes_view_make(data, data_len));
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_SIZE:
      return br_i64_result_make((i64)br_byte_buffer_len(buffer), BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_READ) | br_io_mode_bit(BR_IO_MODE_WRITE) |
              br_io_mode_bit(BR_IO_MODE_SIZE);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_byte_buffer_as_stream(br_byte_buffer *buffer) {
  return br_stream_make(buffer, br__byte_buffer_stream_proc);
}

/* ==== src/bytes/bytes.c ==== */

static br_bytes_result br__bytes_result(void *data, usize len, br_status status) {
  br_bytes_result result;

  result.value = br_bytes_make(data, len);
  result.status = status;
  return result;
}

static br_bytes_view_list_result
br__bytes_view_list_result(br_bytes_view *data, usize len, br_status status) {
  br_bytes_view_list_result result;

  result.value.data = data;
  result.value.len = len;
  result.status = status;
  return result;
}

static br_bytes_rewrite_result br__bytes_rewrite_alias_result(br_bytes_view value) {
  br_bytes_rewrite_result result;

  result.value = value;
  result.owned = br_bytes_make(NULL, 0u);
  result.allocated = false;
  result.status = BR_STATUS_OK;
  return result;
}

static br_bytes_rewrite_result
br__bytes_rewrite_owned_result(void *data, usize len, br_status status) {
  br_bytes_rewrite_result result;

  result.owned = br_bytes_make(data, len);
  result.value = br_bytes_view_from_bytes(result.owned);
  result.allocated = status == BR_STATUS_OK;
  result.status = status;
  return result;
}

static bool br__bytes_contains_byte(br_bytes_view chars, u8 byte_value) {
  for (usize i = 0; i < chars.len; ++i) {
    if (chars.data[i] == byte_value) {
      return true;
    }
  }

  return false;
}

static bool br__bytes_add_overflow(usize lhs, usize rhs, usize *out) {
  if (lhs > SIZE_MAX - rhs) {
    return true;
  }

  *out = lhs + rhs;
  return false;
}

br_status br_bytes_free(br_bytes bytes, br_allocator allocator) {
  return br_allocator_free(allocator, bytes.data, bytes.len);
}

br_status br_bytes_view_list_free(br_bytes_view_list list, br_allocator allocator) {
  return br_allocator_free(allocator, list.data, list.len * sizeof(br_bytes_view));
}

br_status br_bytes_rewrite_free(br_bytes_rewrite_result result, br_allocator allocator) {
  if (!result.allocated) {
    return BR_STATUS_OK;
  }
  return br_bytes_free(result.owned, allocator);
}

br_bytes_result br_bytes_clone(br_bytes_view src, br_allocator allocator) {
  br_alloc_result alloc;

  if (src.len == 0u) {
    return br__bytes_result(NULL, 0u, BR_STATUS_OK);
  }

  alloc = br_allocator_alloc_uninit(allocator, src.len, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__bytes_result(NULL, 0u, alloc.status);
  }

  memcpy(alloc.ptr, src.data, src.len);
  return br__bytes_result(alloc.ptr, src.len, BR_STATUS_OK);
}

i32 br_bytes_compare(br_bytes_view lhs, br_bytes_view rhs) {
  usize common_len = br_min_size(lhs.len, rhs.len);
  i32 cmp;

  if (common_len == 0u) {
    if (lhs.len == rhs.len) {
      return 0;
    }
    return lhs.len < rhs.len ? -1 : 1;
  }

  cmp = memcmp(lhs.data, rhs.data, common_len);
  if (cmp < 0) {
    return -1;
  }
  if (cmp > 0) {
    return 1;
  }
  if (lhs.len == rhs.len) {
    return 0;
  }
  return lhs.len < rhs.len ? -1 : 1;
}

bool br_bytes_equal(br_bytes_view lhs, br_bytes_view rhs) {
  if (lhs.len != rhs.len) {
    return false;
  }
  if (lhs.len == 0u) {
    return true;
  }
  return memcmp(lhs.data, rhs.data, lhs.len) == 0;
}

bool br_bytes_has_prefix(br_bytes_view s, br_bytes_view prefix) {
  if (prefix.len > s.len) {
    return false;
  }
  if (prefix.len == 0u) {
    return true;
  }
  return memcmp(s.data, prefix.data, prefix.len) == 0;
}

bool br_bytes_has_suffix(br_bytes_view s, br_bytes_view suffix) {
  if (suffix.len > s.len) {
    return false;
  }
  if (suffix.len == 0u) {
    return true;
  }
  return memcmp(s.data + (s.len - suffix.len), suffix.data, suffix.len) == 0;
}

bool br_bytes_contains(br_bytes_view s, br_bytes_view needle) {
  return br_bytes_index(s, needle) >= 0;
}

bool br_bytes_contains_any(br_bytes_view s, br_bytes_view chars) {
  return br_bytes_index_any(s, chars) >= 0;
}

isize br_bytes_index_byte(br_bytes_view s, u8 byte_value) {
  const void *ptr = memchr(s.data, byte_value, s.len);

  if (ptr == NULL) {
    return -1;
  }

  return (isize)((const u8 *)ptr - s.data);
}

isize br_bytes_last_index_byte(br_bytes_view s, u8 byte_value) {
  for (usize i = s.len; i > 0u; --i) {
    if (s.data[i - 1u] == byte_value) {
      return (isize)(i - 1u);
    }
  }

  return -1;
}

isize br_bytes_index(br_bytes_view s, br_bytes_view needle) {
  if (needle.len == 0u) {
    return 0;
  }
  if (needle.len == 1u) {
    return br_bytes_index_byte(s, needle.data[0]);
  }
  if (needle.len > s.len) {
    return -1;
  }
  if (needle.len == s.len) {
    return br_bytes_equal(s, needle) ? 0 : -1;
  }

  for (usize i = 0; i <= s.len - needle.len; ++i) {
    if (s.data[i] == needle.data[0] && memcmp(s.data + i, needle.data, needle.len) == 0) {
      return (isize)i;
    }
  }

  return -1;
}

isize br_bytes_last_index(br_bytes_view s, br_bytes_view needle) {
  if (needle.len == 0u) {
    return (isize)s.len;
  }
  if (needle.len == 1u) {
    return br_bytes_last_index_byte(s, needle.data[0]);
  }
  if (needle.len > s.len) {
    return -1;
  }
  if (needle.len == s.len) {
    return br_bytes_equal(s, needle) ? 0 : -1;
  }

  for (usize i = s.len - needle.len + 1u; i > 0u; --i) {
    usize pos = i - 1u;
    if (s.data[pos] == needle.data[0] && memcmp(s.data + pos, needle.data, needle.len) == 0) {
      return (isize)pos;
    }
  }

  return -1;
}

isize br_bytes_index_any(br_bytes_view s, br_bytes_view chars) {
  if (chars.len == 0u) {
    return -1;
  }

  for (usize i = 0; i < s.len; ++i) {
    if (br__bytes_contains_byte(chars, s.data[i])) {
      return (isize)i;
    }
  }

  return -1;
}

isize br_bytes_count(br_bytes_view s, br_bytes_view needle) {
  isize count = 0;
  isize index;

  if (needle.len == 0u) {
    return (isize)br_utf8_rune_count(s) + 1;
  }
  if (needle.len == 1u) {
    for (usize i = 0; i < s.len; ++i) {
      if (s.data[i] == needle.data[0]) {
        count += 1;
      }
    }
    return count;
  }

  while ((index = br_bytes_index(s, needle)) >= 0) {
    count += 1;
    s = br_bytes_view_make(s.data + (usize)index + needle.len, s.len - ((usize)index + needle.len));
  }

  return count;
}

br_bytes_view br_bytes_truncate_to_byte(br_bytes_view s, u8 byte_value) {
  isize index = br_bytes_index_byte(s, byte_value);

  if (index < 0) {
    return s;
  }

  return br_bytes_view_make(s.data, (usize)index);
}

br_bytes_view br_bytes_trim_prefix(br_bytes_view s, br_bytes_view prefix) {
  if (br_bytes_has_prefix(s, prefix)) {
    return br_bytes_view_make(s.data + prefix.len, s.len - prefix.len);
  }

  return s;
}

br_bytes_view br_bytes_trim_suffix(br_bytes_view s, br_bytes_view suffix) {
  if (br_bytes_has_suffix(s, suffix)) {
    return br_bytes_view_make(s.data, s.len - suffix.len);
  }

  return s;
}

br_bytes_result br_bytes_join(const br_bytes_view *parts,
                              usize part_count,
                              br_bytes_view sep,
                              br_allocator allocator) {
  br_alloc_result alloc;
  usize total = 0u;
  usize offset = 0u;

  if (part_count == 0u) {
    return br__bytes_result(NULL, 0u, BR_STATUS_OK);
  }
  if (parts == NULL) {
    return br__bytes_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  for (usize i = 0; i < part_count; ++i) {
    if (br__bytes_add_overflow(total, parts[i].len, &total)) {
      return br__bytes_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
  }

  if (part_count > 1u) {
    usize sep_total;

    if (sep.len > 0u && (part_count - 1u) > (SIZE_MAX / sep.len)) {
      return br__bytes_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
    sep_total = sep.len * (part_count - 1u);
    if (br__bytes_add_overflow(total, sep_total, &total)) {
      return br__bytes_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
  }

  if (total == 0u) {
    return br__bytes_result(NULL, 0u, BR_STATUS_OK);
  }

  alloc = br_allocator_alloc_uninit(allocator, total, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__bytes_result(NULL, 0u, alloc.status);
  }

  for (usize i = 0; i < part_count; ++i) {
    if (i > 0u && sep.len > 0u) {
      memcpy((u8 *)alloc.ptr + offset, sep.data, sep.len);
      offset += sep.len;
    }
    if (parts[i].len > 0u) {
      memcpy((u8 *)alloc.ptr + offset, parts[i].data, parts[i].len);
      offset += parts[i].len;
    }
  }

  return br__bytes_result(alloc.ptr, total, BR_STATUS_OK);
}

br_bytes_result
br_bytes_concat(const br_bytes_view *parts, usize part_count, br_allocator allocator) {
  return br_bytes_join(parts, part_count, br_bytes_view_make(NULL, 0u), allocator);
}

br_bytes_result br_bytes_repeat(br_bytes_view s, usize count, br_allocator allocator) {
  br_alloc_result alloc;
  usize total;
  usize copied;

  if (count == 0u || s.len == 0u) {
    return br__bytes_result(NULL, 0u, BR_STATUS_OK);
  }
  if (count > (SIZE_MAX / s.len)) {
    return br__bytes_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  total = s.len * count;
  alloc = br_allocator_alloc_uninit(allocator, total, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__bytes_result(NULL, 0u, alloc.status);
  }

  memcpy(alloc.ptr, s.data, s.len);
  copied = s.len;
  while (copied < total) {
    usize chunk = br_min_size(copied, total - copied);
    memcpy((u8 *)alloc.ptr + copied, alloc.ptr, chunk);
    copied += chunk;
  }

  return br__bytes_result(alloc.ptr, total, BR_STATUS_OK);
}

static br_bytes_view_list_result br__bytes_split_impl(
  br_bytes_view s, br_bytes_view sep, usize sep_save, isize n, br_allocator allocator) {
  br_alloc_result alloc;
  br_bytes_view *parts;
  usize target_count;
  usize part_index = 0u;

  if (n == 0) {
    return br__bytes_view_list_result(NULL, 0u, BR_STATUS_OK);
  }

  if (sep.len == 0u) {
    target_count = br_utf8_rune_count(s);
    if (n >= 0 && (usize)n < target_count) {
      target_count = (usize)n;
    }
    if (target_count == 0u) {
      return br__bytes_view_list_result(NULL, 0u, BR_STATUS_OK);
    }

    alloc = br_allocator_alloc_uninit(
      allocator, target_count * sizeof(br_bytes_view), _Alignof(br_bytes_view));
    if (alloc.status != BR_STATUS_OK) {
      return br__bytes_view_list_result(NULL, 0u, alloc.status);
    }

    parts = alloc.ptr;
    for (; part_index + 1u < target_count; ++part_index) {
      br_utf8_decode_result decoded;

      decoded = br_utf8_decode(s);
      if (decoded.width == 0u) {
        break;
      }
      parts[part_index] = br_bytes_view_make(s.data, decoded.width);
      s = br_bytes_view_make(s.data + decoded.width, s.len - decoded.width);
    }
    parts[part_index] = s;
    return br__bytes_view_list_result(parts, part_index + 1u, BR_STATUS_OK);
  }

  if (n < 0) {
    target_count = (usize)br_bytes_count(s, sep) + 1u;
  } else {
    target_count = (usize)n;
  }

  if (target_count == 0u) {
    return br__bytes_view_list_result(NULL, 0u, BR_STATUS_OK);
  }

  alloc = br_allocator_alloc_uninit(
    allocator, target_count * sizeof(br_bytes_view), _Alignof(br_bytes_view));
  if (alloc.status != BR_STATUS_OK) {
    return br__bytes_view_list_result(NULL, 0u, alloc.status);
  }

  parts = alloc.ptr;
  if (target_count > 0u) {
    usize remaining_slots = target_count - 1u;

    while (part_index < remaining_slots) {
      isize split_at = br_bytes_index(s, sep);
      if (split_at < 0) {
        break;
      }

      parts[part_index] = br_bytes_view_make(s.data, (usize)split_at + sep_save);
      s =
        br_bytes_view_make(s.data + (usize)split_at + sep.len, s.len - ((usize)split_at + sep.len));
      part_index += 1u;
    }
  }

  parts[part_index] = s;
  part_index += 1u;
  return br__bytes_view_list_result(parts, part_index, BR_STATUS_OK);
}

br_bytes_view_list_result
br_bytes_split(br_bytes_view s, br_bytes_view sep, br_allocator allocator) {
  return br__bytes_split_impl(s, sep, 0u, -1, allocator);
}

br_bytes_view_list_result
br_bytes_split_n(br_bytes_view s, br_bytes_view sep, isize n, br_allocator allocator) {
  return br__bytes_split_impl(s, sep, 0u, n, allocator);
}

br_bytes_view_list_result
br_bytes_split_after(br_bytes_view s, br_bytes_view sep, br_allocator allocator) {
  return br__bytes_split_impl(s, sep, sep.len, -1, allocator);
}

br_bytes_view_list_result
br_bytes_split_after_n(br_bytes_view s, br_bytes_view sep, isize n, br_allocator allocator) {
  return br__bytes_split_impl(s, sep, sep.len, n, allocator);
}

br_bytes_rewrite_result br_bytes_replace(br_bytes_view s,
                                         br_bytes_view old_bytes,
                                         br_bytes_view new_bytes,
                                         isize n,
                                         br_allocator allocator) {
  br_alloc_result alloc;
  isize match_count;
  isize replace_count;
  usize output_len;
  usize start = 0u;
  usize write_offset = 0u;

  if ((old_bytes.len == new_bytes.len &&
       (old_bytes.len == 0u || (old_bytes.data != NULL && new_bytes.data != NULL &&
                                memcmp(old_bytes.data, new_bytes.data, old_bytes.len) == 0))) ||
      n == 0) {
    return br__bytes_rewrite_alias_result(s);
  }

  match_count = br_bytes_count(s, old_bytes);
  if (match_count == 0) {
    return br__bytes_rewrite_alias_result(s);
  }

  replace_count = match_count;
  if (n >= 0 && n < replace_count) {
    replace_count = n;
  }

  if (new_bytes.len >= old_bytes.len) {
    usize extra_per_match = new_bytes.len - old_bytes.len;
    if ((usize)replace_count > 0u && extra_per_match > SIZE_MAX / (usize)replace_count) {
      return br__bytes_rewrite_owned_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
    if (s.len > SIZE_MAX - (extra_per_match * (usize)replace_count)) {
      return br__bytes_rewrite_owned_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
    output_len = s.len + (extra_per_match * (usize)replace_count);
  } else {
    output_len = s.len - ((old_bytes.len - new_bytes.len) * (usize)replace_count);
  }

  if (output_len == 0u) {
    return br__bytes_rewrite_owned_result(NULL, 0u, BR_STATUS_OK);
  }

  alloc = br_allocator_alloc_uninit(allocator, output_len, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__bytes_rewrite_owned_result(NULL, 0u, alloc.status);
  }

  for (isize i = 0; i < replace_count; ++i) {
    usize split_at;
    usize next_start;

    if (old_bytes.len == 0u) {
      split_at = start;
      if (i > 0 && split_at < s.len) {
        br_utf8_decode_result decoded;

        decoded = br_utf8_decode(br_bytes_view_make(s.data + start, s.len - start));
        if (decoded.width > 0u) {
          split_at += decoded.width;
        }
      }
      next_start = split_at;
    } else {
      isize local_index =
        br_bytes_index(br_bytes_view_make(s.data + start, s.len - start), old_bytes);
      split_at = start + (usize)local_index;
      next_start = split_at + old_bytes.len;
    }

    if (split_at > start) {
      memcpy((u8 *)alloc.ptr + write_offset, s.data + start, split_at - start);
      write_offset += split_at - start;
    }
    if (new_bytes.len > 0u) {
      memcpy((u8 *)alloc.ptr + write_offset, new_bytes.data, new_bytes.len);
      write_offset += new_bytes.len;
    }
    start = next_start;
  }

  if (start < s.len) {
    memcpy((u8 *)alloc.ptr + write_offset, s.data + start, s.len - start);
    write_offset += s.len - start;
  }

  return br__bytes_rewrite_owned_result(alloc.ptr, write_offset, BR_STATUS_OK);
}

br_bytes_rewrite_result br_bytes_replace_all(br_bytes_view s,
                                             br_bytes_view old_bytes,
                                             br_bytes_view new_bytes,
                                             br_allocator allocator) {
  return br_bytes_replace(s, old_bytes, new_bytes, -1, allocator);
}

br_bytes_rewrite_result
br_bytes_remove(br_bytes_view s, br_bytes_view key, isize n, br_allocator allocator) {
  return br_bytes_replace(s, key, br_bytes_view_make(NULL, 0u), n, allocator);
}

br_bytes_rewrite_result
br_bytes_remove_all(br_bytes_view s, br_bytes_view key, br_allocator allocator) {
  return br_bytes_remove(s, key, -1, allocator);
}

/* ==== src/bytes/reader.c ==== */

static br_byte_reader_io_result br__byte_reader_io_result(usize count, br_status status) {
  br_byte_reader_io_result result;

  result.count = count;
  result.status = status;
  return result;
}

static br_byte_reader_byte_result br__byte_reader_byte_result(u8 value, br_status status) {
  br_byte_reader_byte_result result;

  result.value = value;
  result.status = status;
  return result;
}

static br_byte_reader_seek_result br__byte_reader_seek_result(i64 offset, br_status status) {
  br_byte_reader_seek_result result;

  result.offset = offset;
  result.status = status;
  return result;
}

void br_byte_reader_init(br_byte_reader *reader, br_bytes_view source) {
  if (reader == NULL) {
    return;
  }

  reader->source = source;
  reader->index = 0;
}

void br_byte_reader_reset(br_byte_reader *reader) {
  if (reader == NULL) {
    return;
  }

  reader->index = 0;
}

br_bytes_view br_byte_reader_view(const br_byte_reader *reader) {
  usize remaining;

  if (reader == NULL) {
    return br_bytes_view_make(NULL, 0u);
  }

  remaining = br_byte_reader_len(reader);
  if (remaining == 0u) {
    return br_bytes_view_make(NULL, 0u);
  }

  return br_bytes_view_make(reader->source.data + (usize)reader->index, remaining);
}

usize br_byte_reader_len(const br_byte_reader *reader) {
  if (reader == NULL || reader->index >= (i64)reader->source.len) {
    return 0u;
  }
  if (reader->index < 0) {
    return reader->source.len;
  }

  return reader->source.len - (usize)reader->index;
}

usize br_byte_reader_size(const br_byte_reader *reader) {
  return reader != NULL ? reader->source.len : 0u;
}

br_byte_reader_io_result br_byte_reader_read(br_byte_reader *reader, void *dst, usize dst_len) {
  usize count;

  if (reader == NULL || (dst == NULL && dst_len > 0u)) {
    return br__byte_reader_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_len == 0u) {
    return br__byte_reader_io_result(0u, BR_STATUS_OK);
  }
  if (reader->index >= (i64)reader->source.len) {
    return br__byte_reader_io_result(0u, BR_STATUS_EOF);
  }

  count = br_min_size(dst_len, reader->source.len - (usize)reader->index);
  memcpy(dst, reader->source.data + (usize)reader->index, count);
  reader->index += (i64)count;
  return br__byte_reader_io_result(count, BR_STATUS_OK);
}

br_byte_reader_io_result
br_byte_reader_read_at(const br_byte_reader *reader, void *dst, usize dst_len, i64 offset) {
  usize count;

  if (reader == NULL || (dst == NULL && dst_len > 0u)) {
    return br__byte_reader_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_len == 0u) {
    return br__byte_reader_io_result(0u, BR_STATUS_OK);
  }
  if (offset < 0) {
    return br__byte_reader_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (offset >= (i64)reader->source.len) {
    return br__byte_reader_io_result(0u, BR_STATUS_EOF);
  }

  count = br_min_size(dst_len, reader->source.len - (usize)offset);
  memcpy(dst, reader->source.data + (usize)offset, count);
  if (count < dst_len) {
    return br__byte_reader_io_result(count, BR_STATUS_EOF);
  }
  return br__byte_reader_io_result(count, BR_STATUS_OK);
}

br_byte_reader_byte_result br_byte_reader_read_byte(br_byte_reader *reader) {
  if (reader == NULL) {
    return br__byte_reader_byte_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (reader->index >= (i64)reader->source.len) {
    return br__byte_reader_byte_result(0u, BR_STATUS_EOF);
  }

  reader->index += 1;
  return br__byte_reader_byte_result(reader->source.data[(usize)reader->index - 1u], BR_STATUS_OK);
}

br_status br_byte_reader_unread_byte(br_byte_reader *reader) {
  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (reader->index <= 0) {
    return BR_STATUS_INVALID_STATE;
  }

  reader->index -= 1;
  return BR_STATUS_OK;
}

br_byte_reader_seek_result
br_byte_reader_seek(br_byte_reader *reader, i64 offset, br_seek_from whence) {
  i64 absolute;

  if (reader == NULL) {
    return br__byte_reader_seek_result(0, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (whence) {
    case BR_SEEK_FROM_START:
      absolute = offset;
      break;
    case BR_SEEK_FROM_CURRENT:
      absolute = reader->index + offset;
      break;
    case BR_SEEK_FROM_END:
      absolute = (i64)reader->source.len + offset;
      break;
    default:
      return br__byte_reader_seek_result(0, BR_STATUS_INVALID_ARGUMENT);
  }

  if (absolute < 0) {
    return br__byte_reader_seek_result(0, BR_STATUS_INVALID_ARGUMENT);
  }

  reader->index = absolute;
  return br__byte_reader_seek_result(absolute, BR_STATUS_OK);
}

static br_i64_result br__byte_reader_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  br_byte_reader *reader;
  br_byte_reader_io_result io_result;
  br_byte_reader_seek_result seek_result;
  br_io_mode_set modes;

  reader = (br_byte_reader *)context;
  switch (mode) {
    case BR_IO_MODE_READ:
      io_result = br_byte_reader_read(reader, data, data_len);
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_READ_AT:
      io_result = br_byte_reader_read_at(reader, data, data_len, offset);
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_SEEK:
      seek_result = br_byte_reader_seek(reader, offset, whence);
      return br_i64_result_make(seek_result.offset, seek_result.status);
    case BR_IO_MODE_SIZE:
      return br_i64_result_make((i64)br_byte_reader_size(reader), BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_READ) | br_io_mode_bit(BR_IO_MODE_READ_AT) |
              br_io_mode_bit(BR_IO_MODE_SEEK) | br_io_mode_bit(BR_IO_MODE_SIZE);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_byte_reader_as_stream(br_byte_reader *reader) {
  return br_stream_make(reader, br__byte_reader_stream_proc);
}

/* ==== src/encoding/hex.c ==== */

static const u8 br__hex_lower[16] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

static const u8 br__hex_upper[16] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static const u8 *br__hex_table(br_hex_case letter_case) {
  return letter_case == BR_HEX_UPPER ? br__hex_upper : br__hex_lower;
}

/* Map one hex character to its 0-15 nibble value; false if not [0-9a-fA-F]. */
static bool br__hex_digit(u8 c, u8 *out) {
  if (c >= '0' && c <= '9') {
    *out = (u8)(c - '0');
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    *out = (u8)(c - 'a' + 10);
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    *out = (u8)(c - 'A' + 10);
    return true;
  }

  return false;
}

static br_bytes_result br__bytes_result(void *data, usize len, br_status status) {
  br_bytes_result result;

  result.value = br_bytes_make(data, len);
  result.status = status;
  return result;
}

static br_decode_result br__decode_result(br_bytes value, usize error_offset, br_status status) {
  br_decode_result result;

  result.value = value;
  result.error_offset = error_offset;
  result.status = status;
  return result;
}

static br_decode_into_result
br__decode_into_result(usize count, usize error_offset, br_status status) {
  br_decode_into_result result;

  result.count = count;
  result.error_offset = error_offset;
  result.status = status;
  return result;
}

br_bytes_result br_hex_encode(br_bytes_view src, br_hex_case letter_case, br_allocator allocator) {
  const u8 *table = br__hex_table(letter_case);
  br_alloc_result alloc;
  u8 *dst;
  usize out_len;

  if (src.len == 0u) {
    return br__bytes_result(NULL, 0u, BR_STATUS_OK);
  }

  /* out_len = src.len * 2; guard the (practically unreachable) overflow. */
  if (src.len > SIZE_MAX / 2u) {
    return br__bytes_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }
  out_len = src.len * 2u;

  alloc = br_allocator_alloc_uninit(allocator, out_len, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__bytes_result(NULL, 0u, alloc.status);
  }

  dst = (u8 *)alloc.ptr;
  for (usize i = 0u, j = 0u; i < src.len; i += 1u, j += 2u) {
    u8 v = src.data[i];

    dst[j] = table[v >> 4];
    dst[j + 1u] = table[v & 0x0fu];
  }

  return br__bytes_result(dst, out_len, BR_STATUS_OK);
}

br_io_result
br_hex_encode_into(br_bytes_view src, br_hex_case letter_case, u8 *dst, usize dst_cap) {
  const u8 *table = br__hex_table(letter_case);
  usize out_len;

  /* out_len = src.len * 2; guard the (practically unreachable) overflow. */
  if (src.len > SIZE_MAX / 2u) {
    return br_io_result_make(0u, BR_STATUS_SHORT_BUFFER);
  }
  out_len = src.len * 2u;

  if (out_len > 0u && dst == NULL) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_cap < out_len) {
    return br_io_result_make(0u, BR_STATUS_SHORT_BUFFER);
  }

  for (usize i = 0u, j = 0u; i < src.len; i += 1u, j += 2u) {
    u8 v = src.data[i];

    dst[j] = table[v >> 4];
    dst[j + 1u] = table[v & 0x0fu];
  }

  return br_io_result_make(out_len, BR_STATUS_OK);
}

br_io_result br_hex_encode_to_writer(br_bytes_view src, br_hex_case letter_case, br_writer w) {
  const u8 *table = br__hex_table(letter_case);
  u8 buffer[512];
  usize buffered = 0u;
  usize total = 0u;

  for (usize i = 0u; i < src.len; i += 1u) {
    u8 v = src.data[i];

    buffer[buffered] = table[v >> 4];
    buffer[buffered + 1u] = table[v & 0x0fu];
    buffered += 2u;

    if (buffered == sizeof(buffer)) {
      br_io_result written = br_write_full(w, buffer, buffered);

      total += written.count;
      if (written.status != BR_STATUS_OK) {
        return br_io_result_make(total, written.status);
      }
      buffered = 0u;
    }
  }

  if (buffered > 0u) {
    br_io_result written = br_write_full(w, buffer, buffered);

    total += written.count;
    if (written.status != BR_STATUS_OK) {
      return br_io_result_make(total, written.status);
    }
  }

  return br_io_result_make(total, BR_STATUS_OK);
}

br_decode_result br_hex_decode(br_bytes_view src, br_allocator allocator) {
  br_alloc_result alloc;
  u8 *dst;
  usize out_len;

  /* Length parity is a structural property, so it is checked before allocating.
     This also keeps the odd-length path off the allocate-then-fail branch. */
  if ((src.len & 1u) != 0u) {
    return br__decode_result(br_bytes_make(NULL, 0u), src.len - 1u, BR_STATUS_INVALID_ENCODING);
  }

  out_len = src.len / 2u;
  if (out_len == 0u) {
    return br__decode_result(br_bytes_make(NULL, 0u), 0u, BR_STATUS_OK);
  }

  alloc = br_allocator_alloc_uninit(allocator, out_len, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__decode_result(br_bytes_make(NULL, 0u), 0u, alloc.status);
  }
  dst = (u8 *)alloc.ptr;

  for (usize i = 0u, j = 0u; j < src.len; i += 1u, j += 2u) {
    u8 hi;
    u8 lo;

    /* Free before returning any failure: this fixes Odin's decode leak, where
       the destination buffer is abandoned on an invalid byte. */
    if (!br__hex_digit(src.data[j], &hi)) {
      br_allocator_free(allocator, dst, out_len);
      return br__decode_result(br_bytes_make(NULL, 0u), j, BR_STATUS_INVALID_ENCODING);
    }
    if (!br__hex_digit(src.data[j + 1u], &lo)) {
      br_allocator_free(allocator, dst, out_len);
      return br__decode_result(br_bytes_make(NULL, 0u), j + 1u, BR_STATUS_INVALID_ENCODING);
    }

    dst[i] = (u8)((hi << 4) | lo);
  }

  return br__decode_result(br_bytes_make(dst, out_len), 0u, BR_STATUS_OK);
}

br_decode_into_result br_hex_decode_into(br_bytes_view src, u8 *dst, usize dst_cap) {
  usize out_len;

  /* Parity is checked first so odd-length input classifies as INVALID_ENCODING
     identically to br_hex_decode, independent of the caller's buffer state. */
  if ((src.len & 1u) != 0u) {
    return br__decode_into_result(0u, src.len - 1u, BR_STATUS_INVALID_ENCODING);
  }

  out_len = src.len / 2u;

  if (out_len > 0u && dst == NULL) {
    return br__decode_into_result(0u, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_cap < out_len) {
    return br__decode_into_result(0u, 0u, BR_STATUS_SHORT_BUFFER);
  }

  for (usize i = 0u, j = 0u; j < src.len; i += 1u, j += 2u) {
    u8 hi;
    u8 lo;

    /* On a malformed byte the caller's buffer holds only partial scratch, so
       count reports 0: no valid output was produced (mirrors the allocating
       decode returning an empty value). error_offset locates the fault. */
    if (!br__hex_digit(src.data[j], &hi)) {
      return br__decode_into_result(0u, j, BR_STATUS_INVALID_ENCODING);
    }
    if (!br__hex_digit(src.data[j + 1u], &lo)) {
      return br__decode_into_result(0u, j + 1u, BR_STATUS_INVALID_ENCODING);
    }

    dst[i] = (u8)((hi << 4) | lo);
  }

  return br__decode_into_result(out_len, 0u, BR_STATUS_OK);
}

br_io_byte_result br_hex_decode_sequence(br_bytes_view seq) {
  u8 hi;
  u8 lo;

  /* Strip an optional "0x"/"0X" prefix (seq is a by-value view). */
  if (seq.len >= 2u && seq.data[0] == '0' && (seq.data[1] == 'x' || seq.data[1] == 'X')) {
    seq.data += 2;
    seq.len -= 2u;
  }

  if (seq.len != 2u) {
    return br_io_byte_result_make(0u, BR_STATUS_INVALID_ENCODING);
  }
  if (!br__hex_digit(seq.data[0], &hi) || !br__hex_digit(seq.data[1], &lo)) {
    return br_io_byte_result_make(0u, BR_STATUS_INVALID_ENCODING);
  }

  return br_io_byte_result_make((u8)((hi << 4) | lo), BR_STATUS_OK);
}

/* ==== src/io/io.c ==== */
#include <limits.h>


enum { BR__IO_COPY_BUFFER_SIZE = 4096 };

/*
Odin relies on stream proc contracts to report sane byte counts. Bedrock
validates them here because arbitrary C callbacks can return impossible values.
*/
static br_io_result br__io_result_from_i64(br_i64_result result, usize requested) {
  if (result.value < 0) {
    return br_io_result_make(0u, BR_STATUS_INVALID_STATE);
  }
  if ((u64)result.value > (u64)SIZE_MAX) {
    return br_io_result_make(0u, BR_STATUS_INVALID_STATE);
  }
  if ((usize)result.value > requested) {
    return br_io_result_make(0u, BR_STATUS_INVALID_STATE);
  }

  return br_io_result_make((usize)result.value, result.status);
}

static usize br__utf8_expected_width(u8 byte_value) {
  if (byte_value < 0x80u) {
    return 1u;
  }
  if (byte_value >= 0xc2u && byte_value <= 0xdfu) {
    return 2u;
  }
  if (byte_value >= 0xe0u && byte_value <= 0xefu) {
    return 3u;
  }
  if (byte_value >= 0xf0u && byte_value <= 0xf4u) {
    return 4u;
  }

  return 1u;
}

static br_status br__short_write_status(br_io_result result, usize expected) {
  if (result.count == expected) {
    return result.status;
  }
  if (result.status == BR_STATUS_OK) {
    return BR_STATUS_SHORT_WRITE;
  }

  return result.status;
}

static br_i64_result br__stream_call(
  br_stream stream, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  if (stream.procedure == NULL) {
    return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }

  return stream.procedure(stream.context, mode, data, data_len, offset, whence);
}

static br_io_result br__read_at_fallback(br_stream stream, void *dst, usize dst_len, i64 offset) {
  br_io_seek_result current;
  br_io_seek_result target;
  br_io_seek_result restore;
  br_io_result result;

  current = br_seek(stream, 0, BR_SEEK_FROM_CURRENT);
  if (current.status != BR_STATUS_OK) {
    return br_io_result_make(0u, current.status);
  }

  target = br_seek(stream, offset, BR_SEEK_FROM_START);
  if (target.status != BR_STATUS_OK) {
    return br_io_result_make(0u, target.status);
  }

  result = br_read(stream, dst, dst_len);
  restore = br_seek(stream, current.offset, BR_SEEK_FROM_START);
  if (restore.status != BR_STATUS_OK && result.status == BR_STATUS_OK) {
    result.status = restore.status;
  }
  return result;
}

static br_io_result
br__write_at_fallback(br_stream stream, const void *src, usize src_len, i64 offset) {
  br_io_seek_result current;
  br_io_seek_result target;
  br_io_seek_result restore;
  br_io_result result;

  current = br_seek(stream, 0, BR_SEEK_FROM_CURRENT);
  if (current.status != BR_STATUS_OK) {
    return br_io_result_make(0u, current.status);
  }

  target = br_seek(stream, offset, BR_SEEK_FROM_START);
  if (target.status != BR_STATUS_OK) {
    return br_io_result_make(0u, target.status);
  }

  result = br_write(stream, src, src_len);
  restore = br_seek(stream, current.offset, BR_SEEK_FROM_START);
  if (restore.status != BR_STATUS_OK && result.status == BR_STATUS_OK) {
    result.status = restore.status;
  }
  return result;
}

br_io_result br_read(br_stream stream, void *dst, usize dst_len) {
  if (dst == NULL && dst_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  return br__io_result_from_i64(
    br__stream_call(stream, BR_IO_MODE_READ, dst, dst_len, 0, BR_SEEK_FROM_START), dst_len);
}

br_io_result br_read_at_least(br_stream stream, void *dst, usize dst_len, usize min_len) {
  u8 *cursor;
  usize total;
  br_status status;

  if (dst == NULL && dst_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_len < min_len) {
    return br_io_result_make(0u, BR_STATUS_SHORT_BUFFER);
  }

  cursor = (u8 *)dst;
  total = 0u;
  status = BR_STATUS_OK;
  while (total < min_len && status == BR_STATUS_OK) {
    br_io_result result;

    result = br_read(stream, cursor + total, dst_len - total);
    total += result.count;
    if (result.status != BR_STATUS_OK) {
      status = result.status;
      break;
    }
    /*
    Odin assumes well-behaved readers here. Bedrock treats a zero-byte success
    as no progress so a buggy C callback cannot spin forever.
    */
    if (result.count == 0u) {
      status = BR_STATUS_NO_PROGRESS;
      break;
    }
  }

  if (total >= min_len) {
    return br_io_result_make(total, BR_STATUS_OK);
  }
  if (total > 0u && status == BR_STATUS_EOF) {
    return br_io_result_make(total, BR_STATUS_UNEXPECTED_EOF);
  }

  return br_io_result_make(total, status);
}

br_io_result br_read_full(br_stream stream, void *dst, usize dst_len) {
  return br_read_at_least(stream, dst, dst_len, dst_len);
}

br_io_result br_write(br_stream stream, const void *src, usize src_len) {
  if (src == NULL && src_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  return br__io_result_from_i64(
    br__stream_call(stream, BR_IO_MODE_WRITE, (void *)src, src_len, 0, BR_SEEK_FROM_START),
    src_len);
}

br_io_result br_write_at_least(br_stream stream, const void *src, usize src_len, usize min_len) {
  const u8 *cursor;
  usize total;
  br_status status;

  if (src == NULL && src_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (src_len < min_len) {
    return br_io_result_make(0u, BR_STATUS_SHORT_BUFFER);
  }

  cursor = (const u8 *)src;
  total = 0u;
  status = BR_STATUS_OK;
  while (total < min_len && status == BR_STATUS_OK) {
    br_io_result result;

    result = br_write(stream, cursor + total, src_len - total);
    total += result.count;
    if (result.status != BR_STATUS_OK) {
      status = result.status;
      break;
    }
    /*
    Same reasoning as br_read_at_least: a zero-byte successful write would
    otherwise loop forever on a malformed C callback.
    */
    if (result.count == 0u) {
      status = BR_STATUS_NO_PROGRESS;
      break;
    }
  }

  return br_io_result_make(total, status);
}

br_io_result br_write_full(br_stream stream, const void *src, usize src_len) {
  return br_write_at_least(stream, src, src_len, src_len);
}

br_io_result br_read_at(br_stream stream, void *dst, usize dst_len, i64 offset) {
  br_i64_result raw;

  if (dst == NULL && dst_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (offset < 0) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  raw = br__stream_call(stream, BR_IO_MODE_READ_AT, dst, dst_len, offset, BR_SEEK_FROM_START);
  if (raw.status == BR_STATUS_NOT_SUPPORTED) {
    return br__read_at_fallback(stream, dst, dst_len, offset);
  }

  return br__io_result_from_i64(raw, dst_len);
}

br_io_result br_write_at(br_stream stream, const void *src, usize src_len, i64 offset) {
  br_i64_result raw;

  if (src == NULL && src_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (offset < 0) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  raw =
    br__stream_call(stream, BR_IO_MODE_WRITE_AT, (void *)src, src_len, offset, BR_SEEK_FROM_START);
  if (raw.status == BR_STATUS_NOT_SUPPORTED) {
    return br__write_at_fallback(stream, src, src_len, offset);
  }

  return br__io_result_from_i64(raw, src_len);
}

br_io_seek_result br_seek(br_stream stream, i64 offset, br_seek_from whence) {
  br_i64_result raw;

  raw = br__stream_call(stream, BR_IO_MODE_SEEK, NULL, 0u, offset, whence);
  return br_io_seek_result_make(raw.value, raw.status);
}

br_status br_close(br_stream stream) {
  return br__stream_call(stream, BR_IO_MODE_CLOSE, NULL, 0u, 0, BR_SEEK_FROM_START).status;
}

br_status br_flush(br_stream stream) {
  return br__stream_call(stream, BR_IO_MODE_FLUSH, NULL, 0u, 0, BR_SEEK_FROM_START).status;
}

br_status br_destroy(br_stream stream) {
  return br__stream_call(stream, BR_IO_MODE_DESTROY, NULL, 0u, 0, BR_SEEK_FROM_START).status;
}

br_io_size_result br_size(br_stream stream) {
  br_i64_result raw;
  br_io_seek_result current;
  br_io_seek_result end;
  br_io_seek_result restore;

  raw = br__stream_call(stream, BR_IO_MODE_SIZE, NULL, 0u, 0, BR_SEEK_FROM_START);
  if (raw.status != BR_STATUS_NOT_SUPPORTED) {
    return br_io_size_result_make(raw.value, raw.status);
  }

  current = br_seek(stream, 0, BR_SEEK_FROM_CURRENT);
  if (current.status != BR_STATUS_OK) {
    return br_io_size_result_make(0, current.status);
  }

  end = br_seek(stream, 0, BR_SEEK_FROM_END);
  if (end.status != BR_STATUS_OK) {
    return br_io_size_result_make(0, end.status);
  }

  restore = br_seek(stream, current.offset, BR_SEEK_FROM_START);
  if (restore.status != BR_STATUS_OK) {
    return br_io_size_result_make(0, restore.status);
  }

  return br_io_size_result_make(end.offset, BR_STATUS_OK);
}

br_io_query_result br_query(br_stream stream) {
  br_i64_result raw;
  br_io_mode_set modes;

  raw = br__stream_call(stream, BR_IO_MODE_QUERY, NULL, 0u, 0, BR_SEEK_FROM_START);
  if (raw.status != BR_STATUS_OK) {
    return br_io_query_result_make(0u, raw.status);
  }
  if (raw.value < 0) {
    return br_io_query_result_make(0u, BR_STATUS_INVALID_STATE);
  }

  modes = (br_io_mode_set)raw.value;
  modes |= br_io_mode_bit(BR_IO_MODE_QUERY);
  return br_io_query_result_make(modes, BR_STATUS_OK);
}

br_io_byte_result br_read_byte(br_stream stream) {
  u8 byte_value;
  br_io_result result;

  result = br_read(stream, &byte_value, 1u);
  if (result.count == 1u) {
    return br_io_byte_result_make(byte_value, result.status);
  }
  if (result.status == BR_STATUS_OK) {
    return br_io_byte_result_make(0u, BR_STATUS_INVALID_STATE);
  }

  return br_io_byte_result_make(0u, result.status);
}

br_status br_write_byte(br_stream stream, u8 value) {
  br_io_result result;

  result = br_write(stream, &value, 1u);
  return br__short_write_status(result, 1u);
}

br_io_rune_result br_read_rune(br_stream stream) {
  u8 buffer[BR_UTF8_MAX];
  br_io_byte_result first;
  br_io_result tail;
  br_utf8_decode_result decoded;
  usize expected;
  usize total;
  br_status status;

  first = br_read_byte(stream);
  if (first.status != BR_STATUS_OK) {
    return br_io_rune_result_make(0, 0u, first.status);
  }
  if (first.value < (u8)BR_RUNE_SELF) {
    return br_io_rune_result_make((br_rune)first.value, 1u, BR_STATUS_OK);
  }

  buffer[0] = first.value;
  expected = br__utf8_expected_width(first.value);
  if (expected == 1u) {
    return br_io_rune_result_make(BR_RUNE_ERROR, 1u, BR_STATUS_OK);
  }

  tail = br_read(stream, buffer + 1u, expected - 1u);
  total = 1u + tail.count;
  if (tail.count + 1u < expected) {
    status = tail.status != BR_STATUS_OK ? tail.status : BR_STATUS_EOF;
    return br_io_rune_result_make(BR_RUNE_ERROR, total, status);
  }
  if (tail.status != BR_STATUS_OK) {
    return br_io_rune_result_make(BR_RUNE_ERROR, total, tail.status);
  }

  decoded = br_utf8_decode(br_bytes_view_make(buffer, total));
  return br_io_rune_result_make(decoded.value, total, BR_STATUS_OK);
}

br_io_result br_write_rune(br_stream stream, br_rune value) {
  br_utf8_encode_result encoded;
  br_io_result result;

  encoded = br_utf8_encode(value);
  result = br_write(stream, encoded.bytes, encoded.len);
  if (result.count < encoded.len && result.status == BR_STATUS_OK) {
    result.status = BR_STATUS_SHORT_WRITE;
  }
  return result;
}

br_i64_result br_copy(br_stream dst, br_stream src) {
  u8 buffer[BR__IO_COPY_BUFFER_SIZE];

  return br_copy_buffer(dst, src, buffer, BR_ARRAY_COUNT(buffer));
}

br_i64_result br_copy_buffer(br_stream dst, br_stream src, void *buffer, usize buffer_len) {
  u8 *scratch;
  i64 written;

  if (buffer == NULL || buffer_len == 0u) {
    return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }

  scratch = (u8 *)buffer;
  written = 0;
  for (;;) {
    br_io_result read_result;

    read_result = br_read(src, scratch, buffer_len);
    if (read_result.count > 0u) {
      br_io_result write_result;

      write_result = br_write(dst, scratch, read_result.count);
      written += (i64)write_result.count;
      if (write_result.count < read_result.count) {
        if (write_result.status == BR_STATUS_OK) {
          return br_i64_result_make(written, BR_STATUS_SHORT_WRITE);
        }
        return br_i64_result_make(written, write_result.status);
      }
      if (write_result.status != BR_STATUS_OK) {
        return br_i64_result_make(written, write_result.status);
      }
    }

    if (read_result.status != BR_STATUS_OK) {
      if (read_result.status == BR_STATUS_EOF) {
        return br_i64_result_make(written, BR_STATUS_OK);
      }
      return br_i64_result_make(written, read_result.status);
    }
    if (read_result.count == 0u) {
      return br_i64_result_make(written, BR_STATUS_INVALID_STATE);
    }
  }
}

/* ==== src/mem/alloc.c ==== */

#include <stdlib.h>

typedef struct br__heap_header {
  void *base;
  usize size;
} br__heap_header;

static br_alloc_result br__alloc_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static usize br__normalize_alignment(usize alignment) {
  usize min_alignment = (usize) _Alignof(br__heap_header);

  if (alignment == 0u) {
    alignment = BR_DEFAULT_ALIGNMENT;
  }

  if (alignment < min_alignment) {
    alignment = min_alignment;
  }

  return alignment;
}

static uptr br__align_up_uintptr(uptr value, usize alignment) {
  return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static br_alloc_result br__heap_alloc(usize size, usize alignment, bool zeroed) {
  br__heap_header *header;
  uptr aligned_addr;
  u8 *base;
  usize extra;
  usize total;

  alignment = br__normalize_alignment(alignment);
  if (!br_is_power_of_two_size(alignment)) {
    return br__alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  if (size == 0u) {
    return br__alloc_result(NULL, 0u, BR_STATUS_OK);
  }

  extra = sizeof(br__heap_header) + alignment - 1u;
  if (size > SIZE_MAX - extra) {
    return br__alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  total = size + extra;
  base = zeroed ? (u8 *)calloc(1u, total) : (u8 *)malloc(total);
  if (base == NULL) {
    return br__alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  aligned_addr = br__align_up_uintptr((uptr)base + sizeof(br__heap_header), alignment);
  header = (br__heap_header *)(void *)(aligned_addr - (uptr)sizeof(br__heap_header));
  header->base = base;
  header->size = size;

  return br__alloc_result((void *)aligned_addr, size, BR_STATUS_OK);
}

static br_alloc_result
br__heap_resize(void *ptr, usize old_size, usize new_size, usize alignment, bool zeroed) {
  br__heap_header *header;
  br_alloc_result result;

  if (ptr == NULL) {
    return br__heap_alloc(new_size, alignment, zeroed);
  }

  if (new_size == 0u) {
    header = (br__heap_header *)ptr - 1;
    free(header->base);
    return br__alloc_result(NULL, 0u, BR_STATUS_OK);
  }

  header = (br__heap_header *)ptr - 1;
  if (old_size == 0u) {
    old_size = header->size;
  }

  result = br__heap_alloc(new_size, alignment, zeroed);
  if (result.status != BR_STATUS_OK) {
    return result;
  }

  memcpy(result.ptr, ptr, br_min_size(old_size, new_size));
  free(header->base);
  return result;
}

static br_alloc_result br__heap_allocator_fn(void *ctx, const br_alloc_request *req) {
  BR_UNUSED(ctx);

  if (req == NULL) {
    return br__alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__heap_alloc(req->size, req->alignment, 1);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__heap_alloc(req->size, req->alignment, 0);
    case BR_ALLOC_OP_RESIZE:
      return br__heap_resize(req->ptr, req->old_size, req->size, req->alignment, 1);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__heap_resize(req->ptr, req->old_size, req->size, req->alignment, 0);
    case BR_ALLOC_OP_FREE:
      if (req->ptr != NULL) {
        br__heap_header *header = (br__heap_header *)req->ptr - 1;
        free(header->base);
      }
      return br__alloc_result(NULL, 0u, BR_STATUS_OK);
    case BR_ALLOC_OP_RESET:
      return br__alloc_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

static br_alloc_result br__null_allocator_fn(void *ctx, const br_alloc_request *req) {
  BR_UNUSED(ctx);
  BR_UNUSED(req);
  return br__alloc_result(NULL, 0u, BR_STATUS_OK);
}

static br_alloc_result br__fail_allocator_fn(void *ctx, const br_alloc_request *req) {
  BR_UNUSED(ctx);

  if (req == NULL) {
    return br__alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
    case BR_ALLOC_OP_ALLOC_UNINIT:
    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    case BR_ALLOC_OP_FREE:
    case BR_ALLOC_OP_RESET:
      return br__alloc_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

br_alloc_result br_allocator_call(br_allocator allocator, const br_alloc_request *req) {
  if (allocator.fn == NULL) {
    return br__alloc_result(NULL, 0u, BR_STATUS_NOT_SUPPORTED);
  }

  return allocator.fn(allocator.ctx, req);
}

br_alloc_result br_allocator_alloc(br_allocator allocator, usize size, usize alignment) {
  br_alloc_request req;

  req.op = BR_ALLOC_OP_ALLOC;
  req.ptr = NULL;
  req.old_size = 0u;
  req.size = size;
  req.alignment = alignment;
  return br_allocator_call(allocator, &req);
}

br_alloc_result br_allocator_alloc_uninit(br_allocator allocator, usize size, usize alignment) {
  br_alloc_request req;

  req.op = BR_ALLOC_OP_ALLOC_UNINIT;
  req.ptr = NULL;
  req.old_size = 0u;
  req.size = size;
  req.alignment = alignment;
  return br_allocator_call(allocator, &req);
}

br_alloc_result br_allocator_resize(
  br_allocator allocator, void *ptr, usize old_size, usize new_size, usize alignment) {
  br_alloc_request req;

  req.op = BR_ALLOC_OP_RESIZE;
  req.ptr = ptr;
  req.old_size = old_size;
  req.size = new_size;
  req.alignment = alignment;
  return br_allocator_call(allocator, &req);
}

br_alloc_result br_allocator_resize_uninit(
  br_allocator allocator, void *ptr, usize old_size, usize new_size, usize alignment) {
  br_alloc_request req;

  req.op = BR_ALLOC_OP_RESIZE_UNINIT;
  req.ptr = ptr;
  req.old_size = old_size;
  req.size = new_size;
  req.alignment = alignment;
  return br_allocator_call(allocator, &req);
}

br_status br_allocator_free(br_allocator allocator, void *ptr, usize old_size) {
  br_alloc_request req;
  br_alloc_result result;

  req.op = BR_ALLOC_OP_FREE;
  req.ptr = ptr;
  req.old_size = old_size;
  req.size = 0u;
  req.alignment = 0u;
  result = br_allocator_call(allocator, &req);
  return result.status;
}

br_status br_allocator_reset(br_allocator allocator) {
  br_alloc_request req;
  br_alloc_result result;

  req.op = BR_ALLOC_OP_RESET;
  req.ptr = NULL;
  req.old_size = 0u;
  req.size = 0u;
  req.alignment = 0u;
  result = br_allocator_call(allocator, &req);
  return result.status;
}

br_allocator br_allocator_heap(void) {
  br_allocator allocator;

  allocator.fn = br__heap_allocator_fn;
  allocator.ctx = NULL;
  return allocator;
}

br_allocator br_allocator_null(void) {
  br_allocator allocator;

  allocator.fn = br__null_allocator_fn;
  allocator.ctx = NULL;
  return allocator;
}

br_allocator br_allocator_fail(void) {
  br_allocator allocator;

  allocator.fn = br__fail_allocator_fn;
  allocator.ctx = NULL;
  return allocator;
}

/* ==== src/mem/arena.c ==== */

static br_alloc_result br__arena_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static uptr br__arena_align_up(uptr value, usize alignment) {
  return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static br_alloc_result br__arena_alloc(br_arena *arena, usize size, usize alignment, bool zeroed) {
  uptr base_addr;
  uptr aligned_addr;
  usize new_offset;
  usize start;

  if (arena == NULL) {
    return br__arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  if (alignment == 0u) {
    alignment = BR_DEFAULT_ALIGNMENT;
  }

  if (!br_is_power_of_two_size(alignment)) {
    return br__arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  if (size == 0u) {
    return br__arena_result(NULL, 0u, BR_STATUS_OK);
  }

  base_addr = (uptr)arena->buffer;
  aligned_addr = br__arena_align_up(base_addr + arena->offset, alignment);
  start = (usize)(aligned_addr - base_addr);

  if (start > arena->capacity || size > (arena->capacity - start)) {
    return br__arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  new_offset = start + size;
  if (zeroed) {
    memset((void *)aligned_addr, 0, size);
  }

  arena->offset = new_offset;
  if (arena->peak < new_offset) {
    arena->peak = new_offset;
  }

  return br__arena_result((void *)aligned_addr, size, BR_STATUS_OK);
}

static br_alloc_result br__arena_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_alloc_result result;
  br_arena *arena = (br_arena *)ctx;

  if (req == NULL) {
    return br__arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__arena_alloc(arena, req->size, req->alignment, 1);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__arena_alloc(arena, req->size, req->alignment, 0);
    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
      if (req->ptr == NULL) {
        return br__arena_alloc(arena, req->size, req->alignment, req->op == BR_ALLOC_OP_RESIZE);
      }
      if (req->old_size == 0u) {
        return br__arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
      }

      result = br__arena_alloc(arena, req->size, req->alignment, req->op == BR_ALLOC_OP_RESIZE);
      if (result.status != BR_STATUS_OK) {
        return result;
      }

      memcpy(result.ptr, req->ptr, br_min_size(req->old_size, req->size));
      return result;
    case BR_ALLOC_OP_FREE:
      return br__arena_result(NULL, 0u, BR_STATUS_OK);
    case BR_ALLOC_OP_RESET:
      br_arena_reset(arena);
      return br__arena_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

void br_arena_init(br_arena *arena, void *buffer, usize capacity) {
  if (arena == NULL) {
    return;
  }

  arena->buffer = (u8 *)buffer;
  arena->capacity = capacity;
  arena->offset = 0u;
  arena->peak = 0u;
}

void br_arena_reset(br_arena *arena) {
  if (arena == NULL) {
    return;
  }

  arena->offset = 0u;
}

br_arena_mark br_arena_mark_save(const br_arena *arena) {
  br_arena_mark mark;

  mark.offset = arena != NULL ? arena->offset : 0u;
  return mark;
}

br_status br_arena_rewind(br_arena *arena, br_arena_mark mark) {
  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (mark.offset > arena->offset) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  arena->offset = mark.offset;
  return BR_STATUS_OK;
}

br_alloc_result br_arena_alloc(br_arena *arena, usize size, usize alignment) {
  return br__arena_alloc(arena, size, alignment, 1);
}

br_alloc_result br_arena_alloc_uninit(br_arena *arena, usize size, usize alignment) {
  return br__arena_alloc(arena, size, alignment, 0);
}

br_allocator br_arena_allocator(br_arena *arena) {
  br_allocator allocator;

  allocator.fn = br__arena_allocator_fn;
  allocator.ctx = arena;
  return allocator;
}

/* ==== src/mem/buddy_allocator.c ==== */

struct br_buddy_block {
  usize size;
  bool is_free;
};

BR_STATIC_ASSERT((sizeof(struct br_buddy_block) & (sizeof(struct br_buddy_block) - 1u)) == 0u,
                 "Buddy block header size must stay a power of two.");

static br_alloc_result br__buddy_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static br_status br__buddy_validate_request_alignment(usize *alignment) {
  if (alignment == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (*alignment == 0u) {
    *alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (!br_is_power_of_two_size(*alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  return BR_STATUS_OK;
}

static usize br__buddy_normalize_init_alignment(usize alignment) {
  if (alignment < sizeof(struct br_buddy_block)) {
    alignment = sizeof(struct br_buddy_block);
  }
  return alignment;
}

static usize br__buddy_block_size_required_for_alignment(usize alignment, usize size) {
  usize actual_size;
  usize rounded;

  if (alignment == 0u || size == 0u) {
    return 0u;
  }
  if (size > SIZE_MAX - alignment) {
    return 0u;
  }

  actual_size = alignment + size;
  if (br_is_power_of_two_size(actual_size)) {
    return actual_size;
  }

  rounded = 1u;
  while (rounded < actual_size) {
    if (rounded > SIZE_MAX / 2u) {
      return 0u;
    }
    rounded <<= 1u;
  }
  return rounded;
}

static usize br__buddy_min_alloc_size(const br_buddy_allocator *buddy) {
  if (buddy == NULL) {
    return 0u;
  }
  return br__buddy_block_size_required_for_alignment(buddy->alignment, 1u);
}

static br_status br__buddy_validate_initialized(const br_buddy_allocator *buddy) {
  if (buddy == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (buddy->head == NULL || buddy->tail == NULL || buddy->alignment == 0u) {
    return BR_STATUS_INVALID_STATE;
  }
  return BR_STATUS_OK;
}

static br_buddy_block *br__buddy_block_next(br_buddy_block *block) {
  return (br_buddy_block *)(void *)((u8 *)(void *)block + block->size);
}

static br_buddy_block *br__buddy_block_split(br_buddy_block *block, usize size) {
  if (block != NULL && size != 0u) {
    while (size < block->size) {
      usize half = block->size >> 1u;

      block->size = half;
      block = br__buddy_block_next(block);
      block->size = half;
      block->is_free = true;
    }
    if (size <= block->size) {
      return block;
    }
  }

  return NULL;
}

static void br__buddy_block_coalescence(br_buddy_block *head, br_buddy_block *tail) {
  for (;;) {
    br_buddy_block *block = head;
    br_buddy_block *buddy = br__buddy_block_next(block);
    bool no_coalescence = true;

    while (block < tail && buddy < tail) {
      if (block->is_free && buddy->is_free && block->size == buddy->size) {
        block->size <<= 1u;
        block = br__buddy_block_next(block);
        if (block < tail) {
          buddy = br__buddy_block_next(block);
          no_coalescence = false;
        }
      } else if (block->size < buddy->size) {
        block = buddy;
        buddy = br__buddy_block_next(buddy);
      } else {
        block = br__buddy_block_next(buddy);
        if (block < tail) {
          buddy = br__buddy_block_next(block);
        }
      }
    }

    if (no_coalescence) {
      return;
    }
  }
}

static br_buddy_block *
br__buddy_block_find_best(br_buddy_block *head, br_buddy_block *tail, usize size) {
  br_buddy_block *best_block = NULL;
  br_buddy_block *block = head;
  br_buddy_block *buddy = br__buddy_block_next(block);

  if (buddy == tail && block->is_free) {
    return br__buddy_block_split(block, size);
  }

  while (block < tail && buddy < tail) {
    if (block->is_free && buddy->is_free && block->size == buddy->size) {
      block->size <<= 1u;
      if (size <= block->size && (best_block == NULL || block->size <= best_block->size)) {
        best_block = block;
      }
      block = br__buddy_block_next(buddy);
      if (block < tail) {
        buddy = br__buddy_block_next(block);
      }
      continue;
    }
    if (block->is_free && size <= block->size &&
        (best_block == NULL || block->size <= best_block->size)) {
      best_block = block;
    }
    if (buddy->is_free && size <= buddy->size &&
        (best_block == NULL || buddy->size < best_block->size)) {
      best_block = buddy;
    }

    if (block->size <= buddy->size) {
      block = br__buddy_block_next(buddy);
      if (block < tail) {
        buddy = br__buddy_block_next(block);
      }
    } else {
      block = buddy;
      buddy = br__buddy_block_next(buddy);
    }
  }

  if (best_block != NULL) {
    return br__buddy_block_split(best_block, size);
  }
  return NULL;
}

static br_status
br__buddy_get_block(const br_buddy_allocator *buddy, void *ptr, br_buddy_block **out_block) {
  uptr head_addr;
  uptr tail_addr;
  uptr ptr_addr;
  uptr block_addr;
  br_buddy_block *block;
  usize min_size;

  if (out_block == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  *out_block = NULL;

  if (ptr == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (br__buddy_validate_initialized(buddy) != BR_STATUS_OK) {
    return BR_STATUS_INVALID_STATE;
  }

  head_addr = (uptr)(void *)buddy->head;
  tail_addr = (uptr)(void *)buddy->tail;
  ptr_addr = (uptr)ptr;
  if (!(head_addr <= ptr_addr && ptr_addr <= tail_addr)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if ((ptr_addr & (uptr)(buddy->alignment - 1u)) != 0u || ptr_addr < head_addr + buddy->alignment) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  block_addr = ptr_addr - buddy->alignment;
  if (!(head_addr <= block_addr && block_addr < tail_addr)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  block = (br_buddy_block *)(void *)block_addr;
  min_size = br__buddy_min_alloc_size(buddy);
  if (!br_is_power_of_two_size(block->size) || block->size < min_size ||
      block->size > tail_addr - block_addr) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if ((uptr)(void *)br__buddy_block_next(block) > tail_addr) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  *out_block = block;
  return BR_STATUS_OK;
}

static br_alloc_result
br__buddy_alloc_internal(br_buddy_allocator *buddy, usize size, bool zeroed) {
  br_buddy_block *found;
  usize actual_size;

  if (buddy == NULL) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (br__buddy_validate_initialized(buddy) != BR_STATUS_OK) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (size == 0u) {
    return br__buddy_result(NULL, 0u, BR_STATUS_OK);
  }

  actual_size = br__buddy_block_size_required_for_alignment(buddy->alignment, size);
  if (actual_size == 0u) {
    return br__buddy_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  found = br__buddy_block_find_best(buddy->head, buddy->tail, actual_size);
  if (found == NULL) {
    br__buddy_block_coalescence(buddy->head, buddy->tail);
    found = br__buddy_block_find_best(buddy->head, buddy->tail, actual_size);
  }
  if (found == NULL) {
    return br__buddy_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  found->is_free = false;
  if (zeroed) {
    memset((u8 *)(void *)found + buddy->alignment, 0, size);
  }
  return br__buddy_result((u8 *)(void *)found + buddy->alignment, size, BR_STATUS_OK);
}

static br_alloc_result br__buddy_resize_internal(br_buddy_allocator *buddy,
                                                 void *ptr,
                                                 usize old_size,
                                                 usize new_size,
                                                 usize alignment,
                                                 bool zeroed) {
  br_buddy_block *block;
  br_alloc_result result;
  br_status status;
  usize payload_size;

  if (buddy == NULL) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (br__buddy_validate_initialized(buddy) != BR_STATUS_OK) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (br__buddy_validate_request_alignment(&alignment) != BR_STATUS_OK) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (ptr == NULL) {
    return br__buddy_alloc_internal(buddy, new_size, zeroed);
  }
  if (new_size == 0u) {
    return br__buddy_result(NULL, 0u, br_buddy_allocator_free(buddy, ptr));
  }
  if (old_size == 0u) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  status = br__buddy_get_block(buddy, ptr, &block);
  if (status != BR_STATUS_OK) {
    return br__buddy_result(NULL, 0u, status);
  }

  /*
  Odin's default resize helper receives a typed `[]byte` old slice. Bedrock's
  raw C resize API gets only `ptr + old_size`, so we bound-check the caller's
  `old_size` against the owning buddy block before copying.
  */
  payload_size = block->size - buddy->alignment;
  if (old_size > payload_size) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (old_size == new_size && ((uptr)ptr & (uptr)(alignment - 1u)) == 0u) {
    return br__buddy_result(ptr, new_size, BR_STATUS_OK);
  }

  result = br__buddy_alloc_internal(buddy, new_size, false);
  if (result.status != BR_STATUS_OK) {
    return result;
  }

  memcpy(result.ptr, ptr, br_min_size(old_size, new_size));
  if (zeroed && new_size > old_size) {
    memset((u8 *)result.ptr + old_size, 0, new_size - old_size);
  }

  (void)br_buddy_allocator_free(buddy, ptr);
  return br__buddy_result(result.ptr, result.size, BR_STATUS_OK);
}

static br_alloc_result br__buddy_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_buddy_allocator *buddy = (br_buddy_allocator *)ctx;

  if (req == NULL) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__buddy_alloc_internal(buddy, req->size, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__buddy_alloc_internal(buddy, req->size, false);
    case BR_ALLOC_OP_RESIZE:
      return br__buddy_resize_internal(
        buddy, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__buddy_resize_internal(
        buddy, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__buddy_result(NULL, 0u, br_buddy_allocator_free(buddy, req->ptr));
    case BR_ALLOC_OP_RESET:
      br_buddy_allocator_free_all(buddy);
      return br__buddy_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

br_status
br_buddy_allocator_init(br_buddy_allocator *buddy, void *buffer, usize capacity, usize alignment) {
  usize min_capacity;

  if (buddy == NULL || buffer == NULL || capacity == 0u || alignment == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (!br_is_power_of_two_size(capacity) || !br_is_power_of_two_size(alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  alignment = br__buddy_normalize_init_alignment(alignment);
  if (((uptr)buffer & (uptr)(alignment - 1u)) != 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  min_capacity = br__buddy_block_size_required_for_alignment(alignment, 1u);
  if (min_capacity == 0u || capacity < min_capacity * 2u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  buddy->head = (br_buddy_block *)buffer;
  buddy->head->size = capacity;
  buddy->head->is_free = true;
  buddy->tail = br__buddy_block_next(buddy->head);
  buddy->alignment = alignment;
  return BR_STATUS_OK;
}

void br_buddy_allocator_free_all(br_buddy_allocator *buddy) {
  void *buffer;
  usize capacity;
  usize alignment;

  if (buddy == NULL || buddy->head == NULL || buddy->tail == NULL || buddy->alignment == 0u) {
    return;
  }

  buffer = (void *)buddy->head;
  capacity = (usize)((u8 *)(void *)buddy->tail - (u8 *)(void *)buddy->head);
  alignment = buddy->alignment;
  (void)br_buddy_allocator_init(buddy, buffer, capacity, alignment);
}

br_alloc_result br_buddy_allocator_alloc(br_buddy_allocator *buddy, usize size) {
  return br__buddy_alloc_internal(buddy, size, true);
}

br_alloc_result br_buddy_allocator_alloc_uninit(br_buddy_allocator *buddy, usize size) {
  return br__buddy_alloc_internal(buddy, size, false);
}

br_status br_buddy_allocator_free(br_buddy_allocator *buddy, void *ptr) {
  br_buddy_block *block;
  br_status status;

  if (ptr == NULL) {
    return BR_STATUS_OK;
  }
  status = br__buddy_get_block(buddy, ptr, &block);
  if (status != BR_STATUS_OK) {
    return status;
  }

  block->is_free = true;
  br__buddy_block_coalescence(buddy->head, buddy->tail);
  return BR_STATUS_OK;
}

br_allocator br_buddy_allocator_allocator(br_buddy_allocator *buddy) {
  br_allocator allocator = {0};

  allocator.fn = br__buddy_allocator_fn;
  allocator.ctx = buddy;
  return allocator;
}

/* ==== src/mem/compat_allocator.c ==== */

typedef struct br__compat_header {
  usize size;
  usize alignment;
} br__compat_header;

BR_STATIC_ASSERT((sizeof(br__compat_header) & (sizeof(br__compat_header) - 1u)) == 0u,
                 "Compat header size must stay a power of two.");

static br_alloc_result br__compat_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static br_status br__compat_validate_alignment(usize *alignment) {
  if (alignment == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (*alignment == 0u) {
    *alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (!br_is_power_of_two_size(*alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  return BR_STATUS_OK;
}

static usize br__compat_prefix_size(usize alignment) {
  return br_max_size(alignment, sizeof(br__compat_header));
}

static br__compat_header *br__compat_header_from_user_ptr(void *ptr) {
  return &((br__compat_header *)ptr)[-1];
}

static br_allocator br__compat_parent(const br_compat_allocator *compat) {
  if (compat != NULL && compat->parent.fn != NULL) {
    return compat->parent;
  }
  return br_allocator_heap();
}

static br_alloc_result
br__compat_alloc_internal(br_compat_allocator *compat, usize size, usize alignment, bool zeroed) {
  br_allocator parent;
  br_alloc_result allocation;
  br__compat_header *header;
  usize prefix;
  usize req_size;
  void *data;
  br_status status;

  if (compat == NULL) {
    return br__compat_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  status = br__compat_validate_alignment(&alignment);
  if (status != BR_STATUS_OK) {
    return br__compat_result(NULL, 0u, status);
  }
  if (size == 0u) {
    return br__compat_result(NULL, 0u, BR_STATUS_OK);
  }

  prefix = br__compat_prefix_size(alignment);
  if (size > SIZE_MAX - prefix) {
    return br__compat_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }
  req_size = size + prefix;

  parent = br__compat_parent(compat);
  allocation = zeroed ? br_allocator_alloc(parent, req_size, alignment)
                      : br_allocator_alloc_uninit(parent, req_size, alignment);
  if (allocation.status != BR_STATUS_OK) {
    return allocation;
  }
  if (allocation.ptr == NULL) {
    return br__compat_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  data = (u8 *)allocation.ptr + prefix;
  header = br__compat_header_from_user_ptr(data);
  header->size = size;
  header->alignment = alignment;
  return br__compat_result(data, size, BR_STATUS_OK);
}

static br_status br__compat_free_internal(br_compat_allocator *compat, void *ptr) {
  br__compat_header *header;
  br_allocator parent;
  usize prefix;
  usize orig_size;

  if (compat == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (ptr == NULL) {
    return BR_STATUS_OK;
  }

  header = br__compat_header_from_user_ptr(ptr);
  prefix = br__compat_prefix_size(header->alignment);
  if (header->size > SIZE_MAX - prefix) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  orig_size = header->size + prefix;
  parent = br__compat_parent(compat);
  return br_allocator_free(parent, (u8 *)ptr - prefix, orig_size);
}

static br_alloc_result br__compat_resize_internal(br_compat_allocator *compat,
                                                  void *ptr,
                                                  usize old_size,
                                                  usize new_size,
                                                  usize alignment,
                                                  bool zeroed) {
  br__compat_header *header;
  br_allocator parent;
  br_alloc_result allocation;
  usize orig_alignment;
  usize orig_prefix;
  usize orig_size;
  usize new_alignment;
  usize new_prefix;
  usize req_size;
  usize bytes_to_move;
  void *data;
  br_status status;

  BR_UNUSED(old_size);

  if (compat == NULL) {
    return br__compat_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  status = br__compat_validate_alignment(&alignment);
  if (status != BR_STATUS_OK) {
    return br__compat_result(NULL, 0u, status);
  }
  if (ptr == NULL) {
    return br__compat_alloc_internal(compat, new_size, alignment, zeroed);
  }
  if (new_size == 0u) {
    return br__compat_result(NULL, 0u, br__compat_free_internal(compat, ptr));
  }

  header = br__compat_header_from_user_ptr(ptr);
  orig_alignment = header->alignment;
  orig_prefix = br__compat_prefix_size(orig_alignment);
  if (header->size > SIZE_MAX - orig_prefix) {
    return br__compat_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  orig_size = header->size + orig_prefix;
  new_alignment = br_max_size(orig_alignment, alignment);
  new_prefix = br__compat_prefix_size(new_alignment);
  if (new_size > SIZE_MAX - new_prefix) {
    return br__compat_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }
  req_size = new_size + new_prefix;

  if (header->size == new_size && orig_alignment == new_alignment) {
    return br__compat_result(ptr, new_size, BR_STATUS_OK);
  }

  parent = br__compat_parent(compat);
  allocation =
    zeroed
      ? br_allocator_resize(parent, (u8 *)ptr - orig_prefix, orig_size, req_size, new_alignment)
      : br_allocator_resize_uninit(
          parent, (u8 *)ptr - orig_prefix, orig_size, req_size, new_alignment);
  if (allocation.status != BR_STATUS_OK) {
    return allocation;
  }
  if (allocation.ptr == NULL) {
    return br__compat_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  /*
  Odin forwards the wrapped allocation directly to the parent resize. Bedrock
  additionally shifts the user payload forward when the compat header prefix
  grows so data remains stable even if the parent resize keeps the same base
  pointer in place.
  */
  bytes_to_move = br_min_size(header->size, new_size);
  if (new_prefix != orig_prefix && bytes_to_move != 0u) {
    memmove((u8 *)allocation.ptr + new_prefix, (u8 *)allocation.ptr + orig_prefix, bytes_to_move);
  }

  data = (u8 *)allocation.ptr + new_prefix;
  header = br__compat_header_from_user_ptr(data);
  header->size = new_size;
  header->alignment = new_alignment;

  if (zeroed && new_size > bytes_to_move) {
    memset((u8 *)data + bytes_to_move, 0, new_size - bytes_to_move);
  }

  return br__compat_result(data, new_size, BR_STATUS_OK);
}

static br_alloc_result br__compat_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_compat_allocator *compat = (br_compat_allocator *)ctx;

  if (req == NULL) {
    return br__compat_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__compat_alloc_internal(compat, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__compat_alloc_internal(compat, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
      return br__compat_resize_internal(
        compat, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__compat_resize_internal(
        compat, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__compat_result(NULL, 0u, br__compat_free_internal(compat, req->ptr));
    case BR_ALLOC_OP_RESET:
      return br__compat_result(NULL, 0u, br_allocator_reset(br__compat_parent(compat)));
  }

  return br__compat_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

void br_compat_allocator_init(br_compat_allocator *compat, br_allocator parent) {
  if (compat == NULL) {
    return;
  }
  if (parent.fn == NULL) {
    parent = br_allocator_heap();
  }
  compat->parent = parent;
}

br_allocator br_compat_allocator_allocator(br_compat_allocator *compat) {
  br_allocator allocator = {0};

  allocator.fn = br__compat_allocator_fn;
  allocator.ctx = compat;
  return allocator;
}

/* ==== src/mem/dynamic_arena.c ==== */

static br_alloc_result br__dynamic_arena_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static br_allocator br__dynamic_arena_block_allocator(const br_dynamic_arena *arena) {
  if (arena != NULL && arena->block_allocator.fn != NULL) {
    return arena->block_allocator;
  }

  return br_allocator_heap();
}

static br_allocator br__dynamic_arena_array_allocator(const br_dynamic_arena *arena) {
  if (arena != NULL && arena->array_allocator.fn != NULL) {
    return arena->array_allocator;
  }

  return br_allocator_heap();
}

static bool br__dynamic_arena_align_size(usize size, usize alignment, usize *result) {
  usize aligned;
  usize mask;

  if (result == NULL) {
    return false;
  }
  if (alignment == 0u || !br_is_power_of_two_size(alignment)) {
    return false;
  }

  mask = alignment - 1u;
  if (size > SIZE_MAX - mask) {
    return false;
  }

  aligned = (size + mask) & ~mask;
  *result = aligned;
  return true;
}

static uptr br__dynamic_arena_align_up_ptr(uptr value, usize alignment) {
  return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static bool br__dynamic_arena_reserve_ptr_array(br_dynamic_arena *arena,
                                                void ***data,
                                                usize *cap,
                                                usize min_cap) {
  br_allocator allocator;
  br_alloc_result resized;
  usize new_cap;

  if (arena == NULL || data == NULL || cap == NULL) {
    return false;
  }
  if (*cap >= min_cap) {
    return true;
  }

  allocator = br__dynamic_arena_array_allocator(arena);
  new_cap = *cap != 0u ? *cap : 8u;
  while (new_cap < min_cap) {
    if (new_cap > SIZE_MAX / 2u) {
      new_cap = min_cap;
      break;
    }
    new_cap *= 2u;
  }

  resized = br_allocator_resize_uninit(
    allocator, *data, *cap * sizeof(**data), new_cap * sizeof(**data), (usize) _Alignof(void *));
  if (resized.status != BR_STATUS_OK) {
    return false;
  }

  *data = (void **)resized.ptr;
  *cap = new_cap;
  return true;
}

static br_status br__dynamic_arena_push_ptr(
  br_dynamic_arena *arena, void ***data, usize *count, usize *cap, void *ptr) {
  if (arena == NULL || data == NULL || count == NULL || cap == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (!br__dynamic_arena_reserve_ptr_array(arena, data, cap, *count + 1u)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  (*data)[*count] = ptr;
  *count += 1u;
  return BR_STATUS_OK;
}

static void *br__dynamic_arena_pop_ptr(void **data, usize *count) {
  if (data == NULL || count == NULL || *count == 0u) {
    return NULL;
  }

  *count -= 1u;
  return data[*count];
}

static br_status br__dynamic_arena_cycle_new_block(br_dynamic_arena *arena, usize alignment) {
  br_alloc_result allocated;
  void *new_block;
  br_status status;

  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (arena->block_allocator.fn == NULL && arena->array_allocator.fn == NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  new_block = br__dynamic_arena_pop_ptr(arena->unused_blocks, &arena->unused_count);
  if (new_block == NULL) {
    allocated = br_allocator_alloc(br__dynamic_arena_block_allocator(arena),
                                   arena->block_size,
                                   br_max_size(arena->minimum_alignment, alignment));
    if (allocated.status != BR_STATUS_OK) {
      return allocated.status;
    }
    new_block = allocated.ptr;
  }

  /*
  Bedrock moves the old current block into `used_blocks` only after the next
  block is secured, so a failed block allocation cannot orphan the arena's
  current block.
  */
  if (arena->current_block != NULL) {
    status = br__dynamic_arena_push_ptr(
      arena, &arena->used_blocks, &arena->used_count, &arena->used_cap, arena->current_block);
    if (status != BR_STATUS_OK) {
      if (arena->unused_count < arena->unused_cap && arena->unused_blocks != NULL) {
        arena->unused_blocks[arena->unused_count] = new_block;
        arena->unused_count += 1u;
      } else if (allocated.ptr == new_block) {
        (void)br_allocator_free(
          br__dynamic_arena_block_allocator(arena), new_block, arena->block_size);
      }
      return status;
    }
  }

  arena->bytes_left = arena->block_size;
  arena->current_pos = (u8 *)new_block;
  arena->current_block = new_block;
  return BR_STATUS_OK;
}

static br_alloc_result br__dynamic_arena_alloc_internal(br_dynamic_arena *arena,
                                                        usize size,
                                                        usize alignment,
                                                        bool zeroed) {
  br_alloc_result allocated;
  br_status status;
  usize actual_alignment;
  usize needed;
  usize margin;
  uptr aligned;
  u8 *memory;

  if (arena == NULL) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (arena->block_allocator.fn == NULL || arena->array_allocator.fn == NULL ||
      arena->block_size == 0u || arena->out_band_size == 0u || arena->minimum_alignment == 0u) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (size == 0u) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OK);
  }

  /*
  Every allocation is floored by the arena's `minimum_alignment` and honors a
  larger per-request alignment, so the effective alignment is
  `max(minimum_alignment, alignment)`. This matches Odin's documented contract
  that all allocations respect `minimum_alignment`, and we apply it to out-band
  allocations too (Odin's out-band path passes the raw request instead). A
  non-power-of-two effective alignment is rejected, consistent with the heap
  allocator's normalize-then-reject rule in `alloc.c`.
  */
  actual_alignment = br_max_size(arena->minimum_alignment, alignment);
  if (!br_is_power_of_two_size(actual_alignment)) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  if (size >= arena->out_band_size) {
    allocated =
      zeroed ? br_allocator_alloc(br__dynamic_arena_array_allocator(arena), size, actual_alignment)
             : br_allocator_alloc_uninit(
                 br__dynamic_arena_array_allocator(arena), size, actual_alignment);
    if (allocated.status != BR_STATUS_OK) {
      return allocated;
    }
    status = br__dynamic_arena_push_ptr(arena,
                                        &arena->out_band_allocations,
                                        &arena->out_band_count,
                                        &arena->out_band_cap,
                                        allocated.ptr);
    if (status != BR_STATUS_OK) {
      (void)br_allocator_free(
        br__dynamic_arena_array_allocator(arena), allocated.ptr, allocated.size);
      return br__dynamic_arena_result(NULL, 0u, status);
    }
    return allocated;
  }

  if (!br__dynamic_arena_align_size(size, actual_alignment, &needed)) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }
  if (needed > arena->block_size) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  /*
  Align the bump pointer forward and account for the alignment margin, following
  Odin's per-request model. Unlike Odin, Bedrock re-aligns against the block it
  actually obtained from `cycle` rather than assuming a zero margin: a block
  reused from `unused_blocks` may have been allocated for a smaller alignment,
  so re-aligning guarantees the returned pointer always satisfies
  `actual_alignment`. A reused block too poorly aligned to fit the request
  reports OUT_OF_MEMORY instead of returning an under-aligned pointer.
  */
  aligned = br__dynamic_arena_align_up_ptr((uptr)(void *)arena->current_pos, actual_alignment);
  margin = (usize)(aligned - (uptr)(void *)arena->current_pos);
  if (arena->current_block == NULL || needed > arena->bytes_left ||
      margin > arena->bytes_left - needed) {
    status = br__dynamic_arena_cycle_new_block(arena, alignment);
    if (status != BR_STATUS_OK) {
      return br__dynamic_arena_result(NULL, 0u, status);
    }
    if (arena->current_block == NULL) {
      return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
    aligned = br__dynamic_arena_align_up_ptr((uptr)(void *)arena->current_pos, actual_alignment);
    margin = (usize)(aligned - (uptr)(void *)arena->current_pos);
    if (needed > arena->bytes_left || margin > arena->bytes_left - needed) {
      return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
  }

  memory = (u8 *)(void *)aligned;
  arena->current_pos = memory + needed;
  arena->bytes_left -= margin + needed;
  if (zeroed) {
    memset(memory, 0, size);
  }
  return br__dynamic_arena_result(memory, size, BR_STATUS_OK);
}

static br_alloc_result br__dynamic_arena_resize_internal(br_dynamic_arena *arena,
                                                         void *ptr,
                                                         usize old_size,
                                                         usize new_size,
                                                         usize alignment,
                                                         bool zeroed) {
  br_alloc_result result;

  if (arena == NULL) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (ptr == NULL) {
    return br__dynamic_arena_alloc_internal(arena, new_size, alignment, zeroed);
  }
  if (new_size == 0u) {
    /*
    Odin's dynamic arena has no individual free operation. A zero-size resize is
    therefore a successful no-op that returns nil.
    */
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OK);
  }
  if (old_size >= new_size) {
    if (zeroed && new_size > old_size) {
      memset((u8 *)ptr + old_size, 0, new_size - old_size);
    }
    return br__dynamic_arena_result(ptr, new_size, BR_STATUS_OK);
  }

  result = br__dynamic_arena_alloc_internal(arena, new_size, alignment, false);
  if (result.status != BR_STATUS_OK) {
    return result;
  }

  memcpy(result.ptr, ptr, old_size);
  if (zeroed && new_size > old_size) {
    memset((u8 *)result.ptr + old_size, 0, new_size - old_size);
  }
  result.status = BR_STATUS_OK;
  return result;
}

static br_alloc_result br__dynamic_arena_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_dynamic_arena *arena = (br_dynamic_arena *)ctx;

  if (req == NULL) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__dynamic_arena_alloc_internal(arena, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__dynamic_arena_alloc_internal(arena, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
      return br__dynamic_arena_resize_internal(
        arena, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__dynamic_arena_resize_internal(
        arena, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__dynamic_arena_result(NULL, 0u, BR_STATUS_NOT_SUPPORTED);
    case BR_ALLOC_OP_RESET:
      br_dynamic_arena_free_all(arena);
      return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

br_status br_dynamic_arena_init(br_dynamic_arena *arena,
                                br_allocator block_allocator,
                                br_allocator array_allocator,
                                usize block_size,
                                usize out_band_size,
                                usize minimum_alignment) {
  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (arena->unused_blocks != NULL || arena->used_blocks != NULL ||
      arena->out_band_allocations != NULL || arena->current_block != NULL ||
      arena->unused_count != 0u || arena->used_count != 0u || arena->out_band_count != 0u) {
    return BR_STATUS_INVALID_STATE;
  }

  if (block_allocator.fn == NULL) {
    block_allocator = br_allocator_heap();
  }
  if (array_allocator.fn == NULL) {
    array_allocator = br_allocator_heap();
  }
  if (block_size == 0u) {
    block_size = BR_DYNAMIC_ARENA_DEFAULT_BLOCK_SIZE;
  }
  if (out_band_size == 0u) {
    out_band_size = BR_DYNAMIC_ARENA_DEFAULT_OUT_BAND_SIZE;
  }
  if (minimum_alignment == 0u) {
    minimum_alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (!br_is_power_of_two_size(minimum_alignment) || block_size == 0u || out_band_size == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  arena->block_size = block_size;
  arena->out_band_size = out_band_size;
  arena->minimum_alignment = minimum_alignment;
  arena->block_allocator = block_allocator;
  arena->array_allocator = array_allocator;
  arena->unused_blocks = NULL;
  arena->unused_count = 0u;
  arena->unused_cap = 0u;
  arena->used_blocks = NULL;
  arena->used_count = 0u;
  arena->used_cap = 0u;
  arena->out_band_allocations = NULL;
  arena->out_band_count = 0u;
  arena->out_band_cap = 0u;
  arena->current_block = NULL;
  arena->current_pos = NULL;
  arena->bytes_left = 0u;
  return BR_STATUS_OK;
}

void br_dynamic_arena_destroy(br_dynamic_arena *arena) {
  br_allocator array_allocator;

  if (arena == NULL) {
    return;
  }

  array_allocator = br__dynamic_arena_array_allocator(arena);
  br_dynamic_arena_free_all(arena);
  if (arena->unused_blocks != NULL) {
    (void)br_allocator_free(
      array_allocator, arena->unused_blocks, arena->unused_cap * sizeof(*arena->unused_blocks));
  }
  if (arena->used_blocks != NULL) {
    (void)br_allocator_free(
      array_allocator, arena->used_blocks, arena->used_cap * sizeof(*arena->used_blocks));
  }
  if (arena->out_band_allocations != NULL) {
    (void)br_allocator_free(array_allocator,
                            arena->out_band_allocations,
                            arena->out_band_cap * sizeof(*arena->out_band_allocations));
  }

  memset(arena, 0, sizeof(*arena));
}

void br_dynamic_arena_reset(br_dynamic_arena *arena) {
  usize i;

  if (arena == NULL) {
    return;
  }

  if (arena->current_block != NULL) {
    if (br__dynamic_arena_push_ptr(arena,
                                   &arena->unused_blocks,
                                   &arena->unused_count,
                                   &arena->unused_cap,
                                   arena->current_block) == BR_STATUS_OK) {
      arena->current_block = NULL;
      arena->current_pos = NULL;
    }
  }

  for (i = 0u; i < arena->used_count; ++i) {
    if (br__dynamic_arena_push_ptr(arena,
                                   &arena->unused_blocks,
                                   &arena->unused_count,
                                   &arena->unused_cap,
                                   arena->used_blocks[i]) != BR_STATUS_OK) {
      break;
    }
  }
  arena->used_count = 0u;

  for (i = 0u; i < arena->out_band_count; ++i) {
    (void)br_allocator_free(
      br__dynamic_arena_array_allocator(arena), arena->out_band_allocations[i], 0u);
  }
  arena->out_band_count = 0u;
  arena->bytes_left = 0u;
  arena->current_pos = NULL;
}

void br_dynamic_arena_free_all(br_dynamic_arena *arena) {
  usize i;

  if (arena == NULL) {
    return;
  }

  br_dynamic_arena_reset(arena);
  if (arena->current_block != NULL) {
    (void)br_allocator_free(
      br__dynamic_arena_block_allocator(arena), arena->current_block, arena->block_size);
    arena->current_block = NULL;
    arena->current_pos = NULL;
  }
  for (i = 0u; i < arena->used_count; ++i) {
    (void)br_allocator_free(
      br__dynamic_arena_block_allocator(arena), arena->used_blocks[i], arena->block_size);
  }
  arena->used_count = 0u;
  for (i = 0u; i < arena->unused_count; ++i) {
    (void)br_allocator_free(
      br__dynamic_arena_block_allocator(arena), arena->unused_blocks[i], arena->block_size);
  }
  arena->unused_count = 0u;
}

br_alloc_result br_dynamic_arena_alloc(br_dynamic_arena *arena, usize size) {
  return br__dynamic_arena_alloc_internal(arena, size, 0u, true);
}

br_alloc_result br_dynamic_arena_alloc_uninit(br_dynamic_arena *arena, usize size) {
  return br__dynamic_arena_alloc_internal(arena, size, 0u, false);
}

br_alloc_result
br_dynamic_arena_resize(br_dynamic_arena *arena, void *ptr, usize old_size, usize new_size) {
  return br__dynamic_arena_resize_internal(arena, ptr, old_size, new_size, 0u, true);
}

br_alloc_result
br_dynamic_arena_resize_uninit(br_dynamic_arena *arena, void *ptr, usize old_size, usize new_size) {
  return br__dynamic_arena_resize_internal(arena, ptr, old_size, new_size, 0u, false);
}

br_allocator br_dynamic_arena_allocator(br_dynamic_arena *arena) {
  br_allocator allocator;

  allocator.fn = br__dynamic_arena_allocator_fn;
  allocator.ctx = arena;
  return allocator;
}

/* ==== src/mem/mutex_allocator.c ==== */

static br_alloc_result br__mutex_allocator_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static br_allocator br__mutex_allocator_backing(const br_mutex_allocator *mutex_allocator) {
  if (mutex_allocator != NULL && mutex_allocator->backing.fn != NULL) {
    return mutex_allocator->backing;
  }

  return br_allocator_heap();
}

static br_alloc_result br__mutex_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_mutex_allocator *mutex_allocator = (br_mutex_allocator *)ctx;
  br_allocator backing;
  br_alloc_result result;

  if (mutex_allocator == NULL || req == NULL) {
    return br__mutex_allocator_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  backing = br__mutex_allocator_backing(mutex_allocator);
  br_mutex_lock(&mutex_allocator->mutex);
  result = br_allocator_call(backing, req);
  br_mutex_unlock(&mutex_allocator->mutex);
  return result;
}

void br_mutex_allocator_init(br_mutex_allocator *mutex_allocator, br_allocator backing) {
  if (mutex_allocator == NULL) {
    return;
  }

  if (backing.fn == NULL) {
    backing = br_allocator_heap();
  }

  mutex_allocator->backing = backing;
  mutex_allocator->mutex = (br_mutex)BR_MUTEX_INIT;
}

br_allocator br_mutex_allocator_as_allocator(br_mutex_allocator *mutex_allocator) {
  br_allocator allocator;

  allocator.fn = br__mutex_allocator_fn;
  allocator.ctx = mutex_allocator;
  return allocator;
}

/* ==== src/mem/rollback_stack.c ==== */

#define BR__ROLLBACK_HEADER_PREV_OFFSET_MASK UINT64_C(0xffffffff)
#define BR__ROLLBACK_HEADER_IS_FREE_SHIFT 32u
#define BR__ROLLBACK_HEADER_PREV_PTR_SHIFT 33u
#define BR__ROLLBACK_HEADER_PREV_PTR_MASK UINT64_C(0x7fffffff)

typedef u64 br__rollback_header_bits;

struct br_rollback_stack_block {
  struct br_rollback_stack_block *next_block;
  void *last_alloc;
  usize offset;
  usize capacity;
  u8 buffer[];
};

static br_alloc_result br__rollback_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static bool br__rollback_add_overflow(usize a, usize b, usize *out) {
  if (a > SIZE_MAX - b) {
    return true;
  }

  if (out != NULL) {
    *out = a + b;
  }
  return false;
}

static br_allocator br__rollback_block_allocator(br_allocator allocator) {
  if (allocator.fn != NULL) {
    return allocator;
  }

  return br_allocator_heap();
}

static usize br__rollback_normalize_alignment(usize alignment) {
  usize min_alignment = (usize) _Alignof(br__rollback_header_bits);

  if (alignment == 0u) {
    alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (alignment < min_alignment) {
    alignment = min_alignment;
  }

  return alignment;
}

static uptr br__rollback_align_up(uptr value, usize alignment) {
  return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static usize br__rollback_calc_padding_with_header(uptr start, usize alignment) {
  uptr user_ptr;

  user_ptr = br__rollback_align_up(start + sizeof(br__rollback_header_bits), alignment);
  return (usize)(user_ptr - start);
}

static br__rollback_header_bits
br__rollback_pack_header(usize prev_offset, usize prev_ptr, bool is_free) {
  br__rollback_header_bits bits = 0u;

  bits |= (br__rollback_header_bits)(prev_offset & BR__ROLLBACK_HEADER_PREV_OFFSET_MASK);
  bits |= (br__rollback_header_bits)(is_free ? 1u : 0u) << BR__ROLLBACK_HEADER_IS_FREE_SHIFT;
  bits |= (br__rollback_header_bits)(prev_ptr & BR__ROLLBACK_HEADER_PREV_PTR_MASK)
          << BR__ROLLBACK_HEADER_PREV_PTR_SHIFT;
  return bits;
}

static usize br__rollback_header_prev_offset(br__rollback_header_bits bits) {
  return (usize)(bits & BR__ROLLBACK_HEADER_PREV_OFFSET_MASK);
}

static usize br__rollback_header_prev_ptr(br__rollback_header_bits bits) {
  return (usize)((bits >> BR__ROLLBACK_HEADER_PREV_PTR_SHIFT) & BR__ROLLBACK_HEADER_PREV_PTR_MASK);
}

static bool br__rollback_header_is_free(br__rollback_header_bits bits) {
  return ((bits >> BR__ROLLBACK_HEADER_IS_FREE_SHIFT) & 1u) != 0u;
}

static br__rollback_header_bits br__rollback_header_set_free(br__rollback_header_bits bits,
                                                             bool is_free) {
  bits &= ~((br__rollback_header_bits)1u << BR__ROLLBACK_HEADER_IS_FREE_SHIFT);
  bits |= (br__rollback_header_bits)(is_free ? 1u : 0u) << BR__ROLLBACK_HEADER_IS_FREE_SHIFT;
  return bits;
}

static br__rollback_header_bits *br__rollback_header_ptr(void *ptr) {
  return (br__rollback_header_bits *)((u8 *)ptr - sizeof(br__rollback_header_bits));
}

static bool br__rollback_ptr_in_bounds(const br_rollback_stack_block *block, const void *ptr) {
  const u8 *start;
  const u8 *end;
  const u8 *memory = (const u8 *)ptr;

  if (block == NULL || ptr == NULL) {
    return false;
  }

  start = block->buffer;
  end = block->buffer + block->offset;
  return start < memory && memory <= end;
}

static br_status br__rollback_find_ptr(br_rollback_stack *stack,
                                       void *ptr,
                                       br_rollback_stack_block **parent_out,
                                       br_rollback_stack_block **block_out,
                                       br__rollback_header_bits **header_out) {
  br_rollback_stack_block *parent = NULL;
  br_rollback_stack_block *block;

  if (stack == NULL || ptr == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  for (block = stack->head; block != NULL; block = block->next_block) {
    if (br__rollback_ptr_in_bounds(block, ptr)) {
      if (parent_out != NULL) {
        *parent_out = parent;
      }
      if (block_out != NULL) {
        *block_out = block;
      }
      if (header_out != NULL) {
        *header_out = br__rollback_header_ptr(ptr);
      }
      return BR_STATUS_OK;
    }
    parent = block;
  }

  return BR_STATUS_INVALID_ARGUMENT;
}

static bool br__rollback_find_last_alloc(br_rollback_stack *stack,
                                         void *ptr,
                                         br_rollback_stack_block **block_out,
                                         br__rollback_header_bits **header_out) {
  br_rollback_stack_block *block;

  if (stack == NULL || ptr == NULL) {
    return false;
  }

  for (block = stack->head; block != NULL; block = block->next_block) {
    if (block->last_alloc == ptr) {
      if (block_out != NULL) {
        *block_out = block;
      }
      if (header_out != NULL) {
        *header_out = br__rollback_header_ptr(ptr);
      }
      return true;
    }
  }

  return false;
}

static bool br__rollback_block_is_singleton(const br_rollback_stack *stack,
                                            const br_rollback_stack_block *block) {
  return stack != NULL && block != NULL && block->capacity > stack->block_size;
}

static void br__rollback_collapse_freed_tail(br_rollback_stack_block *block) {
  while (block != NULL && block->offset > 0u && block->last_alloc != NULL) {
    br__rollback_header_bits bits = *br__rollback_header_ptr(block->last_alloc);
    usize prev_offset;
    usize prev_ptr;

    if (!br__rollback_header_is_free(bits)) {
      break;
    }

    prev_offset = br__rollback_header_prev_offset(bits);
    prev_ptr = br__rollback_header_prev_ptr(bits);
    block->offset = prev_offset;

    /*
    Bedrock stops here instead of blindly stepping to `prev_ptr - header_size`
    when the chain reaches the start of the block. This avoids underflow on the
    no-previous-allocation case while preserving Odin's rollback behavior.
    */
    if (block->offset == 0u || prev_ptr == 0u) {
      block->last_alloc = NULL;
      break;
    }

    block->last_alloc = block->buffer + prev_ptr;
  }
}

static br_status br__rollback_make_block(usize capacity,
                                         br_allocator allocator,
                                         br_rollback_stack_block **block_out) {
  br_alloc_result allocated;
  usize total_size;
  br_rollback_stack_block *block;

  if (block_out == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (br__rollback_add_overflow(offsetof(br_rollback_stack_block, buffer), capacity, &total_size)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  allocated =
    br_allocator_alloc_uninit(allocator, total_size, (usize) _Alignof(br_rollback_stack_block));
  if (allocated.status != BR_STATUS_OK || allocated.ptr == NULL) {
    return allocated.status;
  }

  block = (br_rollback_stack_block *)allocated.ptr;
  memset(block, 0, offsetof(br_rollback_stack_block, buffer));
  block->capacity = capacity;
  *block_out = block;
  return BR_STATUS_OK;
}

static void br__rollback_free_block(br_rollback_stack_block *block, br_allocator allocator) {
  usize total_size;

  if (block == NULL) {
    return;
  }

  total_size = offsetof(br_rollback_stack_block, buffer) + block->capacity;
  (void)br_allocator_free(allocator, block, total_size);
}

static void br__rollback_reset_head(br_rollback_stack *stack) {
  if (stack == NULL || stack->head == NULL) {
    return;
  }

  stack->head->next_block = NULL;
  stack->head->last_alloc = NULL;
  stack->head->offset = 0u;
}

static void br__rollback_free_all_extra_blocks(br_rollback_stack *stack) {
  br_rollback_stack_block *block;
  br_rollback_stack_block *next;
  br_allocator allocator;

  if (stack == NULL || stack->head == NULL || stack->block_allocator.fn == NULL) {
    return;
  }

  allocator = br__rollback_block_allocator(stack->block_allocator);
  block = stack->head->next_block;
  while (block != NULL) {
    next = block->next_block;
    br__rollback_free_block(block, allocator);
    block = next;
  }

  br__rollback_reset_head(stack);
}

static br_alloc_result
br__rollback_alloc(br_rollback_stack *stack, usize size, usize alignment, bool zeroed) {
  br_rollback_stack_block *block;
  br_rollback_stack_block *parent = NULL;
  br_allocator allocator;

  if (stack == NULL || stack->head == NULL) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }

  alignment = br__rollback_normalize_alignment(alignment);
  if (!br_is_power_of_two_size(alignment)) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (size == 0u) {
    return br__rollback_result(NULL, 0u, BR_STATUS_OK);
  }

  allocator = br__rollback_block_allocator(stack->block_allocator);

  for (block = stack->head;; block = block->next_block) {
    usize minimum_size_required;
    usize new_block_size;
    uptr start_addr;
    usize padding;
    usize needed;
    u8 *ptr;
    br__rollback_header_bits *header_ptr;
    usize prev_ptr;

    if (block == NULL) {
      br_status status;

      if (stack->block_allocator.fn == NULL) {
        return br__rollback_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
      }

      minimum_size_required = sizeof(br__rollback_header_bits);
      if (br__rollback_add_overflow(
            minimum_size_required, alignment - 1u, &minimum_size_required) ||
          br__rollback_add_overflow(minimum_size_required, size, &minimum_size_required)) {
        return br__rollback_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
      }
      new_block_size = stack->block_size;
      if (new_block_size < minimum_size_required) {
        new_block_size = minimum_size_required;
      }

      status = br__rollback_make_block(new_block_size, allocator, &block);
      if (status != BR_STATUS_OK) {
        return br__rollback_result(NULL, 0u, status);
      }

      parent->next_block = block;
    }

    start_addr = (uptr)(block->buffer + block->offset);
    padding = br__rollback_calc_padding_with_header(start_addr, alignment);
    if (br__rollback_add_overflow(padding, size, &needed) ||
        needed > block->capacity - br_min_size(block->offset, block->capacity)) {
      parent = block;
      continue;
    }
    if (block->offset > block->capacity || needed > block->capacity - block->offset) {
      parent = block;
      continue;
    }

    prev_ptr = block->last_alloc != NULL ? (usize)((u8 *)block->last_alloc - block->buffer) : 0u;
    if (block->offset > UINT32_MAX || prev_ptr > BR_ROLLBACK_STACK_MAX_HEAD_BLOCK_SIZE) {
      return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_STATE);
    }

    ptr = block->buffer + block->offset + padding;
    header_ptr = (br__rollback_header_bits *)(void *)(ptr - sizeof(br__rollback_header_bits));
    *header_ptr = br__rollback_pack_header(block->offset, prev_ptr, false);

    block->last_alloc = ptr;
    block->offset += needed;
    if (br__rollback_block_is_singleton(stack, block)) {
      block->offset = block->capacity;
    }

    if (zeroed) {
      memset(ptr, 0, size);
    }

    return br__rollback_result(ptr, size, BR_STATUS_OK);
  }
}

static br_status br__rollback_free(br_rollback_stack *stack, void *ptr) {
  br_rollback_stack_block *parent = NULL;
  br_rollback_stack_block *block = NULL;
  br__rollback_header_bits *header_ptr = NULL;
  br__rollback_header_bits bits;
  br_status status;

  if (stack == NULL || stack->head == NULL) {
    return BR_STATUS_INVALID_STATE;
  }
  if (ptr == NULL) {
    return BR_STATUS_OK;
  }

  status = br__rollback_find_ptr(stack, ptr, &parent, &block, &header_ptr);
  if (status != BR_STATUS_OK) {
    return status;
  }

  bits = *header_ptr;
  if (br__rollback_header_is_free(bits)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  *header_ptr = br__rollback_header_set_free(bits, true);
  if (block->last_alloc == ptr) {
    block->offset = br__rollback_header_prev_offset(bits);
    if (block->offset == 0u || br__rollback_header_prev_ptr(bits) == 0u) {
      block->last_alloc = NULL;
    } else {
      block->last_alloc = block->buffer + br__rollback_header_prev_ptr(bits);
      br__rollback_collapse_freed_tail(block);
    }
  }

  if (parent != NULL && block->offset == 0u) {
    parent->next_block = block->next_block;
    br__rollback_free_block(block, br__rollback_block_allocator(stack->block_allocator));
  }

  return BR_STATUS_OK;
}

static br_alloc_result br__rollback_resize(br_rollback_stack *stack,
                                           void *ptr,
                                           usize old_size,
                                           usize new_size,
                                           usize alignment,
                                           bool zeroed) {
  br_rollback_stack_block *block = NULL;
  br__rollback_header_bits *header_ptr = NULL;
  br_alloc_result result;
  usize new_offset;
  usize copy_size;

  if (stack == NULL || stack->head == NULL) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }

  alignment = br__rollback_normalize_alignment(alignment);
  if (!br_is_power_of_two_size(alignment)) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  if (ptr == NULL) {
    return br__rollback_alloc(stack, new_size, alignment, zeroed);
  }
  if (new_size == 0u) {
    return br__rollback_result(NULL, 0u, br__rollback_free(stack, ptr));
  }
  if (old_size == 0u) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  if (br__rollback_find_last_alloc(stack, ptr, &block, &header_ptr)) {
    if (block->offset < old_size) {
      return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
    }
    if (new_size <= old_size) {
      if (!br__rollback_block_is_singleton(stack, block)) {
        block->offset -= old_size - new_size;
      }
      return br__rollback_result(ptr, new_size, BR_STATUS_OK);
    }

    if (new_size - old_size <= block->capacity - block->offset) {
      if (!br__rollback_block_is_singleton(stack, block)) {
        new_offset = block->offset + (new_size - old_size);
        block->offset = new_offset;
      }
      if (zeroed) {
        memset((u8 *)ptr + old_size, 0, new_size - old_size);
      }
      return br__rollback_result(ptr, new_size, BR_STATUS_OK);
    }
  }

  if (br__rollback_find_ptr(stack, ptr, NULL, NULL, NULL) != BR_STATUS_OK) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  result = br__rollback_alloc(stack, new_size, alignment, false);
  if (result.status != BR_STATUS_OK) {
    return result;
  }

  /*
  Odin delegates this copy to runtime helpers. Bedrock copies the smaller of
  the two sizes explicitly so C callers do not rely on implicit truncation.
  */
  copy_size = br_min_size(old_size, new_size);
  memcpy(result.ptr, ptr, copy_size);
  if (zeroed && new_size > copy_size) {
    memset((u8 *)result.ptr + copy_size, 0, new_size - copy_size);
  }

  result.status = br__rollback_free(stack, ptr);
  if (result.status != BR_STATUS_OK) {
    return br__rollback_result(NULL, 0u, result.status);
  }
  result.status = BR_STATUS_OK;
  return result;
}

static br_alloc_result br__rollback_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_rollback_stack *stack = (br_rollback_stack *)ctx;

  if (req == NULL) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__rollback_alloc(stack, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__rollback_alloc(stack, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
      return br__rollback_resize(stack, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__rollback_resize(stack, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__rollback_result(NULL, 0u, br__rollback_free(stack, req->ptr));
    case BR_ALLOC_OP_RESET:
      br_rollback_stack_reset(stack);
      return br__rollback_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

br_status br_rollback_stack_init_buffered(br_rollback_stack *stack, void *buffer, usize capacity) {
  br_rollback_stack_block *block;
  usize min_capacity =
    offsetof(br_rollback_stack_block, buffer) + sizeof(br__rollback_header_bits) + sizeof(void *);

  if (stack == NULL || buffer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (capacity < min_capacity) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (capacity - offsetof(br_rollback_stack_block, buffer) >
      BR_ROLLBACK_STACK_MAX_HEAD_BLOCK_SIZE) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  block = (br_rollback_stack_block *)buffer;
  memset(block, 0, offsetof(br_rollback_stack_block, buffer));
  block->capacity = capacity - offsetof(br_rollback_stack_block, buffer);

  memset(stack, 0, sizeof(*stack));
  stack->head = block;
  stack->block_size = block->capacity;
  return BR_STATUS_OK;
}

br_status br_rollback_stack_init_dynamic(br_rollback_stack *stack,
                                         usize block_size,
                                         br_allocator block_allocator) {
  br_rollback_stack_block *block;
  br_status status;
  usize min_capacity = sizeof(br__rollback_header_bits) + sizeof(void *);

  if (stack == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (block_size == 0u) {
    block_size = BR_ROLLBACK_STACK_DEFAULT_BLOCK_SIZE;
  }
  if (block_size < min_capacity || block_size > BR_ROLLBACK_STACK_MAX_HEAD_BLOCK_SIZE) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  block_allocator = br__rollback_block_allocator(block_allocator);
  status = br__rollback_make_block(block_size, block_allocator, &block);
  if (status != BR_STATUS_OK) {
    return status;
  }

  memset(stack, 0, sizeof(*stack));
  stack->head = block;
  stack->block_size = block_size;
  stack->block_allocator = block_allocator;
  stack->head_owned = true;
  return BR_STATUS_OK;
}

void br_rollback_stack_destroy(br_rollback_stack *stack) {
  br_allocator allocator;

  if (stack == NULL || stack->head == NULL) {
    return;
  }

  allocator = br__rollback_block_allocator(stack->block_allocator);
  br__rollback_free_all_extra_blocks(stack);
  if (stack->head_owned) {
    br__rollback_free_block(stack->head, allocator);
  }

  memset(stack, 0, sizeof(*stack));
}

void br_rollback_stack_reset(br_rollback_stack *stack) {
  if (stack == NULL || stack->head == NULL) {
    return;
  }

  br__rollback_free_all_extra_blocks(stack);
}

br_alloc_result br_rollback_stack_alloc(br_rollback_stack *stack, usize size, usize alignment) {
  return br__rollback_alloc(stack, size, alignment, true);
}

br_alloc_result
br_rollback_stack_alloc_uninit(br_rollback_stack *stack, usize size, usize alignment) {
  return br__rollback_alloc(stack, size, alignment, false);
}

br_alloc_result br_rollback_stack_resize(
  br_rollback_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__rollback_resize(stack, ptr, old_size, new_size, alignment, true);
}

br_alloc_result br_rollback_stack_resize_uninit(
  br_rollback_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__rollback_resize(stack, ptr, old_size, new_size, alignment, false);
}

br_status br_rollback_stack_free(br_rollback_stack *stack, void *ptr) {
  return br__rollback_free(stack, ptr);
}

br_allocator br_rollback_stack_allocator(br_rollback_stack *stack) {
  br_allocator allocator;

  allocator.fn = br__rollback_allocator_fn;
  allocator.ctx = stack;
  return allocator;
}

/* ==== src/mem/scratch.c ==== */

static br_alloc_result br__scratch_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static uptr br__scratch_align_up(uptr value, usize alignment) {
  return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static bool br__scratch_is_self_allocator(const br_scratch *scratch, br_allocator allocator) {
  return scratch != NULL && allocator.ctx == (void *)scratch;
}

static br_allocator_fn br__scratch_allocator_fn_ptr(void);

static br_allocator br__scratch_backup_allocator(const br_scratch *scratch) {
  br_allocator allocator = {0};

  if (scratch != NULL && scratch->backup_allocator.fn != NULL) {
    allocator = scratch->backup_allocator;
  } else {
    allocator = br_allocator_heap();
  }

  if (allocator.fn == br__scratch_allocator_fn_ptr() &&
      br__scratch_is_self_allocator(scratch, allocator)) {
    allocator.fn = NULL;
    allocator.ctx = NULL;
  }

  return allocator;
}

static bool br__scratch_reserve_leaks(br_scratch *scratch, usize min_cap) {
  br_allocator allocator;
  br_alloc_result resized;
  usize new_cap;

  if (scratch == NULL) {
    return false;
  }
  if (scratch->leaked_cap >= min_cap) {
    return true;
  }

  allocator = br__scratch_backup_allocator(scratch);
  if (allocator.fn == NULL) {
    return false;
  }

  new_cap = scratch->leaked_cap != 0u ? scratch->leaked_cap : 8u;
  while (new_cap < min_cap) {
    if (new_cap > SIZE_MAX / 2u) {
      new_cap = min_cap;
      break;
    }
    new_cap *= 2u;
  }

  resized = br_allocator_resize_uninit(allocator,
                                       scratch->leaked_allocations,
                                       scratch->leaked_cap * sizeof(*scratch->leaked_allocations),
                                       new_cap * sizeof(*scratch->leaked_allocations),
                                       (usize) _Alignof(br_scratch_leaked_allocation));
  if (resized.status != BR_STATUS_OK) {
    return false;
  }

  scratch->leaked_allocations = (br_scratch_leaked_allocation *)resized.ptr;
  scratch->leaked_cap = new_cap;
  return true;
}

static br_status br__scratch_ensure_initialized(br_scratch *scratch) {
  br_allocator allocator;

  if (scratch == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (scratch->data != NULL) {
    return BR_STATUS_OK;
  }

  allocator = br__scratch_backup_allocator(scratch);
  if (allocator.fn == NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  return br_scratch_init(scratch, BR_SCRATCH_DEFAULT_BACKING_SIZE, allocator);
}

static br_status br__scratch_validate_alignment(usize *alignment) {
  if (alignment == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (*alignment == 0u) {
    *alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (!br_is_power_of_two_size(*alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  return BR_STATUS_OK;
}

static br_alloc_result
br__scratch_alloc_internal(br_scratch *scratch, usize size, usize alignment, bool zeroed) {
  br_allocator backup_allocator;
  br_alloc_result result;
  br_status status;
  usize aligned_size;

  if (scratch == NULL) {
    return br__scratch_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  status = br__scratch_validate_alignment(&alignment);
  if (status != BR_STATUS_OK) {
    return br__scratch_result(NULL, 0u, status);
  }
  if (size == 0u) {
    return br__scratch_result(NULL, 0u, BR_STATUS_OK);
  }

  status = br__scratch_ensure_initialized(scratch);
  if (status != BR_STATUS_OK) {
    return br__scratch_result(NULL, 0u, status);
  }

  aligned_size = size;
  if (alignment > 1u) {
    if (aligned_size > SIZE_MAX - (alignment - 1u)) {
      return br__scratch_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
    aligned_size += alignment - 1u;
  }

  if (scratch->curr_offset <= scratch->capacity &&
      aligned_size <= scratch->capacity - scratch->curr_offset) {
    uptr offset = (uptr)scratch->curr_offset;
    u8 *root = scratch->data + scratch->curr_offset;
    u8 *ptr = root;

    scratch->prev_allocation_root = root;
    if (((uptr)ptr & (uptr)(alignment - 1u)) != 0u) {
      ptr = (u8 *)(void *)br__scratch_align_up((uptr)ptr, alignment);
    }

    scratch->prev_allocation = ptr;
    scratch->curr_offset = (usize)offset + aligned_size;
    if (zeroed) {
      memset(ptr, 0, size);
    }
    return br__scratch_result(ptr, size, BR_STATUS_OK);
  }

  backup_allocator = br__scratch_backup_allocator(scratch);
  if (backup_allocator.fn == NULL) {
    return br__scratch_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }

  result = zeroed ? br_allocator_alloc(backup_allocator, size, alignment)
                  : br_allocator_alloc_uninit(backup_allocator, size, alignment);
  if (result.status != BR_STATUS_OK || result.ptr == NULL) {
    return result;
  }

  if (!br__scratch_reserve_leaks(scratch, scratch->leaked_count + 1u)) {
    (void)br_allocator_free(backup_allocator, result.ptr, result.size);
    return br__scratch_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  scratch->leaked_allocations[scratch->leaked_count].memory = result.ptr;
  scratch->leaked_allocations[scratch->leaked_count].size = result.size;
  scratch->leaked_count += 1u;
  return result;
}

static br_status br__scratch_remove_leak(br_scratch *scratch, usize index) {
  br_allocator allocator;
  br_status status;

  if (scratch == NULL || index >= scratch->leaked_count) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  allocator = br__scratch_backup_allocator(scratch);
  if (allocator.fn == NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  status = br_allocator_free(
    allocator, scratch->leaked_allocations[index].memory, scratch->leaked_allocations[index].size);
  if (status != BR_STATUS_OK) {
    return status;
  }

  if (index + 1u < scratch->leaked_count) {
    memmove(&scratch->leaked_allocations[index],
            &scratch->leaked_allocations[index + 1u],
            (scratch->leaked_count - index - 1u) * sizeof(*scratch->leaked_allocations));
  }
  scratch->leaked_count -= 1u;
  return BR_STATUS_OK;
}

static br_alloc_result br__scratch_resize_internal(
  br_scratch *scratch, void *ptr, usize old_size, usize new_size, usize alignment, bool zeroed) {
  br_alloc_result result;
  br_status status;

  if (scratch == NULL) {
    return br__scratch_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  status = br__scratch_validate_alignment(&alignment);
  if (status != BR_STATUS_OK) {
    return br__scratch_result(NULL, 0u, status);
  }
  if (ptr == NULL) {
    return br__scratch_alloc_internal(scratch, new_size, alignment, zeroed);
  }
  if (new_size == 0u) {
    return br__scratch_result(NULL, 0u, br_scratch_free(scratch, ptr));
  }
  if (old_size == 0u) {
    return br__scratch_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  status = br__scratch_ensure_initialized(scratch);
  if (status != BR_STATUS_OK) {
    return br__scratch_result(NULL, 0u, status);
  }

  if (scratch->prev_allocation == ptr && (((uptr)ptr & (uptr)(alignment - 1u)) == 0u) &&
      (uptr)ptr <= (uptr)(scratch->data + scratch->capacity) &&
      new_size < (usize)((scratch->data + scratch->capacity) - (u8 *)ptr)) {
    scratch->curr_offset = (usize)((u8 *)ptr - scratch->data) + new_size;
    if (zeroed && new_size > old_size) {
      memset((u8 *)ptr + old_size, 0, new_size - old_size);
    }
    return br__scratch_result(ptr, new_size, BR_STATUS_OK);
  }

  result = br__scratch_alloc_internal(scratch, new_size, alignment, false);
  if (result.status != BR_STATUS_OK) {
    return result;
  }

  memcpy(result.ptr, ptr, br_min_size(old_size, new_size));
  if (zeroed && new_size > old_size) {
    memset((u8 *)result.ptr + old_size, 0, new_size - old_size);
  }

  status = br_scratch_free(scratch, ptr);
  if (status != BR_STATUS_OK) {
    return br__scratch_result(NULL, 0u, status);
  }

  result.status = BR_STATUS_OK;
  return result;
}

static br_alloc_result br__scratch_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_scratch *scratch = (br_scratch *)ctx;

  if (req == NULL) {
    return br__scratch_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__scratch_alloc_internal(scratch, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__scratch_alloc_internal(scratch, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
      return br__scratch_resize_internal(
        scratch, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__scratch_resize_internal(
        scratch, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__scratch_result(NULL, 0u, br_scratch_free(scratch, req->ptr));
    case BR_ALLOC_OP_RESET:
      br_scratch_free_all(scratch);
      return br__scratch_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__scratch_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

static br_allocator_fn br__scratch_allocator_fn_ptr(void) {
  return br__scratch_allocator_fn;
}

br_status br_scratch_init(br_scratch *scratch, usize size, br_allocator backup_allocator) {
  br_alloc_result result;

  if (scratch == NULL || size == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (scratch->data != NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  if (backup_allocator.fn == NULL) {
    backup_allocator = br_allocator_heap();
  }
  if (backup_allocator.fn == br__scratch_allocator_fn_ptr() &&
      backup_allocator.ctx == (void *)scratch) {
    return BR_STATUS_INVALID_STATE;
  }

  result = br_allocator_alloc(backup_allocator, size, (usize)(2u * _Alignof(void *)));
  if (result.status != BR_STATUS_OK) {
    return result.status;
  }

  scratch->data = (u8 *)result.ptr;
  scratch->capacity = result.size;
  scratch->curr_offset = 0u;
  scratch->prev_allocation = NULL;
  scratch->prev_allocation_root = NULL;
  scratch->backup_allocator = backup_allocator;
  scratch->leaked_allocations = NULL;
  scratch->leaked_count = 0u;
  scratch->leaked_cap = 0u;
  return BR_STATUS_OK;
}

void br_scratch_destroy(br_scratch *scratch) {
  br_allocator allocator;
  usize i;

  if (scratch == NULL) {
    return;
  }

  allocator = br__scratch_backup_allocator(scratch);
  for (i = 0u; i < scratch->leaked_count; ++i) {
    if (allocator.fn != NULL) {
      (void)br_allocator_free(
        allocator, scratch->leaked_allocations[i].memory, scratch->leaked_allocations[i].size);
    }
  }
  if (scratch->leaked_allocations != NULL && allocator.fn != NULL) {
    (void)br_allocator_free(allocator,
                            scratch->leaked_allocations,
                            scratch->leaked_cap * sizeof(*scratch->leaked_allocations));
  }
  if (scratch->data != NULL && allocator.fn != NULL) {
    (void)br_allocator_free(allocator, scratch->data, scratch->capacity);
  }

  memset(scratch, 0, sizeof(*scratch));
}

void br_scratch_free_all(br_scratch *scratch) {
  br_allocator allocator;
  usize i;

  if (scratch == NULL || scratch->data == NULL) {
    return;
  }

  allocator = br__scratch_backup_allocator(scratch);
  scratch->curr_offset = 0u;
  scratch->prev_allocation = NULL;
  scratch->prev_allocation_root = NULL;

  if (allocator.fn != NULL) {
    for (i = 0u; i < scratch->leaked_count; ++i) {
      (void)br_allocator_free(
        allocator, scratch->leaked_allocations[i].memory, scratch->leaked_allocations[i].size);
    }
  }
  scratch->leaked_count = 0u;
}

br_alloc_result br_scratch_alloc(br_scratch *scratch, usize size, usize alignment) {
  return br__scratch_alloc_internal(scratch, size, alignment, true);
}

br_alloc_result br_scratch_alloc_uninit(br_scratch *scratch, usize size, usize alignment) {
  return br__scratch_alloc_internal(scratch, size, alignment, false);
}

br_status br_scratch_free(br_scratch *scratch, void *ptr) {
  uptr start;
  uptr end;
  uptr old_ptr;
  usize i;

  if (scratch == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (scratch->data == NULL) {
    return BR_STATUS_INVALID_STATE;
  }
  if (ptr == NULL) {
    return BR_STATUS_OK;
  }

  start = (uptr)scratch->data;
  end = start + scratch->capacity;
  old_ptr = (uptr)ptr;

  if (scratch->prev_allocation == ptr) {
    scratch->curr_offset = (usize)((u8 *)scratch->prev_allocation_root - scratch->data);
    scratch->prev_allocation = NULL;
    scratch->prev_allocation_root = NULL;
    return BR_STATUS_OK;
  }
  if (start <= old_ptr && old_ptr < end) {
    return BR_STATUS_OK;
  }

  for (i = 0u; i < scratch->leaked_count; ++i) {
    /*
    Bedrock compares against the requested pointer explicitly here so leaked
    allocations behave as documented even if the current Odin loop shadowing is
    read literally.
    */
    if (scratch->leaked_allocations[i].memory == ptr) {
      return br__scratch_remove_leak(scratch, i);
    }
  }

  return BR_STATUS_INVALID_ARGUMENT;
}

br_alloc_result
br_scratch_resize(br_scratch *scratch, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__scratch_resize_internal(scratch, ptr, old_size, new_size, alignment, true);
}

br_alloc_result br_scratch_resize_uninit(
  br_scratch *scratch, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__scratch_resize_internal(scratch, ptr, old_size, new_size, alignment, false);
}

br_allocator br_scratch_allocator(br_scratch *scratch) {
  br_allocator allocator;

  allocator.fn = br__scratch_allocator_fn;
  allocator.ctx = scratch;
  return allocator;
}

/* ==== src/mem/small_stack.c ==== */

typedef struct br__small_stack_header {
  u8 padding;
} br__small_stack_header;

static br_alloc_result br__small_stack_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static usize br__small_stack_max_alignment(void) {
  return sizeof(usize) * 4u;
}

static usize br__small_stack_normalize_alignment(usize alignment) {
  if (alignment == 0u) {
    alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (alignment < 1u) {
    alignment = 1u;
  }
  if (alignment > br__small_stack_max_alignment()) {
    alignment = br__small_stack_max_alignment();
  }

  return alignment;
}

static uptr br__small_stack_align_up(uptr value, usize alignment) {
  return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static usize br__small_stack_calc_padding_with_header(uptr start, usize alignment) {
  uptr user_ptr = br__small_stack_align_up(start + sizeof(br__small_stack_header), alignment);
  return (usize)(user_ptr - start);
}

static br_status br__small_stack_validate_alignment(usize *alignment) {
  if (alignment == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  *alignment = br__small_stack_normalize_alignment(*alignment);
  if (!br_is_power_of_two_size(*alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  return BR_STATUS_OK;
}

static br_alloc_result
br__small_stack_alloc_internal(br_small_stack *stack, usize size, usize alignment, bool zeroed) {
  uptr curr_addr;
  uptr next_addr;
  usize padding;
  br__small_stack_header *header;

  if (stack == NULL) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (stack->data == NULL) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (br__small_stack_validate_alignment(&alignment) != BR_STATUS_OK) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (size == 0u) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_OK);
  }

  curr_addr = (uptr)stack->data + stack->offset;
  padding = br__small_stack_calc_padding_with_header(curr_addr, alignment);
  if (padding > UINT8_MAX || stack->offset > stack->capacity ||
      padding > stack->capacity - stack->offset ||
      size > stack->capacity - stack->offset - padding) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  stack->offset += padding;
  next_addr = curr_addr + padding;
  header = (br__small_stack_header *)(void *)(next_addr - sizeof(br__small_stack_header));
  header->padding = (u8)padding;
  stack->offset += size;
  stack->peak_used = br_max_size(stack->peak_used, stack->offset);

  if (zeroed) {
    memset((void *)next_addr, 0, size);
  }

  return br__small_stack_result((void *)next_addr, size, BR_STATUS_OK);
}

static br_alloc_result br__small_stack_resize_fallback(
  br_small_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment, bool zeroed) {
  br_alloc_result result;

  result = br__small_stack_alloc_internal(stack, new_size, alignment, false);
  if (result.status != BR_STATUS_OK) {
    return result;
  }

  memcpy(result.ptr, ptr, br_min_size(old_size, new_size));
  if (zeroed && new_size > old_size) {
    memset((u8 *)result.ptr + old_size, 0, new_size - old_size);
  }

  result.status = BR_STATUS_OK;
  return result;
}

static br_alloc_result br__small_stack_resize_internal(
  br_small_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment, bool zeroed) {
  uptr start;
  uptr end;
  uptr curr_addr;

  if (stack == NULL) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (stack->data == NULL) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (br__small_stack_validate_alignment(&alignment) != BR_STATUS_OK) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (ptr == NULL) {
    return br__small_stack_alloc_internal(stack, new_size, alignment, zeroed);
  }
  if (new_size == 0u) {
    return br__small_stack_result(NULL, 0u, br_small_stack_free(stack, ptr));
  }

  start = (uptr)stack->data;
  end = start + stack->capacity;
  curr_addr = (uptr)ptr;
  if (!(start <= curr_addr && curr_addr < end)) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  /*
  Keep Odin's current small-stack behavior here: a pointer already above the
  current offset is treated like a tolerated double free in resize paths too.
  */
  if (curr_addr >= start + stack->offset) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_OK);
  }

  if ((curr_addr & (uptr)(alignment - 1u)) != 0u) {
    return br__small_stack_resize_fallback(stack, ptr, old_size, new_size, alignment, zeroed);
  }
  if (old_size == new_size) {
    return br__small_stack_result(ptr, new_size, BR_STATUS_OK);
  }

  return br__small_stack_resize_fallback(stack, ptr, old_size, new_size, alignment, zeroed);
}

static br_alloc_result br__small_stack_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_small_stack *stack = (br_small_stack *)ctx;

  if (req == NULL) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__small_stack_alloc_internal(stack, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__small_stack_alloc_internal(stack, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
      return br__small_stack_resize_internal(
        stack, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__small_stack_resize_internal(
        stack, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__small_stack_result(NULL, 0u, br_small_stack_free(stack, req->ptr));
    case BR_ALLOC_OP_RESET:
      br_small_stack_free_all(stack);
      return br__small_stack_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

void br_small_stack_init(br_small_stack *stack, void *buffer, usize capacity) {
  if (stack == NULL) {
    return;
  }

  stack->data = (u8 *)buffer;
  stack->capacity = capacity;
  stack->offset = 0u;
  stack->peak_used = 0u;
}

void br_small_stack_free_all(br_small_stack *stack) {
  if (stack == NULL) {
    return;
  }

  stack->offset = 0u;
}

br_alloc_result br_small_stack_alloc(br_small_stack *stack, usize size, usize alignment) {
  return br__small_stack_alloc_internal(stack, size, alignment, true);
}

br_alloc_result br_small_stack_alloc_uninit(br_small_stack *stack, usize size, usize alignment) {
  return br__small_stack_alloc_internal(stack, size, alignment, false);
}

br_status br_small_stack_free(br_small_stack *stack, void *ptr) {
  uptr start;
  uptr end;
  uptr curr_addr;
  br__small_stack_header *header;

  if (stack == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (stack->data == NULL) {
    return BR_STATUS_INVALID_STATE;
  }
  if (ptr == NULL) {
    return BR_STATUS_OK;
  }

  start = (uptr)stack->data;
  end = start + stack->capacity;
  curr_addr = (uptr)ptr;
  if (!(start <= curr_addr && curr_addr < end)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (curr_addr >= start + stack->offset) {
    return BR_STATUS_OK;
  }

  header = (br__small_stack_header *)(void *)(curr_addr - sizeof(br__small_stack_header));
  stack->offset = (usize)(curr_addr - (uptr)header->padding - start);
  return BR_STATUS_OK;
}

br_alloc_result br_small_stack_resize(
  br_small_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__small_stack_resize_internal(stack, ptr, old_size, new_size, alignment, true);
}

br_alloc_result br_small_stack_resize_uninit(
  br_small_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__small_stack_resize_internal(stack, ptr, old_size, new_size, alignment, false);
}

br_allocator br_small_stack_allocator(br_small_stack *stack) {
  br_allocator allocator;

  allocator.fn = br__small_stack_allocator_fn;
  allocator.ctx = stack;
  return allocator;
}

/* ==== src/mem/stack.c ==== */

typedef struct br__stack_header {
  usize prev_offset;
  usize padding;
} br__stack_header;

static br_alloc_result br__stack_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static usize br__stack_normalize_alignment(usize alignment) {
  if (alignment == 0u) {
    alignment = BR_DEFAULT_ALIGNMENT;
  }

  return alignment;
}

static uptr br__stack_align_up(uptr value, usize alignment) {
  return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static usize br__stack_calc_padding_with_header(uptr start, usize alignment) {
  uptr user_ptr = br__stack_align_up(start + sizeof(br__stack_header), alignment);
  return (usize)(user_ptr - start);
}

static br_status br__stack_validate_alignment(usize *alignment) {
  if (alignment == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  *alignment = br__stack_normalize_alignment(*alignment);
  if (!br_is_power_of_two_size(*alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  return BR_STATUS_OK;
}

static br_alloc_result
br__stack_alloc_internal(br_stack *stack, usize size, usize alignment, bool zeroed) {
  uptr curr_addr;
  uptr next_addr;
  usize old_prev_offset;
  usize padding;
  br__stack_header *header;

  if (stack == NULL) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (stack->data == NULL) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (br__stack_validate_alignment(&alignment) != BR_STATUS_OK) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (size == 0u) {
    return br__stack_result(NULL, 0u, BR_STATUS_OK);
  }

  curr_addr = (uptr)stack->data + stack->curr_offset;
  padding = br__stack_calc_padding_with_header(curr_addr, alignment);
  if (stack->curr_offset > stack->capacity || padding > stack->capacity - stack->curr_offset ||
      size > stack->capacity - stack->curr_offset - padding) {
    return br__stack_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  old_prev_offset = stack->prev_offset;
  stack->prev_offset = stack->curr_offset;
  stack->curr_offset += padding;
  next_addr = curr_addr + padding;
  header = (br__stack_header *)(void *)(next_addr - sizeof(br__stack_header));
  header->padding = padding;
  header->prev_offset = old_prev_offset;
  stack->curr_offset += size;
  stack->peak_used = br_max_size(stack->peak_used, stack->curr_offset);

  if (zeroed) {
    memset((void *)next_addr, 0, size);
  }

  return br__stack_result((void *)next_addr, size, BR_STATUS_OK);
}

static br_alloc_result br__stack_fallback_resize(
  br_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment, bool zeroed) {
  br_alloc_result result;

  result = br__stack_alloc_internal(stack, new_size, alignment, false);
  if (result.status != BR_STATUS_OK) {
    return result;
  }

  memcpy(result.ptr, ptr, br_min_size(old_size, new_size));
  if (zeroed && new_size > old_size) {
    memset((u8 *)result.ptr + old_size, 0, new_size - old_size);
  }

  result.status = BR_STATUS_OK;
  return result;
}

static br_alloc_result br__stack_resize_internal(
  br_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment, bool zeroed) {
  uptr start;
  uptr end;
  uptr curr_addr;
  usize old_offset;
  usize user_offset;
  usize old_memory_size;
  br__stack_header *header;

  if (stack == NULL) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (stack->data == NULL) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (br__stack_validate_alignment(&alignment) != BR_STATUS_OK) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (ptr == NULL) {
    return br__stack_alloc_internal(stack, new_size, alignment, zeroed);
  }
  if (new_size == 0u) {
    return br__stack_result(NULL, 0u, br_stack_free(stack, ptr));
  }
  if (old_size == 0u) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  start = (uptr)stack->data;
  end = start + stack->capacity;
  curr_addr = (uptr)ptr;
  if (!(start <= curr_addr && curr_addr < end)) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  /*
  Keep Odin's current "already above curr_offset" behavior here: resizing an
  already-freed pointer is treated as a tolerated no-op rather than a hard
  error.
  */
  if (curr_addr >= start + stack->curr_offset) {
    return br__stack_result(NULL, 0u, BR_STATUS_OK);
  }

  if ((curr_addr & (uptr)(alignment - 1u)) != 0u) {
    return br__stack_fallback_resize(stack, ptr, old_size, new_size, alignment, zeroed);
  }
  if (old_size == new_size) {
    return br__stack_result(ptr, new_size, BR_STATUS_OK);
  }

  header = (br__stack_header *)(void *)(curr_addr - sizeof(br__stack_header));
  old_offset = (usize)(curr_addr - (uptr)header->padding - start);

  /*
  Bedrock checks against the stack's current `prev_offset` so resize uses the
  same last-allocation rule as `br_stack_free`. Odin's current source compares
  against `header.prev_offset`, which appears inconsistent with its free path
  and would only permit in-place resize for the first allocation in the stack.
  */
  if (old_offset != stack->prev_offset) {
    return br__stack_fallback_resize(stack, ptr, old_size, new_size, alignment, zeroed);
  }

  user_offset = (usize)(curr_addr - start);
  old_memory_size = stack->curr_offset - user_offset;

  /*
  Odin asserts here. Bedrock returns a status instead so a bad `old_size`
  cannot silently corrupt the stack's running offset in C callers.
  */
  if (old_memory_size != old_size) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (user_offset > stack->capacity || new_size > stack->capacity - user_offset) {
    return br__stack_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  stack->curr_offset = user_offset + new_size;
  stack->peak_used = br_max_size(stack->peak_used, stack->curr_offset);
  if (zeroed && new_size > old_size) {
    memset((u8 *)ptr + old_size, 0, new_size - old_size);
  }

  return br__stack_result(ptr, new_size, BR_STATUS_OK);
}

static br_alloc_result br__stack_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_stack *stack = (br_stack *)ctx;

  if (req == NULL) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__stack_alloc_internal(stack, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__stack_alloc_internal(stack, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
      return br__stack_resize_internal(
        stack, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__stack_resize_internal(
        stack, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__stack_result(NULL, 0u, br_stack_free(stack, req->ptr));
    case BR_ALLOC_OP_RESET:
      br_stack_free_all(stack);
      return br__stack_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

void br_stack_init(br_stack *stack, void *buffer, usize capacity) {
  if (stack == NULL) {
    return;
  }

  stack->data = (u8 *)buffer;
  stack->capacity = capacity;
  stack->prev_offset = 0u;
  stack->curr_offset = 0u;
  stack->peak_used = 0u;
}

void br_stack_free_all(br_stack *stack) {
  if (stack == NULL) {
    return;
  }

  stack->prev_offset = 0u;
  stack->curr_offset = 0u;
}

br_alloc_result br_stack_alloc(br_stack *stack, usize size, usize alignment) {
  return br__stack_alloc_internal(stack, size, alignment, true);
}

br_alloc_result br_stack_alloc_uninit(br_stack *stack, usize size, usize alignment) {
  return br__stack_alloc_internal(stack, size, alignment, false);
}

br_status br_stack_free(br_stack *stack, void *ptr) {
  uptr start;
  uptr end;
  uptr curr_addr;
  br__stack_header *header;
  usize old_offset;

  if (stack == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (stack->data == NULL) {
    return BR_STATUS_INVALID_STATE;
  }
  if (ptr == NULL) {
    return BR_STATUS_OK;
  }

  start = (uptr)stack->data;
  end = start + stack->capacity;
  curr_addr = (uptr)ptr;
  if (!(start <= curr_addr && curr_addr < end)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (curr_addr >= start + stack->curr_offset) {
    return BR_STATUS_OK;
  }

  header = (br__stack_header *)(void *)(curr_addr - sizeof(br__stack_header));
  old_offset = (usize)(curr_addr - (uptr)header->padding - start);
  if (old_offset != stack->prev_offset) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  stack->prev_offset = header->prev_offset;
  stack->curr_offset = old_offset;
  return BR_STATUS_OK;
}

br_alloc_result
br_stack_resize(br_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__stack_resize_internal(stack, ptr, old_size, new_size, alignment, true);
}

br_alloc_result br_stack_resize_uninit(
  br_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__stack_resize_internal(stack, ptr, old_size, new_size, alignment, false);
}

br_allocator br_stack_allocator(br_stack *stack) {
  br_allocator allocator;

  allocator.fn = br__stack_allocator_fn;
  allocator.ctx = stack;
  return allocator;
}

/* ==== src/mem/tracking_allocator.c ==== */

typedef enum br__tracking_slot_state {
  BR__TRACKING_SLOT_EMPTY = 0,
  BR__TRACKING_SLOT_FULL,
  BR__TRACKING_SLOT_TOMBSTONE
} br__tracking_slot_state;

typedef struct br__tracking_index_slot {
  void *memory;
  usize entry_index;
  br__tracking_slot_state state;
} br__tracking_index_slot;

struct br_tracking_allocator_internal {
  br__tracking_index_slot *slots;
  usize slot_cap;
  usize slot_count;
  usize tombstone_count;
};

static br_alloc_result br__tracking_alloc_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static br_allocator br__tracking_internals_allocator(const br_tracking_allocator *tracking) {
  if (tracking != NULL && tracking->internals.fn != NULL) {
    return tracking->internals;
  }

  return br_allocator_heap();
}

static bool br__tracking_reserve_entries(br_tracking_allocator *tracking, usize min_cap) {
  br_allocator allocator;
  br_alloc_result resized;
  usize new_cap;

  if (tracking == NULL) {
    return false;
  }
  if (tracking->entry_cap >= min_cap) {
    return true;
  }

  allocator = br__tracking_internals_allocator(tracking);
  new_cap = tracking->entry_cap != 0u ? tracking->entry_cap : 16u;
  while (new_cap < min_cap) {
    if (new_cap > SIZE_MAX / 2u) {
      new_cap = min_cap;
      break;
    }
    new_cap *= 2u;
  }

  resized = br_allocator_resize_uninit(allocator,
                                       tracking->entries,
                                       tracking->entry_cap * sizeof(*tracking->entries),
                                       new_cap * sizeof(*tracking->entries),
                                       (usize) _Alignof(br_tracking_allocator_entry));
  if (resized.status != BR_STATUS_OK) {
    return false;
  }

  tracking->entries = (br_tracking_allocator_entry *)resized.ptr;
  tracking->entry_cap = new_cap;
  return true;
}

static bool br__tracking_reserve_bad_frees(br_tracking_allocator *tracking, usize min_cap) {
  br_allocator allocator;
  br_alloc_result resized;
  usize new_cap;

  if (tracking == NULL) {
    return false;
  }
  if (tracking->bad_free_cap >= min_cap) {
    return true;
  }

  allocator = br__tracking_internals_allocator(tracking);
  new_cap = tracking->bad_free_cap != 0u ? tracking->bad_free_cap : 8u;
  while (new_cap < min_cap) {
    if (new_cap > SIZE_MAX / 2u) {
      new_cap = min_cap;
      break;
    }
    new_cap *= 2u;
  }

  resized = br_allocator_resize_uninit(allocator,
                                       tracking->bad_frees,
                                       tracking->bad_free_cap * sizeof(*tracking->bad_frees),
                                       new_cap * sizeof(*tracking->bad_frees),
                                       (usize) _Alignof(br_tracking_allocator_bad_free));
  if (resized.status != BR_STATUS_OK) {
    return false;
  }

  tracking->bad_frees = (br_tracking_allocator_bad_free *)resized.ptr;
  tracking->bad_free_cap = new_cap;
  return true;
}

static bool br__tracking_ensure_internal(br_tracking_allocator *tracking) {
  br_allocator allocator;
  br_alloc_result allocated;

  if (tracking == NULL) {
    return false;
  }
  if (tracking->internal != NULL) {
    return true;
  }

  allocator = br__tracking_internals_allocator(tracking);
  allocated = br_allocator_alloc(allocator,
                                 sizeof(br_tracking_allocator_internal),
                                 (usize) _Alignof(br_tracking_allocator_internal));
  if (allocated.status != BR_STATUS_OK || allocated.ptr == NULL) {
    return false;
  }

  memset(allocated.ptr, 0, sizeof(br_tracking_allocator_internal));
  tracking->internal = (br_tracking_allocator_internal *)allocated.ptr;
  return true;
}

static usize br__tracking_hash_ptr(const void *memory) {
  uptr x = (uptr)memory;

  x ^= x >> 20u;
  x ^= x >> 12u;
  x ^= x >> 7u;
  x ^= x >> 4u;
  return (usize)x;
}

static usize br__tracking_index_target_cap(usize min_live) {
  usize cap = 32u;

  while (cap / 2u < min_live) {
    if (cap > SIZE_MAX / 2u) {
      return cap;
    }
    cap *= 2u;
  }

  return cap;
}

static usize br__tracking_index_find_slot_in_array(const br__tracking_index_slot *slots,
                                                   usize cap,
                                                   const void *memory) {
  usize mask;
  usize slot_index;

  if (slots == NULL || cap == 0u || memory == NULL) {
    return SIZE_MAX;
  }

  mask = cap - 1u;
  slot_index = br__tracking_hash_ptr(memory) & mask;

  for (;;) {
    const br__tracking_index_slot *slot = &slots[slot_index];

    if (slot->state == BR__TRACKING_SLOT_EMPTY) {
      return SIZE_MAX;
    }
    if (slot->state == BR__TRACKING_SLOT_FULL && slot->memory == memory) {
      return slot_index;
    }

    slot_index = (slot_index + 1u) & mask;
  }
}

static void br__tracking_index_insert_into_array(br__tracking_index_slot *slots,
                                                 usize cap,
                                                 void *memory,
                                                 usize entry_index) {
  usize mask;
  usize slot_index;

  mask = cap - 1u;
  slot_index = br__tracking_hash_ptr(memory) & mask;

  while (slots[slot_index].state == BR__TRACKING_SLOT_FULL) {
    slot_index = (slot_index + 1u) & mask;
  }

  slots[slot_index].memory = memory;
  slots[slot_index].entry_index = entry_index;
  slots[slot_index].state = BR__TRACKING_SLOT_FULL;
}

static bool br__tracking_index_rehash(br_tracking_allocator *tracking, usize min_live) {
  br_tracking_allocator_internal *internal;
  br_allocator allocator;
  br_alloc_result allocated;
  br__tracking_index_slot *new_slots;
  usize old_slot_count;
  usize new_cap;
  usize i;

  if (!br__tracking_ensure_internal(tracking)) {
    return false;
  }

  internal = tracking->internal;
  allocator = br__tracking_internals_allocator(tracking);
  new_cap = br__tracking_index_target_cap(min_live);
  if (internal->slot_cap > new_cap) {
    new_cap = internal->slot_cap;
  }

  allocated = br_allocator_alloc(
    allocator, new_cap * sizeof(*new_slots), (usize) _Alignof(br__tracking_index_slot));
  if (allocated.status != BR_STATUS_OK || allocated.ptr == NULL) {
    return false;
  }

  new_slots = (br__tracking_index_slot *)allocated.ptr;
  old_slot_count = internal->slot_count;
  if (internal->slots != NULL) {
    for (i = 0u; i < internal->slot_cap; ++i) {
      if (internal->slots[i].state == BR__TRACKING_SLOT_FULL) {
        br__tracking_index_insert_into_array(
          new_slots, new_cap, internal->slots[i].memory, internal->slots[i].entry_index);
      }
    }

    (void)br_allocator_free(
      allocator, internal->slots, internal->slot_cap * sizeof(*internal->slots));
  }

  internal->slots = new_slots;
  internal->slot_cap = new_cap;
  internal->slot_count = old_slot_count;
  internal->tombstone_count = 0u;
  return true;
}

static bool br__tracking_index_maybe_grow(br_tracking_allocator *tracking, usize additional_live) {
  br_tracking_allocator_internal *internal;
  usize target_live;

  if (!br__tracking_ensure_internal(tracking)) {
    return false;
  }

  internal = tracking->internal;
  target_live = internal->slot_count + additional_live;
  if (internal->slot_cap == 0u || target_live > internal->slot_cap / 2u ||
      internal->tombstone_count > internal->slot_cap / 4u) {
    return br__tracking_index_rehash(tracking, target_live);
  }

  return true;
}

static bool
br__tracking_index_insert(br_tracking_allocator *tracking, void *memory, usize entry_index) {
  br_tracking_allocator_internal *internal;
  usize mask;
  usize slot_index;
  usize first_tombstone;

  if (tracking == NULL || memory == NULL) {
    return false;
  }
  if (!br__tracking_index_maybe_grow(tracking, 1u)) {
    return false;
  }

  internal = tracking->internal;
  mask = internal->slot_cap - 1u;
  slot_index = br__tracking_hash_ptr(memory) & mask;
  first_tombstone = SIZE_MAX;

  for (;;) {
    br__tracking_index_slot *slot = &internal->slots[slot_index];

    if (slot->state == BR__TRACKING_SLOT_EMPTY) {
      if (first_tombstone != SIZE_MAX) {
        slot = &internal->slots[first_tombstone];
        internal->tombstone_count -= 1u;
      }

      slot->memory = memory;
      slot->entry_index = entry_index;
      slot->state = BR__TRACKING_SLOT_FULL;
      internal->slot_count += 1u;
      return true;
    }
    if (slot->state == BR__TRACKING_SLOT_FULL && slot->memory == memory) {
      slot->entry_index = entry_index;
      return true;
    }
    if (slot->state == BR__TRACKING_SLOT_TOMBSTONE && first_tombstone == SIZE_MAX) {
      first_tombstone = slot_index;
    }

    slot_index = (slot_index + 1u) & mask;
  }
}

static bool br__tracking_index_insert_without_grow(br_tracking_allocator *tracking,
                                                   void *memory,
                                                   usize entry_index) {
  br_tracking_allocator_internal *internal;
  usize mask;
  usize slot_index;
  usize first_tombstone;

  if (tracking == NULL || tracking->internal == NULL || memory == NULL) {
    return false;
  }

  internal = tracking->internal;
  if (internal->slot_cap == 0u) {
    return false;
  }

  mask = internal->slot_cap - 1u;
  slot_index = br__tracking_hash_ptr(memory) & mask;
  first_tombstone = SIZE_MAX;

  for (;;) {
    br__tracking_index_slot *slot = &internal->slots[slot_index];

    if (slot->state == BR__TRACKING_SLOT_EMPTY) {
      if (first_tombstone != SIZE_MAX) {
        slot = &internal->slots[first_tombstone];
        internal->tombstone_count -= 1u;
      }

      slot->memory = memory;
      slot->entry_index = entry_index;
      slot->state = BR__TRACKING_SLOT_FULL;
      internal->slot_count += 1u;
      return true;
    }
    if (slot->state == BR__TRACKING_SLOT_FULL && slot->memory == memory) {
      slot->entry_index = entry_index;
      return true;
    }
    if (slot->state == BR__TRACKING_SLOT_TOMBSTONE && first_tombstone == SIZE_MAX) {
      first_tombstone = slot_index;
    }

    slot_index = (slot_index + 1u) & mask;
  }
}

static bool br__tracking_index_find(const br_tracking_allocator *tracking,
                                    const void *memory,
                                    usize *entry_index_out) {
  const br_tracking_allocator_internal *internal;
  usize slot_index;

  if (tracking == NULL || tracking->internal == NULL || memory == NULL) {
    return false;
  }

  internal = tracking->internal;
  slot_index = br__tracking_index_find_slot_in_array(internal->slots, internal->slot_cap, memory);
  if (slot_index == SIZE_MAX) {
    return false;
  }

  if (entry_index_out != NULL) {
    *entry_index_out = internal->slots[slot_index].entry_index;
  }
  return true;
}

static bool
br__tracking_index_update(br_tracking_allocator *tracking, const void *memory, usize entry_index) {
  br_tracking_allocator_internal *internal;
  usize slot_index;

  if (tracking == NULL || tracking->internal == NULL || memory == NULL) {
    return false;
  }

  internal = tracking->internal;
  slot_index = br__tracking_index_find_slot_in_array(internal->slots, internal->slot_cap, memory);
  if (slot_index == SIZE_MAX) {
    return false;
  }

  internal->slots[slot_index].entry_index = entry_index;
  return true;
}

static bool br__tracking_index_remove(br_tracking_allocator *tracking,
                                      const void *memory,
                                      usize *entry_index_out) {
  br_tracking_allocator_internal *internal;
  usize slot_index;

  if (tracking == NULL || tracking->internal == NULL || memory == NULL) {
    return false;
  }

  internal = tracking->internal;
  slot_index = br__tracking_index_find_slot_in_array(internal->slots, internal->slot_cap, memory);
  if (slot_index == SIZE_MAX) {
    return false;
  }

  if (entry_index_out != NULL) {
    *entry_index_out = internal->slots[slot_index].entry_index;
  }

  internal->slots[slot_index].memory = NULL;
  internal->slots[slot_index].entry_index = 0u;
  internal->slots[slot_index].state = BR__TRACKING_SLOT_TOMBSTONE;
  internal->slot_count -= 1u;
  internal->tombstone_count += 1u;

  if (internal->slot_count == 0u) {
    memset(internal->slots, 0, internal->slot_cap * sizeof(*internal->slots));
    internal->tombstone_count = 0u;
  } else if (internal->tombstone_count > internal->slot_cap / 4u) {
    (void)br__tracking_index_rehash(tracking, internal->slot_count);
  }

  return true;
}

static void br__tracking_index_clear(br_tracking_allocator *tracking) {
  br_tracking_allocator_internal *internal;

  if (tracking == NULL || tracking->internal == NULL) {
    return;
  }

  internal = tracking->internal;
  if (internal->slots != NULL) {
    memset(internal->slots, 0, internal->slot_cap * sizeof(*internal->slots));
  }
  internal->slot_count = 0u;
  internal->tombstone_count = 0u;
}

static void br__tracking_record_alloc(br_tracking_allocator *tracking, usize size) {
  tracking->stats.total_memory_allocated += size;
  tracking->stats.total_allocation_count += 1u;
  tracking->stats.current_memory_allocated += size;
  if (tracking->stats.current_memory_allocated > tracking->stats.peak_memory_allocated) {
    tracking->stats.peak_memory_allocated = tracking->stats.current_memory_allocated;
  }
}

static void br__tracking_record_free(br_tracking_allocator *tracking, usize size) {
  tracking->stats.total_memory_freed += size;
  tracking->stats.total_free_count += 1u;
  tracking->stats.current_memory_allocated -= size;
}

static void br__tracking_note_bad_free(br_tracking_allocator *tracking, void *memory, usize size) {
  br_tracking_allocator_bad_free bad_free;

  if (tracking == NULL || memory == NULL) {
    return;
  }

  bad_free.memory = memory;
  bad_free.size = size;

  if (tracking->bad_free_fn != NULL) {
    tracking->bad_free_fn(tracking->bad_free_ctx, &bad_free);
    return;
  }

  if (!br__tracking_reserve_bad_frees(tracking, tracking->bad_free_count + 1u)) {
    return;
  }

  tracking->bad_frees[tracking->bad_free_count] = bad_free;
  tracking->bad_free_count += 1u;
}

static void br__tracking_clear_unlocked(br_tracking_allocator *tracking) {
  br__tracking_index_clear(tracking);
  tracking->entry_count = 0u;
  tracking->bad_free_count = 0u;
  tracking->stats.current_memory_allocated = 0u;
  tracking->stats.live_allocation_count = 0u;
}

static void br__tracking_reset_unlocked(br_tracking_allocator *tracking) {
  br__tracking_index_clear(tracking);
  tracking->entry_count = 0u;
  tracking->bad_free_count = 0u;
  memset(&tracking->stats, 0, sizeof(tracking->stats));
}

static bool br__tracking_add_entry(br_tracking_allocator *tracking,
                                   void *memory,
                                   usize size,
                                   usize alignment,
                                   br_alloc_op op,
                                   br_status status) {
  br_tracking_allocator_entry *entry;
  usize entry_index;

  if (tracking == NULL || memory == NULL || size == 0u) {
    return true;
  }
  if (!br__tracking_reserve_entries(tracking, tracking->entry_count + 1u)) {
    return false;
  }

  entry_index = tracking->entry_count;
  if (!br__tracking_index_insert(tracking, memory, entry_index)) {
    return false;
  }

  entry = &tracking->entries[entry_index];
  entry->memory = memory;
  entry->size = size;
  entry->alignment = alignment;
  entry->op = op;
  entry->status = status;
  tracking->entry_count += 1u;

  br__tracking_record_alloc(tracking, size);
  tracking->stats.live_allocation_count += 1u;
  return true;
}

static void br__tracking_remove_entry(br_tracking_allocator *tracking, usize entry_index) {
  br_tracking_allocator_entry entry;
  usize last_index;

  if (tracking == NULL || entry_index >= tracking->entry_count) {
    return;
  }

  entry = tracking->entries[entry_index];
  br__tracking_record_free(tracking, entry.size);
  tracking->stats.live_allocation_count -= 1u;

  last_index = tracking->entry_count - 1u;
  if (entry_index != last_index) {
    br_tracking_allocator_entry moved = tracking->entries[last_index];

    tracking->entries[entry_index] = moved;
    /*
    Bedrock keeps the public `entries` array dense for leak inspection, so
    deletions swap-remove here and then retarget the private pointer index.
    */
    (void)br__tracking_index_update(tracking, moved.memory, entry_index);
  }

  tracking->entry_count -= 1u;
}

static void br__tracking_resize_entry(br_tracking_allocator *tracking,
                                      usize entry_index,
                                      void *old_memory,
                                      void *new_memory,
                                      usize new_size,
                                      usize new_alignment,
                                      br_alloc_op op,
                                      br_status status) {
  br_tracking_allocator_entry *entry;
  usize old_size;

  if (tracking == NULL || entry_index >= tracking->entry_count) {
    return;
  }

  entry = &tracking->entries[entry_index];
  old_size = entry->size;

  br__tracking_record_free(tracking, old_size);
  br__tracking_record_alloc(tracking, new_size);

  if (new_memory != old_memory) {
    (void)br__tracking_index_remove(tracking, old_memory, NULL);
    /*
    This is a same-live-count update, so Bedrock reuses the existing table
    capacity instead of risking an allocation failure during reinsertion.
    */
    (void)br__tracking_index_insert_without_grow(tracking, new_memory, entry_index);
  }

  entry->memory = new_memory;
  entry->size = new_size;
  entry->alignment = new_alignment;
  entry->op = op;
  entry->status = status;
}

static br_alloc_result br__tracking_allocator_fn_unlocked(br_tracking_allocator *tracking,
                                                          const br_alloc_request *req) {
  br_alloc_result result;
  usize entry_index;

  if (req == NULL) {
    return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (tracking == NULL) {
    return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
    case BR_ALLOC_OP_ALLOC_UNINIT:
      result = br_allocator_call(tracking->backing, req);
      if (result.status != BR_STATUS_OK) {
        return result;
      }
      if (!br__tracking_add_entry(
            tracking, result.ptr, result.size, req->alignment, req->op, result.status)) {
        if (result.ptr != NULL) {
          (void)br_allocator_free(tracking->backing, result.ptr, result.size);
        }
        return br__tracking_alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
      }
      return result;

    case BR_ALLOC_OP_FREE:
      if (req->ptr == NULL) {
        return br__tracking_alloc_result(NULL, 0u, BR_STATUS_OK);
      }
      if (!br__tracking_index_find(tracking, req->ptr, &entry_index)) {
        br__tracking_note_bad_free(tracking, req->ptr, req->old_size);
        return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
      }

      result = br_allocator_call(tracking->backing, req);
      if (result.status != BR_STATUS_OK) {
        return result;
      }

      (void)br__tracking_index_remove(tracking, req->ptr, NULL);
      br__tracking_remove_entry(tracking, entry_index);
      return result;

    case BR_ALLOC_OP_RESET:
      result = br_allocator_call(tracking->backing, req);
      if (result.status != BR_STATUS_OK) {
        return result;
      }
      if (tracking->clear_on_reset) {
        br__tracking_index_clear(tracking);
        tracking->entry_count = 0u;
        tracking->stats.current_memory_allocated = 0u;
        tracking->stats.live_allocation_count = 0u;
      }
      return result;

    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
      if (req->ptr != NULL) {
        if (!br__tracking_index_find(tracking, req->ptr, &entry_index)) {
          br__tracking_note_bad_free(tracking, req->ptr, req->old_size);
          return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
        }

        if (req->size == 0u) {
          result = br_allocator_call(tracking->backing, req);
          if (result.status == BR_STATUS_OK) {
            (void)br__tracking_index_remove(tracking, req->ptr, NULL);
            br__tracking_remove_entry(tracking, entry_index);
          }
          return result;
        }

        result = br_allocator_call(tracking->backing, req);
        if (result.status != BR_STATUS_OK) {
          return result;
        }

        /*
        Odin updates a map entry in place here. Bedrock keeps a dense public
        entry list for inspection plus a private pointer index for fast lookup.
        */
        br__tracking_resize_entry(tracking,
                                  entry_index,
                                  req->ptr,
                                  result.ptr,
                                  result.size,
                                  req->alignment,
                                  req->op,
                                  result.status);
        return result;
      }

      result = br_allocator_call(tracking->backing, req);
      if (result.status != BR_STATUS_OK) {
        return result;
      }
      if (!br__tracking_add_entry(
            tracking, result.ptr, result.size, req->alignment, req->op, result.status)) {
        if (result.ptr != NULL) {
          (void)br_allocator_free(tracking->backing, result.ptr, result.size);
        }
        return br__tracking_alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
      }
      return result;
  }

  return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

static br_alloc_result br__tracking_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_tracking_allocator *tracking = (br_tracking_allocator *)ctx;
  br_alloc_result result;

  if (req == NULL) {
    return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (tracking == NULL) {
    return br__tracking_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  br_mutex_lock(&tracking->mutex);
  result = br__tracking_allocator_fn_unlocked(tracking, req);
  br_mutex_unlock(&tracking->mutex);
  return result;
}

void br_tracking_allocator_init(br_tracking_allocator *tracking,
                                br_allocator backing,
                                br_allocator internals) {
  if (tracking == NULL) {
    return;
  }

  memset(tracking, 0, sizeof(*tracking));
  tracking->backing = backing;
  tracking->internals = internals.fn != NULL ? internals : br_allocator_heap();
  tracking->mutex = (br_mutex)BR_MUTEX_INIT;
}

void br_tracking_allocator_destroy(br_tracking_allocator *tracking) {
  br_allocator allocator;

  if (tracking == NULL) {
    return;
  }

  br_mutex_lock(&tracking->mutex);
  allocator = br__tracking_internals_allocator(tracking);
  if (tracking->entries != NULL) {
    (void)br_allocator_free(
      allocator, tracking->entries, tracking->entry_cap * sizeof(*tracking->entries));
  }
  if (tracking->bad_frees != NULL) {
    (void)br_allocator_free(
      allocator, tracking->bad_frees, tracking->bad_free_cap * sizeof(*tracking->bad_frees));
  }
  if (tracking->internal != NULL) {
    if (tracking->internal->slots != NULL) {
      (void)br_allocator_free(allocator,
                              tracking->internal->slots,
                              tracking->internal->slot_cap * sizeof(*tracking->internal->slots));
    }
    (void)br_allocator_free(allocator, tracking->internal, sizeof(*tracking->internal));
  }
  br_mutex_unlock(&tracking->mutex);

  memset(tracking, 0, sizeof(*tracking));
}

void br_tracking_allocator_clear(br_tracking_allocator *tracking) {
  if (tracking == NULL) {
    return;
  }

  br_mutex_lock(&tracking->mutex);
  br__tracking_clear_unlocked(tracking);
  br_mutex_unlock(&tracking->mutex);
}

void br_tracking_allocator_reset(br_tracking_allocator *tracking) {
  if (tracking == NULL) {
    return;
  }

  br_mutex_lock(&tracking->mutex);
  br__tracking_reset_unlocked(tracking);
  br_mutex_unlock(&tracking->mutex);
}

br_allocator br_tracking_allocator_allocator(br_tracking_allocator *tracking) {
  br_allocator allocator;

  allocator.fn = br__tracking_allocator_fn;
  allocator.ctx = tracking;
  return allocator;
}

/* ==== src/mem/virtual/arena.c ==== */

static br_virtual_arena_temp_result br__virtual_arena_temp_result(br_virtual_arena *arena,
                                                                  br_virtual_arena_block *block,
                                                                  usize used,
                                                                  br_status status) {
  br_virtual_arena_temp_result result;

  result.value.arena = arena;
  result.value.block = block;
  result.value.used = used;
  result.status = status;
  return result;
}

static br_alloc_result br__virtual_arena_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static void br__virtual_arena_reset_unlocked(br_virtual_arena *arena);

static bool br__safe_add_size(usize a, usize b, usize *result) {
  if (a > SIZE_MAX - b) {
    return false;
  }

  *result = a + b;
  return true;
}

static br_status br__align_up_size(usize value, usize alignment, usize *result) {
  usize mask;

  if (!br_is_power_of_two_size(alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  mask = alignment - 1u;
  if (value > SIZE_MAX - mask) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  *result = (value + mask) & ~mask;
  return BR_STATUS_OK;
}

static br_status br__vm_arena_normalize_alignment(usize alignment, usize minimum, usize *result) {
  if (alignment == 0u) {
    alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (alignment < minimum) {
    alignment = minimum;
  }
  if (!br_is_power_of_two_size(alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  *result = alignment;
  return BR_STATUS_OK;
}

static br_status br__align_to_pages(usize value, usize *result) {
  usize page_size = br_vm_page_size();

  if (page_size == 0u) {
    return BR_STATUS_NOT_SUPPORTED;
  }

  return br__align_up_size(value, page_size, result);
}

static br__vm_platform_memory_block *
br__virtual_platform_from_block(br_virtual_arena_block *block) {
  return (br__vm_platform_memory_block *)(void *)block;
}

static br_status br__virtual_block_create(usize committed,
                                          usize reserved,
                                          usize alignment,
                                          br_virtual_arena_flags flags,
                                          br_virtual_arena_block **out_block) {
  br__vm_platform_memory_block *platform_block;
  br_status status;
  usize base_offset;
  usize page_size;
  usize required_alignment;
  usize total_commit;
  usize total_size;
  usize payload_limit;
  usize guard_end;
  bool overflow_protection;

  if (out_block == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (reserved == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  status = br__vm_arena_normalize_alignment(
    alignment, (usize) _Alignof(br__vm_platform_memory_block), &alignment);
  if (status != BR_STATUS_OK) {
    return status;
  }

  page_size = br_vm_page_size();
  if (page_size == 0u) {
    return BR_STATUS_NOT_SUPPORTED;
  }

  status = br__align_to_pages(committed, &committed);
  if (status != BR_STATUS_OK) {
    return status;
  }
  status = br__align_to_pages(reserved, &reserved);
  if (status != BR_STATUS_OK) {
    return status;
  }
  if (committed > reserved) {
    committed = reserved;
  }

  overflow_protection = (flags & BR_VIRTUAL_ARENA_FLAG_OVERFLOW_PROTECTION) != 0u;
  required_alignment = overflow_protection ? br_max_size(alignment, page_size) : alignment;

  status =
    br__align_up_size(sizeof(br__vm_platform_memory_block), required_alignment, &base_offset);
  if (status != BR_STATUS_OK) {
    return status;
  }
  if (!br__safe_add_size(base_offset, reserved, &total_size)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }
  if (overflow_protection && !br__safe_add_size(total_size, page_size, &total_size)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }
  if (!br__safe_add_size(base_offset, committed, &total_commit)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  platform_block = br__vm_platform_memory_alloc(total_commit, total_size, &status);
  if (platform_block == NULL) {
    return status;
  }

  platform_block->block.base = (u8 *)(void *)platform_block + base_offset;
  platform_block->block.committed = committed;
  platform_block->block.reserved = reserved;

  if (overflow_protection) {
    if (!br__safe_add_size(base_offset, reserved, &payload_limit) ||
        !br__safe_add_size(payload_limit, page_size, &guard_end) ||
        guard_end > platform_block->reserved_total) {
      br__vm_platform_memory_free(platform_block);
      return BR_STATUS_INVALID_STATE;
    }

    /*
    Bedrock keeps overflow protection as a trailing guard page past the usable
    payload. This stays close to Odin's intent while avoiding exposing the guard
    page itself as part of `block.reserved`.

    The guard page must be committed before it is protected: Windows'
    VirtualProtect only operates on committed pages, while POSIX mprotect also
    accepts reserved-only mappings. The page is never touched afterwards, so
    committing it costs address space only on overcommitting platforms.
    */
    if (br_vm_commit((u8 *)(void *)platform_block + payload_limit, page_size) != BR_STATUS_OK) {
      br__vm_platform_memory_free(platform_block);
      return BR_STATUS_OUT_OF_MEMORY;
    }
    if (!br_vm_protect(
          (u8 *)(void *)platform_block + payload_limit, page_size, BR_VM_PROTECT_NONE)) {
      br__vm_platform_memory_free(platform_block);
      return BR_STATUS_NOT_SUPPORTED;
    }
  }

  *out_block = &platform_block->block;
  return BR_STATUS_OK;
}

static void br__virtual_block_destroy(br_virtual_arena_block *block) {
  if (block == NULL) {
    return;
  }

  br__vm_platform_memory_free(br__virtual_platform_from_block(block));
}

static br_status br__virtual_block_commit_if_needed(br_virtual_arena_block *block,
                                                    usize size,
                                                    usize default_commit_size) {
  br__vm_platform_memory_block *platform_block;
  br_status status;
  usize base_offset;
  usize extra_size;
  usize total_commit;
  usize payload_limit;

  if (block == NULL) {
    return BR_STATUS_OUT_OF_MEMORY;
  }
  if (block->committed - block->used >= size) {
    return BR_STATUS_OK;
  }

  platform_block = br__virtual_platform_from_block(block);
  base_offset = (usize)(block->base - (u8 *)(void *)platform_block);

  extra_size = br_max_size(size, block->committed >> 1u);
  if (!br__safe_add_size(base_offset, block->used, &total_commit) ||
      !br__safe_add_size(total_commit, extra_size, &total_commit)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  status = br__align_to_pages(total_commit, &total_commit);
  if (status != BR_STATUS_OK) {
    return status;
  }

  /*
  Odin compares the platform total commit directly against the arena-level
  default commit size. Bedrock keeps that shape for compatibility even though
  the value is nominally expressed in payload bytes rather than header-inclusive
  bytes.
  */
  total_commit = br_max_size(total_commit, default_commit_size);
  if (!br__safe_add_size(base_offset, block->reserved, &payload_limit)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }
  total_commit = br_min_size(total_commit, payload_limit);

  if (total_commit <= platform_block->committed_total) {
    return BR_STATUS_OK;
  }

  status = br__vm_platform_memory_commit(platform_block, total_commit);
  if (status != BR_STATUS_OK) {
    return status;
  }

  block->committed = total_commit - base_offset;
  return BR_STATUS_OK;
}

static br_alloc_result br__virtual_block_alloc(br_virtual_arena_block *block,
                                               usize size,
                                               usize alignment,
                                               usize default_commit_size,
                                               bool zeroed) {
  br_status status;
  usize alignment_offset;
  usize aligned;
  usize total_size;
  uptr current;
  u8 *ptr;

  if (block == NULL) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  status = br__vm_arena_normalize_alignment(alignment, 1u, &alignment);
  if (status != BR_STATUS_OK) {
    return br__virtual_arena_result(NULL, 0u, status);
  }
  if (size == 0u) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_OK);
  }

  current = (uptr)(void *)(block->base + block->used);
  status = br__align_up_size((usize)current, alignment, &aligned);
  if (status != BR_STATUS_OK) {
    return br__virtual_arena_result(NULL, 0u, status);
  }
  alignment_offset = aligned - (usize)current;

  if (!br__safe_add_size(size, alignment_offset, &total_size) ||
      total_size > (block->reserved - block->used)) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  status = br__virtual_block_commit_if_needed(block, total_size, default_commit_size);
  if (status != BR_STATUS_OK) {
    return br__virtual_arena_result(NULL, 0u, status);
  }

  ptr = block->base + block->used + alignment_offset;
  block->used += total_size;
  if (zeroed) {
    memset(ptr, 0, size);
  }

  return br__virtual_arena_result(ptr, size, BR_STATUS_OK);
}

static void br__virtual_arena_free_last_block(br_virtual_arena *arena) {
  br_virtual_arena_block *free_block;

  if (arena == NULL || arena->curr_block == NULL) {
    return;
  }

  free_block = arena->curr_block;
  arena->total_used -= free_block->used;
  arena->total_reserved -= free_block->reserved;
  arena->curr_block = free_block->prev;
  br__virtual_block_destroy(free_block);
}

static bool br__virtual_arena_has_block(const br_virtual_arena *arena,
                                        const br_virtual_arena_block *target) {
  const br_virtual_arena_block *block;

  if (arena == NULL || target == NULL) {
    return false;
  }

  for (block = arena->curr_block; block != NULL; block = block->prev) {
    if (block == target) {
      return true;
    }
  }

  return false;
}

static br_alloc_result br__virtual_arena_alloc_internal(br_virtual_arena *arena,
                                                        usize size,
                                                        usize alignment,
                                                        bool zeroed) {
  br_alloc_result result;
  br_status status;
  usize prev_used;
  usize default_commit_size;
  usize minimum_block_size;
  usize needed;
  usize block_size;
  br_virtual_arena_block *new_block;

  if (arena == NULL) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (size == 0u) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_OK);
  }

  switch (arena->kind) {
    case BR_VIRTUAL_ARENA_KIND_GROWING:
      prev_used = arena->curr_block != NULL ? arena->curr_block->used : 0u;
      result = br__virtual_block_alloc(
        arena->curr_block, size, alignment, arena->default_commit_size, zeroed);
      if (result.status == BR_STATUS_OUT_OF_MEMORY) {
        minimum_block_size = arena->minimum_block_size;
        if (minimum_block_size == 0u) {
          minimum_block_size = BR_VIRTUAL_ARENA_DEFAULT_GROWING_MINIMUM_BLOCK_SIZE;
          status = br__align_to_pages(minimum_block_size, &minimum_block_size);
          if (status != BR_STATUS_OK) {
            return br__virtual_arena_result(NULL, 0u, status);
          }
          arena->minimum_block_size = minimum_block_size;
        }

        default_commit_size = arena->default_commit_size;
        if (default_commit_size == 0u) {
          default_commit_size =
            br_min_size(BR_VIRTUAL_ARENA_DEFAULT_GROWING_COMMIT_SIZE, minimum_block_size);
          status = br__align_to_pages(default_commit_size, &default_commit_size);
          if (status != BR_STATUS_OK) {
            return br__virtual_arena_result(NULL, 0u, status);
          }
          arena->default_commit_size = default_commit_size;
        }

        default_commit_size = br_min_size(arena->default_commit_size, arena->minimum_block_size);
        minimum_block_size = br_max_size(arena->default_commit_size, arena->minimum_block_size);
        arena->default_commit_size = default_commit_size;
        arena->minimum_block_size = minimum_block_size;

        status = br__vm_arena_normalize_alignment(alignment, 1u, &alignment);
        if (status != BR_STATUS_OK) {
          return br__virtual_arena_result(NULL, 0u, status);
        }
        status = br__align_up_size(size, alignment, &needed);
        if (status != BR_STATUS_OK) {
          return br__virtual_arena_result(NULL, 0u, status);
        }

        needed = br_max_size(needed, default_commit_size);
        block_size = br_max_size(needed, minimum_block_size);

        status = br__virtual_block_create(needed, block_size, alignment, arena->flags, &new_block);
        if (status != BR_STATUS_OK) {
          return br__virtual_arena_result(NULL, 0u, status);
        }

        new_block->prev = arena->curr_block;
        arena->curr_block = new_block;
        arena->total_reserved += new_block->reserved;
        prev_used = 0u;
        result = br__virtual_block_alloc(
          arena->curr_block, size, alignment, arena->default_commit_size, zeroed);
      }

      if (result.status == BR_STATUS_OK && arena->curr_block != NULL) {
        arena->total_used += arena->curr_block->used - prev_used;
      }
      return result;

    case BR_VIRTUAL_ARENA_KIND_STATIC:
      result = br__virtual_block_alloc(
        arena->curr_block, size, alignment, arena->default_commit_size, zeroed);
      if (result.status == BR_STATUS_OK && arena->curr_block != NULL) {
        arena->total_used = arena->curr_block->used;
      }
      return result;

    case BR_VIRTUAL_ARENA_KIND_NONE:
      return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }

  return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_STATE);
}

static br_alloc_result br__virtual_arena_allocator_fn_unlocked(br_virtual_arena *arena,
                                                               const br_alloc_request *req) {
  br_alloc_result result;

  if (req == NULL) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__virtual_arena_alloc_internal(arena, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__virtual_arena_alloc_internal(arena, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
      if (req->ptr == NULL) {
        return br__virtual_arena_alloc_internal(
          arena, req->size, req->alignment, req->op == BR_ALLOC_OP_RESIZE);
      }
      if (req->old_size == 0u) {
        return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
      }
      if (req->size == req->old_size) {
        return br__virtual_arena_result(req->ptr, req->size, BR_STATUS_OK);
      }
      if (req->size < req->old_size) {
        return br__virtual_arena_result(req->ptr, req->size, BR_STATUS_OK);
      }

      /*
      Bedrock mirrors Odin's current-block tail growth where possible, but we
      still fall back to allocate-and-copy because the C allocator ABI does not
      track ownership metadata for arbitrary old pointers.
      */
      if (arena != NULL && arena->curr_block != NULL) {
        br_virtual_arena_block *block = arena->curr_block;
        u8 *ptr = (u8 *)req->ptr;
        usize start;
        usize old_end;
        usize extra;
        br_status status;

        if (ptr >= block->base && ptr <= block->base + block->used) {
          start = (usize)(ptr - block->base);
          if (req->old_size <= block->reserved - start) {
            old_end = start + req->old_size;
            if (old_end == block->used) {
              extra = req->size - req->old_size;
              status = br__virtual_block_commit_if_needed(block, extra, arena->default_commit_size);
              if (status == BR_STATUS_OK && extra <= block->reserved - block->used) {
                block->used += extra;
                arena->total_used += extra;
                if (req->op == BR_ALLOC_OP_RESIZE) {
                  memset(ptr + req->old_size, 0, extra);
                }
                return br__virtual_arena_result(req->ptr, req->size, BR_STATUS_OK);
              }
            }
          }
        }
      }

      result = br__virtual_arena_alloc_internal(
        arena, req->size, req->alignment, req->op == BR_ALLOC_OP_RESIZE);
      if (result.status != BR_STATUS_OK) {
        return result;
      }

      memcpy(result.ptr, req->ptr, br_min_size(req->old_size, req->size));
      return result;
    case BR_ALLOC_OP_FREE:
      return br__virtual_arena_result(NULL, 0u, BR_STATUS_OK);
    case BR_ALLOC_OP_RESET:
      br__virtual_arena_reset_unlocked(arena);
      return br__virtual_arena_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

static br_alloc_result br__virtual_arena_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_virtual_arena *arena = (br_virtual_arena *)ctx;
  br_alloc_result result;

  if (req == NULL) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (arena == NULL) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  br_mutex_lock(&arena->mutex);
  result = br__virtual_arena_allocator_fn_unlocked(arena, req);
  br_mutex_unlock(&arena->mutex);
  return result;
}

void br_virtual_arena_init(br_virtual_arena *arena) {
  if (arena == NULL) {
    return;
  }

  memset(arena, 0, sizeof(*arena));
  arena->mutex = (br_mutex)BR_MUTEX_INIT;
}

br_status br_virtual_arena_init_growing(br_virtual_arena *arena, usize reserved) {
  br_status status;

  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (arena->kind != BR_VIRTUAL_ARENA_KIND_NONE || arena->curr_block != NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  if (reserved == 0u) {
    reserved = arena->minimum_block_size != 0u
                 ? arena->minimum_block_size
                 : BR_VIRTUAL_ARENA_DEFAULT_GROWING_MINIMUM_BLOCK_SIZE;
  }

  if (arena->minimum_block_size != 0u) {
    status = br__align_to_pages(arena->minimum_block_size, &arena->minimum_block_size);
    if (status != BR_STATUS_OK) {
      return status;
    }
  }
  if (arena->default_commit_size != 0u) {
    status = br__align_to_pages(arena->default_commit_size, &arena->default_commit_size);
    if (status != BR_STATUS_OK) {
      return status;
    }
  }

  status =
    br__virtual_block_create(0u, reserved, BR_DEFAULT_ALIGNMENT, arena->flags, &arena->curr_block);
  if (status != BR_STATUS_OK) {
    return status;
  }

  arena->kind = BR_VIRTUAL_ARENA_KIND_GROWING;
  arena->total_used = 0u;
  arena->total_reserved = arena->curr_block->reserved;
  if (arena->minimum_block_size == 0u) {
    arena->minimum_block_size = arena->curr_block->reserved;
  }

  return BR_STATUS_OK;
}

br_status br_virtual_arena_init_static(br_virtual_arena *arena, usize reserved, usize commit_size) {
  br_status status;

  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (arena->kind != BR_VIRTUAL_ARENA_KIND_NONE || arena->curr_block != NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  if (reserved == 0u) {
    reserved = arena->minimum_block_size != 0u ? arena->minimum_block_size
                                               : BR_VIRTUAL_ARENA_DEFAULT_STATIC_RESERVE_SIZE;
  }
  if (commit_size == 0u) {
    commit_size = BR_VIRTUAL_ARENA_DEFAULT_STATIC_COMMIT_SIZE;
  }

  if (arena->default_commit_size != 0u) {
    status = br__align_to_pages(arena->default_commit_size, &arena->default_commit_size);
    if (status != BR_STATUS_OK) {
      return status;
    }
  }
  if (arena->minimum_block_size != 0u) {
    status = br__align_to_pages(arena->minimum_block_size, &arena->minimum_block_size);
    if (status != BR_STATUS_OK) {
      return status;
    }
  }

  status = br__virtual_block_create(
    commit_size, reserved, BR_DEFAULT_ALIGNMENT, arena->flags, &arena->curr_block);
  if (status != BR_STATUS_OK) {
    return status;
  }

  arena->kind = BR_VIRTUAL_ARENA_KIND_STATIC;
  arena->total_used = 0u;
  arena->total_reserved = arena->curr_block->reserved;
  if (arena->minimum_block_size == 0u) {
    arena->minimum_block_size = arena->curr_block->reserved;
  }

  return BR_STATUS_OK;
}

static void br__virtual_arena_reset_unlocked(br_virtual_arena *arena) {
  if (arena == NULL) {
    return;
  }

  switch (arena->kind) {
    case BR_VIRTUAL_ARENA_KIND_GROWING:
      while (arena->curr_block != NULL && arena->curr_block->prev != NULL) {
        br__virtual_arena_free_last_block(arena);
      }
      if (arena->curr_block != NULL && arena->curr_block->used != 0u) {
        memset(arena->curr_block->base, 0, arena->curr_block->used);
        arena->curr_block->used = 0u;
      }
      arena->total_used = 0u;
      if (arena->curr_block != NULL) {
        arena->total_reserved = arena->curr_block->reserved;
      }
      break;

    case BR_VIRTUAL_ARENA_KIND_STATIC:
      if (arena->curr_block != NULL && arena->curr_block->used != 0u) {
        memset(arena->curr_block->base, 0, arena->curr_block->used);
        arena->curr_block->used = 0u;
      }
      arena->total_used = 0u;
      break;

    case BR_VIRTUAL_ARENA_KIND_NONE:
      break;
  }
}

void br_virtual_arena_reset(br_virtual_arena *arena) {
  if (arena == NULL) {
    return;
  }

  br_mutex_lock(&arena->mutex);
  br__virtual_arena_reset_unlocked(arena);
  br_mutex_unlock(&arena->mutex);
}

void br_virtual_arena_destroy(br_virtual_arena *arena) {
  br_virtual_arena_block *block;
  br_virtual_arena_block *prev;

  if (arena == NULL) {
    return;
  }

  br_mutex_lock(&arena->mutex);
  block = arena->curr_block;
  while (block != NULL) {
    prev = block->prev;
    br__virtual_block_destroy(block);
    block = prev;
  }
  br_mutex_unlock(&arena->mutex);

  memset(arena, 0, sizeof(*arena));
}

br_virtual_arena_mark br_virtual_arena_mark_save(br_virtual_arena *arena) {
  br_virtual_arena_mark mark;

  mark.block = NULL;
  mark.used = 0u;
  if (arena == NULL) {
    return mark;
  }

  br_mutex_lock(&arena->mutex);
  if (arena != NULL && arena->curr_block != NULL) {
    mark.block = arena->curr_block;
    mark.used = arena->curr_block->used;
  }
  br_mutex_unlock(&arena->mutex);
  return mark;
}

static br_status br__virtual_arena_rewind_unlocked(br_virtual_arena *arena,
                                                   br_virtual_arena_mark mark) {
  br_virtual_arena_block *block;
  usize old_used;

  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  switch (arena->kind) {
    case BR_VIRTUAL_ARENA_KIND_GROWING:
      if (mark.block == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
      }

      for (block = arena->curr_block; block != NULL; block = block->prev) {
        if (block == mark.block) {
          break;
        }
      }
      if (block == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
      }

      while (arena->curr_block != mark.block) {
        br__virtual_arena_free_last_block(arena);
      }

      if (arena->curr_block == NULL || mark.used > arena->curr_block->used) {
        return BR_STATUS_INVALID_ARGUMENT;
      }

      old_used = arena->curr_block->used;
      if (old_used > mark.used) {
        memset(arena->curr_block->base + mark.used, 0, old_used - mark.used);
        arena->curr_block->used = mark.used;
        arena->total_used -= old_used - mark.used;
      }
      return BR_STATUS_OK;

    case BR_VIRTUAL_ARENA_KIND_STATIC:
      if (arena->curr_block == NULL || mark.block != arena->curr_block ||
          mark.used > arena->curr_block->used) {
        return BR_STATUS_INVALID_ARGUMENT;
      }

      old_used = arena->curr_block->used;
      if (old_used > mark.used) {
        memset(arena->curr_block->base + mark.used, 0, old_used - mark.used);
        arena->curr_block->used = mark.used;
        arena->total_used -= old_used - mark.used;
      }
      return BR_STATUS_OK;

    case BR_VIRTUAL_ARENA_KIND_NONE:
      return mark.block == NULL ? BR_STATUS_OK : BR_STATUS_INVALID_STATE;
  }

  return BR_STATUS_INVALID_STATE;
}

br_status br_virtual_arena_rewind(br_virtual_arena *arena, br_virtual_arena_mark mark) {
  br_status status;

  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  br_mutex_lock(&arena->mutex);
  status = br__virtual_arena_rewind_unlocked(arena, mark);
  br_mutex_unlock(&arena->mutex);
  return status;
}

static br_virtual_arena_temp_result br__virtual_arena_temp_begin_unlocked(br_virtual_arena *arena) {
  if (arena == NULL) {
    return br__virtual_arena_temp_result(NULL, NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (arena->curr_block == NULL || arena->kind == BR_VIRTUAL_ARENA_KIND_NONE) {
    return br__virtual_arena_temp_result(arena, NULL, 0u, BR_STATUS_INVALID_STATE);
  }

  arena->temp_count += 1u;
  return br__virtual_arena_temp_result(
    arena, arena->curr_block, arena->curr_block->used, BR_STATUS_OK);
}

br_virtual_arena_temp_result br_virtual_arena_temp_begin(br_virtual_arena *arena) {
  br_virtual_arena_temp_result result;

  if (arena == NULL) {
    return br__virtual_arena_temp_result(NULL, NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  br_mutex_lock(&arena->mutex);
  result = br__virtual_arena_temp_begin_unlocked(arena);
  br_mutex_unlock(&arena->mutex);
  return result;
}

static br_status br__virtual_arena_temp_end_unlocked(br_virtual_arena_temp temp) {
  br_virtual_arena *arena;
  usize old_used;

  if (temp.arena == NULL || temp.block == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  arena = temp.arena;
  if (arena->temp_count == 0u) {
    return BR_STATUS_INVALID_STATE;
  }
  if (!br__virtual_arena_has_block(arena, temp.block)) {
    return BR_STATUS_INVALID_STATE;
  }

  while (arena->curr_block != temp.block) {
    br__virtual_arena_free_last_block(arena);
  }

  if (arena->curr_block == NULL || temp.used > arena->curr_block->used) {
    return BR_STATUS_INVALID_STATE;
  }

  old_used = arena->curr_block->used;
  if (old_used > temp.used) {
    memset(arena->curr_block->base + temp.used, 0, old_used - temp.used);
    arena->curr_block->used = temp.used;
    arena->total_used -= old_used - temp.used;
  }

  arena->temp_count -= 1u;
  return BR_STATUS_OK;
}

br_status br_virtual_arena_temp_end(br_virtual_arena_temp temp) {
  br_status status;

  if (temp.arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  br_mutex_lock(&temp.arena->mutex);
  status = br__virtual_arena_temp_end_unlocked(temp);
  br_mutex_unlock(&temp.arena->mutex);
  return status;
}

static br_status br__virtual_arena_temp_ignore_unlocked(br_virtual_arena_temp temp) {
  if (temp.arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (temp.arena->temp_count == 0u) {
    return BR_STATUS_INVALID_STATE;
  }

  temp.arena->temp_count -= 1u;
  return BR_STATUS_OK;
}

br_status br_virtual_arena_temp_ignore(br_virtual_arena_temp temp) {
  br_status status;

  if (temp.arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  br_mutex_lock(&temp.arena->mutex);
  status = br__virtual_arena_temp_ignore_unlocked(temp);
  br_mutex_unlock(&temp.arena->mutex);
  return status;
}

static br_status br__virtual_arena_check_temp_unlocked(const br_virtual_arena *arena) {
  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  return arena->temp_count == 0u ? BR_STATUS_OK : BR_STATUS_INVALID_STATE;
}

br_status br_virtual_arena_check_temp(br_virtual_arena *arena) {
  br_status status;

  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  br_mutex_lock(&arena->mutex);
  status = br__virtual_arena_check_temp_unlocked(arena);
  br_mutex_unlock(&arena->mutex);
  return status;
}

br_alloc_result br_virtual_arena_alloc(br_virtual_arena *arena, usize size, usize alignment) {
  br_alloc_result result;

  if (arena == NULL) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  br_mutex_lock(&arena->mutex);
  result = br__virtual_arena_alloc_internal(arena, size, alignment, true);
  br_mutex_unlock(&arena->mutex);
  return result;
}

br_alloc_result
br_virtual_arena_alloc_uninit(br_virtual_arena *arena, usize size, usize alignment) {
  br_alloc_result result;

  if (arena == NULL) {
    return br__virtual_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  br_mutex_lock(&arena->mutex);
  result = br__virtual_arena_alloc_internal(arena, size, alignment, false);
  br_mutex_unlock(&arena->mutex);
  return result;
}

br_allocator br_virtual_arena_allocator(br_virtual_arena *arena) {
  br_allocator allocator;

  allocator.fn = br__virtual_arena_allocator_fn;
  allocator.ctx = arena;
  return allocator;
}

/* ==== src/mem/virtual/arena_util.c ==== */

#include <string.h>

static br_virtual_arena_make_result
br__virtual_arena_make_result(void *data, usize len, br_status status) {
  br_virtual_arena_make_result result;

  result.data = data;
  result.len = len;
  result.status = status;
  return result;
}

static bool br__virtual_arena_mul_size(usize a, usize b, usize *result) {
  if (result == NULL) {
    return false;
  }
  if (a != 0u && b > SIZE_MAX / a) {
    return false;
  }

  *result = a * b;
  return true;
}

br_alloc_result br_virtual_arena_new_raw(br_virtual_arena *arena, usize size, usize alignment) {
  return br_virtual_arena_alloc(arena, size, alignment);
}

br_alloc_result
br_virtual_arena_clone_raw(br_virtual_arena *arena, const void *src, usize size, usize alignment) {
  br_alloc_result result;

  result = br_virtual_arena_alloc(arena, size, alignment);
  if (result.status != BR_STATUS_OK || result.ptr == NULL) {
    return result;
  }

  if (src != NULL && size != 0u) {
    memcpy(result.ptr, src, size);
  }
  return result;
}

br_virtual_arena_make_result
br_virtual_arena_make_raw(br_virtual_arena *arena, usize elem_size, usize len, usize alignment) {
  br_alloc_result result;
  usize size;

  if (!br__virtual_arena_mul_size(elem_size, len, &size)) {
    return br__virtual_arena_make_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  result = br_virtual_arena_alloc(arena, size, alignment);
  if (result.status != BR_STATUS_OK) {
    return br__virtual_arena_make_result(NULL, 0u, result.status);
  }
  if (result.ptr == NULL && elem_size != 0u) {
    return br__virtual_arena_make_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  return br__virtual_arena_make_result(result.ptr, len, BR_STATUS_OK);
}

/* ==== src/mem/virtual/common.c ==== */


br_vm_region_result br__vm_region_result(u8 *data, usize size, br_status status) {
  br_vm_region_result result;

  result.value.data = data;
  result.value.size = size;
  result.status = status;
  return result;
}

br_vm_mapped_file_result
br__vm_mapped_file_result(u8 *data, usize size, br_vm_map_file_error error) {
  br_vm_mapped_file_result result;

  result.value.data = data;
  result.value.size = size;
  result.error = error;
  return result;
}

usize br__vm_cached_page_size(void) {
  static usize cached_page_size = 0u;

  if (cached_page_size == 0u) {
    cached_page_size = br__vm_platform_page_size_query();
  }

  return cached_page_size;
}

usize br_vm_page_size(void) {
  return br__vm_cached_page_size();
}

br_vm_region_result br_vm_reserve_commit(usize size) {
  br_status status;
  br_vm_region_result result;

  result = br_vm_reserve(size);
  if (result.status != BR_STATUS_OK || result.value.data == NULL || size == 0u) {
    return result;
  }

  status = br_vm_commit(result.value.data, size);
  if (status != BR_STATUS_OK) {
    br_vm_release(result.value.data, result.value.size);
    return br__vm_region_result(NULL, 0u, status);
  }

  return result;
}

/* ==== src/mem/virtual/file.c ==== */

#if defined(BR__VM_TARGET_WINDOWS)

#include <windows.h>

static br_vm_map_file_error
br__vm_open_file_path(const char *path, br_vm_map_file_flags flags, br__vm_native_file *file_out) {
  DWORD desired_access = 0u;
  HANDLE file;

  if (file_out == NULL) {
    return BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT;
  }

  if ((flags & BR_VM_MAP_FILE_READ) != 0u) {
    desired_access |= GENERIC_READ;
  }
  if ((flags & BR_VM_MAP_FILE_WRITE) != 0u) {
    desired_access |= GENERIC_READ | GENERIC_WRITE;
  }

  file = CreateFileA(
    path, desired_access, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    return BR_VM_MAP_FILE_ERROR_OPEN_FAILURE;
  }

  *file_out = (br__vm_native_file)(iptr)file;
  return BR_VM_MAP_FILE_ERROR_NONE;
}

static br_vm_map_file_error br__vm_query_file_size(br__vm_native_file file, i64 *size_out) {
  LARGE_INTEGER file_size;
  HANDLE handle = (HANDLE)(iptr)file;

  if (size_out == NULL) {
    return BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT;
  }
  if (GetFileSizeEx(handle, &file_size) == 0) {
    return BR_VM_MAP_FILE_ERROR_STAT_FAILURE;
  }
  if (file_size.QuadPart < 0) {
    return BR_VM_MAP_FILE_ERROR_NEGATIVE_SIZE;
  }

  *size_out = (i64)file_size.QuadPart;
  return BR_VM_MAP_FILE_ERROR_NONE;
}

static void br__vm_close_file(br__vm_native_file file) {
  HANDLE handle = (HANDLE)(iptr)file;

  if (handle != NULL && handle != INVALID_HANDLE_VALUE) {
    (void)CloseHandle(handle);
  }
}

#elif defined(BR__VM_TARGET_LINUX) || defined(BR__VM_TARGET_POSIX)

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static br_vm_map_file_error
br__vm_open_file_path(const char *path, br_vm_map_file_flags flags, br__vm_native_file *file_out) {
  int open_flags = O_RDONLY;
  int fd;

  if (file_out == NULL) {
    return BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT;
  }
  if ((flags & BR_VM_MAP_FILE_WRITE) != 0u) {
    open_flags = O_RDWR;
  }

  fd = open(path, open_flags);
  if (fd < 0) {
    return BR_VM_MAP_FILE_ERROR_OPEN_FAILURE;
  }

  *file_out = (br__vm_native_file)fd;
  return BR_VM_MAP_FILE_ERROR_NONE;
}

static br_vm_map_file_error br__vm_query_file_size(br__vm_native_file file, i64 *size_out) {
  struct stat st;
  int fd = (int)file;

  if (size_out == NULL) {
    return BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT;
  }
  if (fstat(fd, &st) != 0) {
    return BR_VM_MAP_FILE_ERROR_STAT_FAILURE;
  }
  if (st.st_size < 0) {
    return BR_VM_MAP_FILE_ERROR_NEGATIVE_SIZE;
  }

  *size_out = (i64)st.st_size;
  return BR_VM_MAP_FILE_ERROR_NONE;
}

static void br__vm_close_file(br__vm_native_file file) {
  int fd = (int)file;

  if (fd >= 0) {
    (void)close(fd);
  }
}

#else

static br_vm_map_file_error
br__vm_open_file_path(const char *path, br_vm_map_file_flags flags, br__vm_native_file *file_out) {
  BR_UNUSED(path);
  BR_UNUSED(flags);
  BR_UNUSED(file_out);
  return BR_VM_MAP_FILE_ERROR_MAP_FAILURE;
}

static br_vm_map_file_error br__vm_query_file_size(br__vm_native_file file, i64 *size_out) {
  BR_UNUSED(file);
  BR_UNUSED(size_out);
  return BR_VM_MAP_FILE_ERROR_MAP_FAILURE;
}

static void br__vm_close_file(br__vm_native_file file) {
  BR_UNUSED(file);
}

#endif

br_vm_mapped_file_result br_vm_map_file(const char *path, br_vm_map_file_flags flags) {
  br__vm_native_file file = (br__vm_native_file)0;
  br_vm_map_file_error error;
  br_vm_mapped_file_result result;
  i64 size;

  if (path == NULL || (flags & (BR_VM_MAP_FILE_READ | BR_VM_MAP_FILE_WRITE)) == 0u) {
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT);
  }

  /*
  Bedrock still exposes only the path-based public API until an `os/file`
  layer exists, but this file now owns the high-level open/stat/map flow like
  Odin's `virtual/file.odin` instead of pushing it all into per-platform files.
  */
  error = br__vm_open_file_path(path, flags, &file);
  if (error != BR_VM_MAP_FILE_ERROR_NONE) {
    return br__vm_mapped_file_result(NULL, 0u, error);
  }

  error = br__vm_query_file_size(file, &size);
  if (error != BR_VM_MAP_FILE_ERROR_NONE) {
    br__vm_close_file(file);
    return br__vm_mapped_file_result(NULL, 0u, error);
  }
  if ((u64)size > (u64)SIZE_MAX) {
    br__vm_close_file(file);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_TOO_LARGE_SIZE);
  }
  if (size == 0) {
    br__vm_close_file(file);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_NONE);
  }

  result = br__vm_platform_map_open_file(file, (usize)size, flags);
  br__vm_close_file(file);
  return result;
}

void br_vm_unmap_file(br_vm_mapped_file mapping) {
  if (mapping.data == NULL || mapping.size == 0u) {
    return;
  }

  br__vm_platform_unmap_file(mapping);
}

/* ==== src/mem/virtual/virtual.c ==== */

br_vm_region_result br_vm_reserve(usize size) {
  if (size == 0u) {
    return br__vm_region_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__vm_platform_reserve(size);
}

br_status br_vm_commit(void *ptr, usize size) {
  if (size == 0u) {
    return BR_STATUS_OK;
  }
  if (ptr == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  return br__vm_platform_commit(ptr, size);
}

void br_vm_decommit(void *ptr, usize size) {
  if (ptr == NULL || size == 0u) {
    return;
  }

  br__vm_platform_decommit(ptr, size);
}

void br_vm_release(void *ptr, usize size) {
  if (ptr == NULL || size == 0u) {
    return;
  }

  br__vm_platform_release(ptr, size);
}

bool br_vm_protect(void *ptr, usize size, br_vm_protect_flags flags) {
  if (size == 0u) {
    return true;
  }
  if (ptr == NULL) {
    return false;
  }

  return br__vm_platform_protect(ptr, size, flags);
}

/* ==== src/mem/virtual/virtual_darwin.c ==== */

#if defined(BR__VM_TARGET_DARWIN)

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

static br_status br__vm_status_from_darwin_errno(int err) {
  switch (err) {
    case EINVAL:
      return BR_STATUS_INVALID_ARGUMENT;
    case ENOMEM:
      return BR_STATUS_OUT_OF_MEMORY;
    default:
      return BR_STATUS_INVALID_STATE;
  }
}

br_vm_region_result br__vm_platform_reserve(usize size) {
  void *ptr;

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

  ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    return br__vm_region_result(NULL, 0u, br__vm_status_from_darwin_errno(errno));
  }

  return br__vm_region_result((u8 *)ptr, size, BR_STATUS_OK);
}

void br__vm_platform_decommit(void *ptr, usize size) {
  enum { BR__VM_DARWIN_MADV_FREE = 5 };

  (void)mprotect(ptr, size, PROT_NONE);
  (void)posix_madvise(ptr, size, BR__VM_DARWIN_MADV_FREE);
}

#endif

/* ==== src/mem/virtual/virtual_freebsd.c ==== */

#if defined(BR__VM_TARGET_FREEBSD)

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

static br_status br__vm_status_from_freebsd_errno(int err) {
  switch (err) {
    case EINVAL:
      return BR_STATUS_INVALID_ARGUMENT;
    case ENOMEM:
      return BR_STATUS_OUT_OF_MEMORY;
    default:
      return BR_STATUS_INVALID_STATE;
  }
}

static int br__vm_freebsd_prot_max(int prot) {
  return prot << 16;
}

br_vm_region_result br__vm_platform_reserve(usize size) {
  void *ptr;

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

  ptr = mmap(NULL,
             size,
             br__vm_freebsd_prot_max(PROT_READ | PROT_WRITE | PROT_EXEC),
             MAP_PRIVATE | MAP_ANONYMOUS,
             -1,
             0);
  if (ptr == MAP_FAILED) {
    return br__vm_region_result(NULL, 0u, br__vm_status_from_freebsd_errno(errno));
  }

  return br__vm_region_result((u8 *)ptr, size, BR_STATUS_OK);
}

void br__vm_platform_decommit(void *ptr, usize size) {
  enum { BR__VM_FREEBSD_MADV_FREE = 5 };

  (void)mprotect(ptr, size, PROT_NONE);
  (void)posix_madvise(ptr, size, BR__VM_FREEBSD_MADV_FREE);
}

#endif

/* ==== src/mem/virtual/virtual_linux.c ==== */

#if defined(BR__VM_TARGET_LINUX)

#include <errno.h>
#include <sys/mman.h>

static br_status br__vm_status_from_linux_errno(int err) {
  switch (err) {
    case EINVAL:
      return BR_STATUS_INVALID_ARGUMENT;
    case ENOMEM:
      return BR_STATUS_OUT_OF_MEMORY;
    default:
      return BR_STATUS_INVALID_STATE;
  }
}

usize br__vm_platform_page_size_query(void) {
  return 4096u;
}

br_vm_region_result br__vm_platform_reserve(usize size) {
  void *ptr;

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

  ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    return br__vm_region_result(NULL, 0u, br__vm_status_from_linux_errno(errno));
  }

  return br__vm_region_result((u8 *)ptr, size, BR_STATUS_OK);
}

br_status br__vm_platform_commit(void *ptr, usize size) {
  return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0 ? BR_STATUS_OK
                                                          : br__vm_status_from_linux_errno(errno);
}

void br__vm_platform_decommit(void *ptr, usize size) {
  (void)mprotect(ptr, size, PROT_NONE);
#if defined(MADV_FREE)
  (void)madvise(ptr, size, MADV_FREE);
#elif defined(MADV_DONTNEED)
  /*
  Odin uses MADV_FREE on Linux. Bedrock falls back to DONTNEED when the libc
  headers do not expose MADV_FREE so the Linux backend still builds cleanly.
  */
  (void)madvise(ptr, size, MADV_DONTNEED);
#endif
}

void br__vm_platform_release(void *ptr, usize size) {
  (void)munmap(ptr, size);
}

bool br__vm_platform_protect(void *ptr, usize size, br_vm_protect_flags flags) {
  int protect = PROT_NONE;

  if ((flags & BR_VM_PROTECT_READ) != 0u) {
    protect |= PROT_READ;
  }
  if ((flags & BR_VM_PROTECT_WRITE) != 0u) {
    protect |= PROT_WRITE;
  }
  if ((flags & BR_VM_PROTECT_EXECUTE) != 0u) {
    protect |= PROT_EXEC;
  }

  return mprotect(ptr, size, protect) == 0;
}

br_vm_mapped_file_result
br__vm_platform_map_open_file(br__vm_native_file file, usize size, br_vm_map_file_flags flags) {
  int prot = 0;
  void *view;

  if ((flags & BR_VM_MAP_FILE_WRITE) != 0u) {
    prot |= PROT_WRITE;
  }
  if ((flags & BR_VM_MAP_FILE_READ) != 0u || (flags & BR_VM_MAP_FILE_WRITE) == 0u) {
    prot |= PROT_READ;
  }

  view = mmap(NULL, size, prot, MAP_SHARED, (int)file, 0);
  if (view == MAP_FAILED || view == NULL) {
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_MAP_FAILURE);
  }

  return br__vm_mapped_file_result((u8 *)view, size, BR_VM_MAP_FILE_ERROR_NONE);
}

void br__vm_platform_unmap_file(br_vm_mapped_file mapping) {
#if defined(MS_SYNC)
  (void)msync(mapping.data, mapping.size, MS_SYNC);
#endif
  (void)munmap(mapping.data, mapping.size);
}

#endif

/* ==== src/mem/virtual/virtual_netbsd.c ==== */

#if defined(BR__VM_TARGET_NETBSD)

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

static br_status br__vm_status_from_netbsd_errno(int err) {
  switch (err) {
    case EINVAL:
      return BR_STATUS_INVALID_ARGUMENT;
    case ENOMEM:
      return BR_STATUS_OUT_OF_MEMORY;
    default:
      return BR_STATUS_INVALID_STATE;
  }
}

static int br__vm_netbsd_prot_mprotect(int prot) {
  return prot << 3;
}

br_vm_region_result br__vm_platform_reserve(usize size) {
  void *ptr;

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

  ptr = mmap(NULL,
             size,
             br__vm_netbsd_prot_mprotect(PROT_READ | PROT_WRITE | PROT_EXEC),
             MAP_PRIVATE | MAP_ANONYMOUS,
             -1,
             0);
  if (ptr == MAP_FAILED) {
    return br__vm_region_result(NULL, 0u, br__vm_status_from_netbsd_errno(errno));
  }

  return br__vm_region_result((u8 *)ptr, size, BR_STATUS_OK);
}

void br__vm_platform_decommit(void *ptr, usize size) {
  enum { BR__VM_NETBSD_MADV_FREE = 6 };

  (void)mprotect(ptr, size, PROT_NONE);
  (void)posix_madvise(ptr, size, BR__VM_NETBSD_MADV_FREE);
}

#endif

/* ==== src/mem/virtual/virtual_openbsd.c ==== */

#if defined(BR__VM_TARGET_OPENBSD)

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

static br_status br__vm_status_from_openbsd_errno(int err) {
  switch (err) {
    case EINVAL:
      return BR_STATUS_INVALID_ARGUMENT;
    case ENOMEM:
      return BR_STATUS_OUT_OF_MEMORY;
    default:
      return BR_STATUS_INVALID_STATE;
  }
}

br_vm_region_result br__vm_platform_reserve(usize size) {
  void *ptr;

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

  ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    return br__vm_region_result(NULL, 0u, br__vm_status_from_openbsd_errno(errno));
  }

  return br__vm_region_result((u8 *)ptr, size, BR_STATUS_OK);
}

void br__vm_platform_decommit(void *ptr, usize size) {
  enum { BR__VM_OPENBSD_MADV_FREE = 6 };

  (void)mprotect(ptr, size, PROT_NONE);
  (void)posix_madvise(ptr, size, BR__VM_OPENBSD_MADV_FREE);
}

#endif

/* ==== src/mem/virtual/virtual_other.c ==== */

#if defined(BR__VM_TARGET_OTHER)

usize br__vm_platform_page_size_query(void) {
  return 0u;
}

br_vm_region_result br__vm_platform_reserve(usize size) {
  BR_UNUSED(size);
  return br__vm_region_result(NULL, 0u, BR_STATUS_NOT_SUPPORTED);
}

br_status br__vm_platform_commit(void *ptr, usize size) {
  BR_UNUSED(ptr);
  BR_UNUSED(size);
  return BR_STATUS_NOT_SUPPORTED;
}

void br__vm_platform_decommit(void *ptr, usize size) {
  BR_UNUSED(ptr);
  BR_UNUSED(size);
}

void br__vm_platform_release(void *ptr, usize size) {
  BR_UNUSED(ptr);
  BR_UNUSED(size);
}

bool br__vm_platform_protect(void *ptr, usize size, br_vm_protect_flags flags) {
  BR_UNUSED(ptr);
  BR_UNUSED(size);
  BR_UNUSED(flags);
  return false;
}

br_vm_mapped_file_result
br__vm_platform_map_open_file(br__vm_native_file file, usize size, br_vm_map_file_flags flags) {
  BR_UNUSED(file);
  BR_UNUSED(size);
  BR_UNUSED(flags);
  return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_MAP_FAILURE);
}

void br__vm_platform_unmap_file(br_vm_mapped_file mapping) {
  BR_UNUSED(mapping);
}

#endif

/* ==== src/mem/virtual/virtual_platform.c ==== */

br__vm_platform_memory_block *
br__vm_platform_memory_alloc(usize to_commit, usize to_reserve, br_status *status) {
  br_vm_region_result region;
  br_status commit_status;
  br__vm_platform_memory_block *block;

  if (status == NULL) {
    return NULL;
  }

  to_reserve = br_max_size(to_reserve, sizeof(br__vm_platform_memory_block));
  to_commit = br_max_size(to_commit, sizeof(br__vm_platform_memory_block));
  to_commit = br_min_size(to_commit, to_reserve);

  region = br_vm_reserve(to_reserve);
  if (region.status != BR_STATUS_OK) {
    *status = region.status;
    return NULL;
  }

  commit_status = br_vm_commit(region.value.data, to_commit);
  if (commit_status != BR_STATUS_OK) {
    br_vm_release(region.value.data, region.value.size);
    *status = commit_status;
    return NULL;
  }

  block = (br__vm_platform_memory_block *)(void *)region.value.data;
  memset(block, 0, sizeof(*block));
  block->committed_total = to_commit;
  block->reserved_total = to_reserve;
  *status = BR_STATUS_OK;
  return block;
}

void br__vm_platform_memory_free(br__vm_platform_memory_block *block) {
  if (block == NULL) {
    return;
  }

  br_vm_release(block, block->reserved_total);
}

br_status br__vm_platform_memory_commit(br__vm_platform_memory_block *block, usize to_commit) {
  br_status status;

  if (block == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (to_commit < block->committed_total) {
    return BR_STATUS_OK;
  }
  if (to_commit > block->reserved_total) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  status = br_vm_commit(block, to_commit);
  if (status != BR_STATUS_OK) {
    return status;
  }

  block->committed_total = to_commit;
  return BR_STATUS_OK;
}

/* ==== src/mem/virtual/virtual_posix.c ==== */

#if defined(BR__VM_TARGET_POSIX)

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

static br_status br__vm_status_from_posix_errno(int err) {
  switch (err) {
    case EACCES:
    case EPERM:
      return BR_STATUS_INVALID_STATE;
    case EINVAL:
    case ENOTSUP:
      return BR_STATUS_INVALID_ARGUMENT;
    case ENOMEM:
      return BR_STATUS_OUT_OF_MEMORY;
    default:
      return BR_STATUS_INVALID_STATE;
  }
}

usize br__vm_platform_page_size_query(void) {
#if defined(_SC_PAGESIZE)
  long page_size = sysconf(_SC_PAGESIZE);

  return page_size > 0 ? (usize)page_size : 4096u;
#else
  return 4096u;
#endif
}

br_status br__vm_platform_commit(void *ptr, usize size) {
  return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0 ? BR_STATUS_OK
                                                          : br__vm_status_from_posix_errno(errno);
}

void br__vm_platform_release(void *ptr, usize size) {
  (void)munmap(ptr, size);
}

bool br__vm_platform_protect(void *ptr, usize size, br_vm_protect_flags flags) {
  int protect = PROT_NONE;

  if ((flags & BR_VM_PROTECT_READ) != 0u) {
    protect |= PROT_READ;
  }
  if ((flags & BR_VM_PROTECT_WRITE) != 0u) {
    protect |= PROT_WRITE;
  }
  if ((flags & BR_VM_PROTECT_EXECUTE) != 0u) {
    protect |= PROT_EXEC;
  }

  return mprotect(ptr, size, protect) == 0;
}

br_vm_mapped_file_result
br__vm_platform_map_open_file(br__vm_native_file file, usize size, br_vm_map_file_flags flags) {
  int prot = 0;
  void *view;

  if ((flags & BR_VM_MAP_FILE_WRITE) != 0u) {
    prot |= PROT_WRITE;
  }
  if ((flags & BR_VM_MAP_FILE_READ) != 0u || (flags & BR_VM_MAP_FILE_WRITE) == 0u) {
    prot |= PROT_READ;
  }

  view = mmap(NULL, size, prot, MAP_SHARED, (int)file, 0);
  if (view == MAP_FAILED || view == NULL) {
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_MAP_FAILURE);
  }

  return br__vm_mapped_file_result((u8 *)view, size, BR_VM_MAP_FILE_ERROR_NONE);
}

void br__vm_platform_unmap_file(br_vm_mapped_file mapping) {
#if defined(MS_SYNC)
  (void)msync(mapping.data, mapping.size, MS_SYNC);
#endif
  (void)munmap(mapping.data, mapping.size);
}

#endif

/* ==== src/mem/virtual/virtual_windows.c ==== */

#if defined(BR__VM_TARGET_WINDOWS)

#include <windows.h>

usize br__vm_platform_page_size_query(void) {
  SYSTEM_INFO info;

  GetSystemInfo(&info);
  return (usize)info.dwPageSize;
}

br_vm_region_result br__vm_platform_reserve(usize size) {
  void *ptr = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);

  if (ptr == NULL) {
    return br__vm_region_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  return br__vm_region_result((u8 *)ptr, size, BR_STATUS_OK);
}

br_status br__vm_platform_commit(void *ptr, usize size) {
  return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL ? BR_STATUS_OK
                                                                     : BR_STATUS_OUT_OF_MEMORY;
}

void br__vm_platform_decommit(void *ptr, usize size) {
  (void)VirtualFree(ptr, size, MEM_DECOMMIT);
}

void br__vm_platform_release(void *ptr, usize size) {
  BR_UNUSED(size);
  (void)VirtualFree(ptr, 0u, MEM_RELEASE);
}

bool br__vm_platform_protect(void *ptr, usize size, br_vm_protect_flags flags) {
  DWORD protect = PAGE_NOACCESS;
  DWORD old_protect = 0u;

  switch (flags) {
    case BR_VM_PROTECT_NONE:
      protect = PAGE_NOACCESS;
      break;
    case BR_VM_PROTECT_READ:
      protect = PAGE_READONLY;
      break;
    case BR_VM_PROTECT_WRITE:
      protect = PAGE_WRITECOPY;
      break;
    case BR_VM_PROTECT_READ | BR_VM_PROTECT_WRITE:
      protect = PAGE_READWRITE;
      break;
    case BR_VM_PROTECT_EXECUTE:
      protect = PAGE_EXECUTE;
      break;
    case BR_VM_PROTECT_EXECUTE | BR_VM_PROTECT_READ:
      protect = PAGE_EXECUTE_READ;
      break;
    case BR_VM_PROTECT_EXECUTE | BR_VM_PROTECT_WRITE:
      protect = PAGE_EXECUTE_WRITECOPY;
      break;
    case BR_VM_PROTECT_EXECUTE | BR_VM_PROTECT_READ | BR_VM_PROTECT_WRITE:
      protect = PAGE_EXECUTE_READWRITE;
      break;
    default:
      return false;
  }

  return VirtualProtect(ptr, size, protect, &old_protect) != 0;
}

br_vm_mapped_file_result
br__vm_platform_map_open_file(br__vm_native_file file, usize size, br_vm_map_file_flags flags) {
  DWORD protect = PAGE_READONLY;
  DWORD map_access = FILE_MAP_READ;
  HANDLE mapping = NULL;
  void *view = NULL;

  if ((flags & BR_VM_MAP_FILE_WRITE) != 0u) {
    protect = PAGE_READWRITE;
    map_access = FILE_MAP_READ | FILE_MAP_WRITE;
  }

  mapping = CreateFileMappingA((HANDLE)(iptr)file, NULL, protect, 0u, 0u, NULL);
  if (mapping == NULL) {
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_MAP_FAILURE);
  }

  view = MapViewOfFile(mapping, map_access, 0u, 0u, size);
  CloseHandle(mapping);
  if (view == NULL) {
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_MAP_FAILURE);
  }

  return br__vm_mapped_file_result((u8 *)view, size, BR_VM_MAP_FILE_ERROR_NONE);
}

void br__vm_platform_unmap_file(br_vm_mapped_file mapping) {
  (void)FlushViewOfFile(mapping.data, mapping.size);
  (void)UnmapViewOfFile(mapping.data);
}

#endif

/* ==== src/strings/builder.c ==== */

enum { BR__STRING_BUILDER_MIN_CAPACITY = 64 };

static br_string_builder_io_result br__string_builder_io_result(usize count, br_status status) {
  br_string_builder_io_result result;

  result.count = count;
  result.status = status;
  return result;
}

static br_string_builder_byte_result br__string_builder_byte_result(u8 value, br_status status) {
  br_string_builder_byte_result result;

  result.value = value;
  result.status = status;
  return result;
}

static br_string_builder_rune_result
br__string_builder_rune_result(br_rune value, usize width, br_status status) {
  br_string_builder_rune_result result;

  result.value = value;
  result.width = width;
  result.status = status;
  return result;
}

static bool br__string_builder_add_overflow(usize lhs, usize rhs, usize *out) {
  if (lhs > SIZE_MAX - rhs) {
    return true;
  }

  *out = lhs + rhs;
  return false;
}

static br_status br__string_builder_grow(br_string_builder *builder, usize additional) {
  br_alloc_result resized;
  usize target_len;
  usize new_cap;

  if (builder == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (additional == 0u) {
    return BR_STATUS_OK;
  }
  if (br__string_builder_add_overflow(builder->len, additional, &target_len)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }
  if (target_len <= builder->cap) {
    return BR_STATUS_OK;
  }
  if (!builder->owns_storage) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  new_cap = builder->cap;
  if (new_cap < BR__STRING_BUILDER_MIN_CAPACITY) {
    new_cap = BR__STRING_BUILDER_MIN_CAPACITY;
  }
  while (new_cap < target_len) {
    usize doubled = new_cap * 2u;

    if (doubled <= new_cap) {
      new_cap = target_len;
      break;
    }
    new_cap = doubled;
  }

  if (builder->data == NULL) {
    resized = br_allocator_alloc_uninit(builder->allocator, new_cap, 1u);
  } else {
    resized =
      br_allocator_resize_uninit(builder->allocator, builder->data, builder->cap, new_cap, 1u);
  }
  if (resized.status != BR_STATUS_OK) {
    return resized.status;
  }

  builder->data = resized.ptr;
  builder->cap = new_cap;
  return BR_STATUS_OK;
}

void br_string_builder_init(br_string_builder *builder, br_allocator allocator) {
  if (builder == NULL) {
    return;
  }

  builder->data = NULL;
  builder->len = 0u;
  builder->cap = 0u;
  builder->allocator = allocator;
  builder->owns_storage = true;
}

br_status br_string_builder_init_with_capacity(br_string_builder *builder,
                                               usize initial_capacity,
                                               br_allocator allocator) {
  br_status status;

  if (builder == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  br_string_builder_init(builder, allocator);
  status = br__string_builder_grow(builder, initial_capacity);
  if (status != BR_STATUS_OK) {
    br_string_builder_destroy(builder);
  }
  return status;
}

void br_string_builder_init_with_backing(br_string_builder *builder,
                                         void *backing,
                                         usize backing_len) {
  if (builder == NULL) {
    return;
  }

  builder->data = (char *)backing;
  builder->len = 0u;
  builder->cap = backing_len;
  builder->allocator = br_allocator_null();
  builder->owns_storage = false;
}

void br_string_builder_destroy(br_string_builder *builder) {
  if (builder == NULL) {
    return;
  }

  if (builder->owns_storage && builder->data != NULL) {
    (void)br_allocator_free(builder->allocator, builder->data, builder->cap);
  }

  builder->data = NULL;
  builder->len = 0u;
  builder->cap = 0u;
  builder->owns_storage = false;
}

void br_string_builder_reset(br_string_builder *builder) {
  if (builder == NULL) {
    return;
  }

  builder->len = 0u;
}

bool br_string_builder_is_empty(const br_string_builder *builder) {
  return builder == NULL || builder->len == 0u;
}

usize br_string_builder_len(const br_string_builder *builder) {
  return builder != NULL ? builder->len : 0u;
}

usize br_string_builder_capacity(const br_string_builder *builder) {
  return builder != NULL ? builder->cap : 0u;
}

usize br_string_builder_space(const br_string_builder *builder) {
  if (builder == NULL || builder->cap < builder->len) {
    return 0u;
  }

  return builder->cap - builder->len;
}

br_string_view br_string_builder_view(const br_string_builder *builder) {
  if (builder == NULL || builder->len == 0u) {
    return br_string_view_make(NULL, 0u);
  }

  return br_string_view_make(builder->data, builder->len);
}

br_status br_string_builder_reserve(br_string_builder *builder, usize additional) {
  return br__string_builder_grow(builder, additional);
}

br_status br_string_builder_truncate(br_string_builder *builder, usize n) {
  if (builder == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (n > builder->len) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  builder->len = n;
  return BR_STATUS_OK;
}

br_string_result br_string_builder_clone(const br_string_builder *builder, br_allocator allocator) {
  if (builder == NULL) {
    return br_string_clone(br_string_view_make(NULL, 0u), allocator);
  }

  return br_string_clone(br_string_builder_view(builder), allocator);
}

br_string_builder_io_result br_string_builder_write(br_string_builder *builder,
                                                    br_string_view src) {
  br_status status;

  if (builder == NULL) {
    return br__string_builder_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (src.len == 0u) {
    return br__string_builder_io_result(0u, BR_STATUS_OK);
  }

  status = br__string_builder_grow(builder, src.len);
  if (status != BR_STATUS_OK) {
    return br__string_builder_io_result(0u, status);
  }

  memcpy(builder->data + builder->len, src.data, src.len);
  builder->len += src.len;
  return br__string_builder_io_result(src.len, BR_STATUS_OK);
}

br_status br_string_builder_write_byte(br_string_builder *builder, u8 byte_value) {
  br_status status;

  if (builder == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  status = br__string_builder_grow(builder, 1u);
  if (status != BR_STATUS_OK) {
    return status;
  }

  builder->data[builder->len] = (char)byte_value;
  builder->len += 1u;
  return BR_STATUS_OK;
}

br_string_builder_io_result br_string_builder_write_rune(br_string_builder *builder,
                                                         br_rune value) {
  br_utf8_encode_result encoded;
  br_status status;

  if (builder == NULL) {
    return br__string_builder_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  encoded = br_utf8_encode(value);
  status = br__string_builder_grow(builder, encoded.len);
  if (status != BR_STATUS_OK) {
    return br__string_builder_io_result(0u, status);
  }

  memcpy(builder->data + builder->len, encoded.bytes, encoded.len);
  builder->len += encoded.len;
  return br__string_builder_io_result(encoded.len, BR_STATUS_OK);
}

br_string_builder_byte_result br_string_builder_pop_byte(br_string_builder *builder) {
  if (builder == NULL) {
    return br__string_builder_byte_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (builder->len == 0u) {
    return br__string_builder_byte_result(0u, BR_STATUS_INVALID_STATE);
  }

  builder->len -= 1u;
  return br__string_builder_byte_result((u8)builder->data[builder->len], BR_STATUS_OK);
}

br_string_builder_rune_result br_string_builder_pop_rune(br_string_builder *builder) {
  br_utf8_decode_result decoded;

  if (builder == NULL) {
    return br__string_builder_rune_result(0, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (builder->len == 0u) {
    return br__string_builder_rune_result(0, 0u, BR_STATUS_INVALID_STATE);
  }

  decoded = br_utf8_decode_last(br_bytes_view_make(builder->data, builder->len));
  builder->len -= decoded.width;
  return br__string_builder_rune_result(decoded.value, decoded.width, BR_STATUS_OK);
}

static br_i64_result br__string_builder_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  br_string_builder *builder;
  br_string_builder_io_result io_result;
  br_io_mode_set modes;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  builder = (br_string_builder *)context;
  switch (mode) {
    case BR_IO_MODE_WRITE:
      io_result = br_string_builder_write(builder, br_string_view_make(data, data_len));
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_SIZE:
      return br_i64_result_make((i64)br_string_builder_len(builder), BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_WRITE) | br_io_mode_bit(BR_IO_MODE_SIZE);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_string_builder_as_stream(br_string_builder *builder) {
  return br_stream_make(builder, br__string_builder_stream_proc);
}

/* ==== src/strings/reader.c ==== */

static br_string_reader_io_result br__string_reader_io_result(usize count, br_status status) {
  br_string_reader_io_result result;

  result.count = count;
  result.status = status;
  return result;
}

static br_string_reader_byte_result br__string_reader_byte_result(u8 value, br_status status) {
  br_string_reader_byte_result result;

  result.value = value;
  result.status = status;
  return result;
}

static br_string_reader_rune_result
br__string_reader_rune_result(br_rune value, usize width, br_status status) {
  br_string_reader_rune_result result;

  result.value = value;
  result.width = width;
  result.status = status;
  return result;
}

static br_string_reader_seek_result br__string_reader_seek_result(i64 offset, br_status status) {
  br_string_reader_seek_result result;

  result.offset = offset;
  result.status = status;
  return result;
}

void br_string_reader_init(br_string_reader *reader, br_string_view source) {
  if (reader == NULL) {
    return;
  }

  reader->source = source;
  reader->index = 0;
  reader->prev_rune = -1;
}

void br_string_reader_reset(br_string_reader *reader) {
  if (reader == NULL) {
    return;
  }

  reader->index = 0;
  reader->prev_rune = -1;
}

br_string_view br_string_reader_view(const br_string_reader *reader) {
  usize remaining;

  if (reader == NULL) {
    return br_string_view_make(NULL, 0u);
  }

  remaining = br_string_reader_len(reader);
  if (remaining == 0u) {
    return br_string_view_make(NULL, 0u);
  }

  return br_string_view_make(reader->source.data + (usize)reader->index, remaining);
}

usize br_string_reader_len(const br_string_reader *reader) {
  if (reader == NULL || reader->index >= (i64)reader->source.len) {
    return 0u;
  }
  if (reader->index < 0) {
    return reader->source.len;
  }

  return reader->source.len - (usize)reader->index;
}

usize br_string_reader_size(const br_string_reader *reader) {
  return reader != NULL ? reader->source.len : 0u;
}

br_string_reader_io_result
br_string_reader_read(br_string_reader *reader, void *dst, usize dst_len) {
  usize count;

  if (reader == NULL || (dst == NULL && dst_len > 0u)) {
    return br__string_reader_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_len == 0u) {
    return br__string_reader_io_result(0u, BR_STATUS_OK);
  }
  if (reader->index >= (i64)reader->source.len) {
    reader->prev_rune = -1;
    return br__string_reader_io_result(0u, BR_STATUS_EOF);
  }

  reader->prev_rune = -1;
  count = br_min_size(dst_len, reader->source.len - (usize)reader->index);
  memcpy(dst, reader->source.data + (usize)reader->index, count);
  reader->index += (i64)count;
  return br__string_reader_io_result(count, BR_STATUS_OK);
}

br_string_reader_io_result
br_string_reader_read_at(const br_string_reader *reader, void *dst, usize dst_len, i64 offset) {
  usize count;

  if (reader == NULL || (dst == NULL && dst_len > 0u)) {
    return br__string_reader_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_len == 0u) {
    return br__string_reader_io_result(0u, BR_STATUS_OK);
  }
  if (offset < 0) {
    return br__string_reader_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (offset >= (i64)reader->source.len) {
    return br__string_reader_io_result(0u, BR_STATUS_EOF);
  }

  count = br_min_size(dst_len, reader->source.len - (usize)offset);
  memcpy(dst, reader->source.data + (usize)offset, count);
  if (count < dst_len) {
    return br__string_reader_io_result(count, BR_STATUS_EOF);
  }
  return br__string_reader_io_result(count, BR_STATUS_OK);
}

br_string_reader_byte_result br_string_reader_read_byte(br_string_reader *reader) {
  if (reader == NULL) {
    return br__string_reader_byte_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  reader->prev_rune = -1;
  if (reader->index >= (i64)reader->source.len) {
    return br__string_reader_byte_result(0u, BR_STATUS_EOF);
  }

  reader->index += 1;
  return br__string_reader_byte_result((u8)reader->source.data[(usize)reader->index - 1u],
                                       BR_STATUS_OK);
}

br_status br_string_reader_unread_byte(br_string_reader *reader) {
  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (reader->index <= 0) {
    return BR_STATUS_INVALID_STATE;
  }

  reader->prev_rune = -1;
  reader->index -= 1;
  return BR_STATUS_OK;
}

br_string_reader_rune_result br_string_reader_read_rune(br_string_reader *reader) {
  br_utf8_decode_result decoded;
  const char *remaining;
  usize remaining_len;

  if (reader == NULL) {
    return br__string_reader_rune_result(0, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (reader->index >= (i64)reader->source.len) {
    reader->prev_rune = -1;
    return br__string_reader_rune_result(0, 0u, BR_STATUS_EOF);
  }

  reader->prev_rune = reader->index;
  remaining = reader->source.data + (usize)reader->index;
  remaining_len = reader->source.len - (usize)reader->index;
  if ((u8)remaining[0] < (u8)BR_RUNE_SELF) {
    reader->index += 1;
    return br__string_reader_rune_result((br_rune)(u8)remaining[0], 1u, BR_STATUS_OK);
  }

  decoded = br_utf8_decode(br_bytes_view_make(remaining, remaining_len));
  reader->index += (i64)decoded.width;
  return br__string_reader_rune_result(decoded.value, decoded.width, BR_STATUS_OK);
}

br_status br_string_reader_unread_rune(br_string_reader *reader) {
  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (reader->index <= 0 || reader->prev_rune < 0) {
    return BR_STATUS_INVALID_STATE;
  }

  reader->index = reader->prev_rune;
  reader->prev_rune = -1;
  return BR_STATUS_OK;
}

br_string_reader_seek_result
br_string_reader_seek(br_string_reader *reader, i64 offset, br_seek_from whence) {
  i64 absolute;

  if (reader == NULL) {
    return br__string_reader_seek_result(0, BR_STATUS_INVALID_ARGUMENT);
  }

  reader->prev_rune = -1;
  switch (whence) {
    case BR_SEEK_FROM_START:
      absolute = offset;
      break;
    case BR_SEEK_FROM_CURRENT:
      absolute = reader->index + offset;
      break;
    case BR_SEEK_FROM_END:
      absolute = (i64)reader->source.len + offset;
      break;
    default:
      return br__string_reader_seek_result(0, BR_STATUS_INVALID_ARGUMENT);
  }

  if (absolute < 0) {
    return br__string_reader_seek_result(0, BR_STATUS_INVALID_ARGUMENT);
  }

  reader->index = absolute;
  return br__string_reader_seek_result(absolute, BR_STATUS_OK);
}

static br_i64_result br__string_reader_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  br_string_reader *reader;
  br_string_reader_io_result io_result;
  br_string_reader_seek_result seek_result;
  br_io_mode_set modes;

  reader = (br_string_reader *)context;
  switch (mode) {
    case BR_IO_MODE_READ:
      io_result = br_string_reader_read(reader, data, data_len);
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_READ_AT:
      io_result = br_string_reader_read_at(reader, data, data_len, offset);
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_SEEK:
      seek_result = br_string_reader_seek(reader, offset, whence);
      return br_i64_result_make(seek_result.offset, seek_result.status);
    case BR_IO_MODE_SIZE:
      return br_i64_result_make((i64)br_string_reader_size(reader), BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_READ) | br_io_mode_bit(BR_IO_MODE_READ_AT) |
              br_io_mode_bit(BR_IO_MODE_SEEK) | br_io_mode_bit(BR_IO_MODE_SIZE);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_string_reader_as_stream(br_string_reader *reader) {
  return br_stream_make(reader, br__string_reader_stream_proc);
}

/* ==== src/strings/strings.c ==== */

static br_string_result br__string_result(void *data, usize len, br_status status) {
  br_string_result result;

  result.value = br_string_make(data, len);
  result.status = status;
  return result;
}

static br_string_view_list_result
br__string_view_list_result(br_string_view *data, usize len, br_status status) {
  br_string_view_list_result result;

  result.value.data = data;
  result.value.len = len;
  result.status = status;
  return result;
}

static br_string_rewrite_result br__string_rewrite_alias_result(br_string_view value) {
  br_string_rewrite_result result;

  result.value = value;
  result.owned = br_string_make(NULL, 0u);
  result.allocated = false;
  result.status = BR_STATUS_OK;
  return result;
}

static br_string_rewrite_result
br__string_rewrite_owned_result(void *data, usize len, br_status status) {
  br_string_rewrite_result result;

  result.owned = br_string_make(data, len);
  result.value = br_string_view_from_string(result.owned);
  result.allocated = status == BR_STATUS_OK;
  result.status = status;
  return result;
}

static bool br__string_add_overflow(usize lhs, usize rhs, usize *out) {
  if (lhs > SIZE_MAX - rhs) {
    return true;
  }

  *out = lhs + rhs;
  return false;
}

br_status br_string_free(br_string string, br_allocator allocator) {
  return br_allocator_free(allocator, string.data, string.len);
}

br_status br_string_view_list_free(br_string_view_list list, br_allocator allocator) {
  return br_allocator_free(allocator, list.data, list.len * sizeof(br_string_view));
}

br_status br_string_rewrite_free(br_string_rewrite_result result, br_allocator allocator) {
  if (!result.allocated) {
    return BR_STATUS_OK;
  }
  return br_string_free(result.owned, allocator);
}

br_string_result br_string_clone(br_string_view s, br_allocator allocator) {
  br_bytes_result result;

  result = br_bytes_clone(br_string_view_as_bytes(s), allocator);
  return br__string_result(result.value.data, result.value.len, result.status);
}

i32 br_string_compare(br_string_view lhs, br_string_view rhs) {
  return br_bytes_compare(br_string_view_as_bytes(lhs), br_string_view_as_bytes(rhs));
}

bool br_string_equal(br_string_view lhs, br_string_view rhs) {
  return br_bytes_equal(br_string_view_as_bytes(lhs), br_string_view_as_bytes(rhs));
}

bool br_string_has_prefix(br_string_view s, br_string_view prefix) {
  return br_bytes_has_prefix(br_string_view_as_bytes(s), br_string_view_as_bytes(prefix));
}

bool br_string_has_suffix(br_string_view s, br_string_view suffix) {
  return br_bytes_has_suffix(br_string_view_as_bytes(s), br_string_view_as_bytes(suffix));
}

bool br_string_contains(br_string_view s, br_string_view needle) {
  return br_bytes_contains(br_string_view_as_bytes(s), br_string_view_as_bytes(needle));
}

bool br_string_contains_any(br_string_view s, br_string_view chars) {
  return br_string_index_any(s, chars) >= 0;
}

bool br_string_contains_rune(br_string_view s, br_rune value) {
  return br_string_index_rune(s, value) >= 0;
}

bool br_string_valid(br_string_view s) {
  return br_utf8_valid(br_string_view_as_bytes(s));
}

isize br_string_index(br_string_view s, br_string_view needle) {
  return br_bytes_index(br_string_view_as_bytes(s), br_string_view_as_bytes(needle));
}

isize br_string_index_byte(br_string_view s, u8 byte_value) {
  return br_bytes_index_byte(br_string_view_as_bytes(s), byte_value);
}

isize br_string_index_rune(br_string_view s, br_rune value) {
  br_utf8_encode_result encoded;
  br_string_view encoded_view;
  usize offset = 0u;

  if ((u32)value < (u32)BR_RUNE_SELF) {
    return br_string_index_byte(s, (u8)value);
  }
  if (value == BR_RUNE_ERROR) {
    while (offset < s.len) {
      br_utf8_decode_result decoded;

      decoded = br_utf8_decode(br_bytes_view_make(s.data + offset, s.len - offset));
      if (decoded.value == BR_RUNE_ERROR) {
        return (isize)offset;
      }
      if (decoded.width == 0u) {
        break;
      }
      offset += decoded.width;
    }
    return -1;
  }
  if (!br_utf8_valid_rune(value)) {
    return -1;
  }

  encoded = br_utf8_encode(value);
  encoded_view = br_string_view_make(encoded.bytes, encoded.len);
  return br_string_index(s, encoded_view);
}

isize br_string_last_index(br_string_view s, br_string_view needle) {
  return br_bytes_last_index(br_string_view_as_bytes(s), br_string_view_as_bytes(needle));
}

isize br_string_last_index_byte(br_string_view s, u8 byte_value) {
  return br_bytes_last_index_byte(br_string_view_as_bytes(s), byte_value);
}

isize br_string_index_any(br_string_view s, br_string_view chars) {
  usize offset = 0u;

  if (chars.len == 0u) {
    return -1;
  }
  if (chars.len == 1u) {
    br_rune value = (u8)chars.data[0];

    if (value >= BR_RUNE_SELF) {
      value = BR_RUNE_ERROR;
    }
    return br_string_index_rune(s, value);
  }

  while (offset < s.len) {
    br_utf8_decode_result decoded;

    decoded = br_utf8_decode(br_bytes_view_make(s.data + offset, s.len - offset));
    if (br_string_index_rune(chars, decoded.value) >= 0) {
      return (isize)offset;
    }
    if (decoded.width == 0u) {
      break;
    }
    offset += decoded.width;
  }

  return -1;
}

isize br_string_last_index_any(br_string_view s, br_string_view chars) {
  usize end = s.len;

  if (chars.len == 0u) {
    return -1;
  }

  while (end > 0u) {
    br_utf8_decode_result decoded;
    usize offset;

    decoded = br_utf8_decode_last(br_bytes_view_make(s.data, end));
    if (decoded.width == 0u) {
      break;
    }
    offset = end - decoded.width;
    if (br_string_index_rune(chars, decoded.value) >= 0) {
      return (isize)offset;
    }
    end = offset;
  }

  return -1;
}

usize br_string_count(br_string_view s, br_string_view needle) {
  if (needle.len == 0u) {
    return br_string_rune_count(s) + 1u;
  }
  return (usize)br_bytes_count(br_string_view_as_bytes(s), br_string_view_as_bytes(needle));
}

usize br_string_rune_count(br_string_view s) {
  return br_utf8_rune_count(br_string_view_as_bytes(s));
}

br_string_result br_string_join(const br_string_view *parts,
                                usize part_count,
                                br_string_view sep,
                                br_allocator allocator) {
  br_bytes_result result;

  result = br_bytes_join(
    (const br_bytes_view *)parts, part_count, br_string_view_as_bytes(sep), allocator);
  return br__string_result(result.value.data, result.value.len, result.status);
}

br_string_result
br_string_concat(const br_string_view *parts, usize part_count, br_allocator allocator) {
  br_bytes_result result;

  result = br_bytes_concat((const br_bytes_view *)parts, part_count, allocator);
  return br__string_result(result.value.data, result.value.len, result.status);
}

br_string_result br_string_repeat(br_string_view s, usize count, br_allocator allocator) {
  br_bytes_result result;

  result = br_bytes_repeat(br_string_view_as_bytes(s), count, allocator);
  return br__string_result(result.value.data, result.value.len, result.status);
}

static br_string_view_list_result br__string_split_impl(
  br_string_view s, br_string_view sep, usize sep_save, isize n, br_allocator allocator) {
  br_alloc_result alloc;
  br_string_view *parts;
  usize target_count;
  usize part_index = 0u;

  if (n == 0) {
    return br__string_view_list_result(NULL, 0u, BR_STATUS_OK);
  }

  if (sep.len == 0u) {
    target_count = br_string_rune_count(s);
    if (n >= 0 && (usize)n < target_count) {
      target_count = (usize)n;
    }
    if (target_count == 0u) {
      return br__string_view_list_result(NULL, 0u, BR_STATUS_OK);
    }

    alloc = br_allocator_alloc_uninit(
      allocator, target_count * sizeof(br_string_view), _Alignof(br_string_view));
    if (alloc.status != BR_STATUS_OK) {
      return br__string_view_list_result(NULL, 0u, alloc.status);
    }

    parts = alloc.ptr;
    for (; part_index + 1u < target_count; ++part_index) {
      br_utf8_decode_result decoded;

      decoded = br_utf8_decode(br_string_view_as_bytes(s));
      if (decoded.width == 0u) {
        break;
      }
      parts[part_index] = br_string_view_make(s.data, decoded.width);
      s = br_string_view_make(s.data + decoded.width, s.len - decoded.width);
    }
    parts[part_index] = s;
    return br__string_view_list_result(parts, part_index + 1u, BR_STATUS_OK);
  }

  if (n < 0) {
    target_count = br_string_count(s, sep) + 1u;
  } else {
    target_count = (usize)n;
  }

  if (target_count == 0u) {
    return br__string_view_list_result(NULL, 0u, BR_STATUS_OK);
  }

  alloc = br_allocator_alloc_uninit(
    allocator, target_count * sizeof(br_string_view), _Alignof(br_string_view));
  if (alloc.status != BR_STATUS_OK) {
    return br__string_view_list_result(NULL, 0u, alloc.status);
  }

  parts = alloc.ptr;
  if (target_count > 0u) {
    usize remaining_slots = target_count - 1u;

    while (part_index < remaining_slots) {
      isize split_at = br_string_index(s, sep);
      if (split_at < 0) {
        break;
      }

      parts[part_index] = br_string_view_make(s.data, (usize)split_at + sep_save);
      s = br_string_view_make(s.data + (usize)split_at + sep.len,
                              s.len - ((usize)split_at + sep.len));
      part_index += 1u;
    }
  }

  parts[part_index] = s;
  return br__string_view_list_result(parts, part_index + 1u, BR_STATUS_OK);
}

br_string_view_list_result
br_string_split(br_string_view s, br_string_view sep, br_allocator allocator) {
  return br__string_split_impl(s, sep, 0u, -1, allocator);
}

br_string_view_list_result
br_string_split_n(br_string_view s, br_string_view sep, isize n, br_allocator allocator) {
  return br__string_split_impl(s, sep, 0u, n, allocator);
}

br_string_view_list_result
br_string_split_after(br_string_view s, br_string_view sep, br_allocator allocator) {
  return br__string_split_impl(s, sep, sep.len, -1, allocator);
}

br_string_view_list_result
br_string_split_after_n(br_string_view s, br_string_view sep, isize n, br_allocator allocator) {
  return br__string_split_impl(s, sep, sep.len, n, allocator);
}

br_string_rewrite_result br_string_replace(br_string_view s,
                                           br_string_view old_string,
                                           br_string_view new_string,
                                           isize n,
                                           br_allocator allocator) {
  br_alloc_result alloc;
  usize match_count;
  usize replace_count;
  usize output_len;
  usize start = 0u;
  usize write_offset = 0u;

  if ((old_string.len == new_string.len &&
       (old_string.len == 0u || br_string_equal(old_string, new_string))) ||
      n == 0) {
    return br__string_rewrite_alias_result(s);
  }

  match_count = br_string_count(s, old_string);
  if (match_count == 0u) {
    return br__string_rewrite_alias_result(s);
  }

  replace_count = match_count;
  if (n >= 0 && (usize)n < replace_count) {
    replace_count = (usize)n;
  }

  if (new_string.len >= old_string.len) {
    usize extra_per_match = new_string.len - old_string.len;
    usize extra_total;

    if (replace_count > 0u && extra_per_match > SIZE_MAX / replace_count) {
      return br__string_rewrite_owned_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
    extra_total = extra_per_match * replace_count;
    if (br__string_add_overflow(s.len, extra_total, &output_len)) {
      return br__string_rewrite_owned_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
  } else {
    output_len = s.len - ((old_string.len - new_string.len) * replace_count);
  }

  if (output_len == 0u) {
    return br__string_rewrite_owned_result(NULL, 0u, BR_STATUS_OK);
  }

  alloc = br_allocator_alloc_uninit(allocator, output_len, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__string_rewrite_owned_result(NULL, 0u, alloc.status);
  }

  for (usize i = 0; i < replace_count; ++i) {
    usize split_at;
    usize next_start;

    if (old_string.len == 0u) {
      split_at = start;
      if (i > 0u && split_at < s.len) {
        br_utf8_decode_result decoded;

        decoded = br_utf8_decode(br_bytes_view_make(s.data + start, s.len - start));
        if (decoded.width > 0u) {
          split_at += decoded.width;
        }
      }
      next_start = split_at;
    } else {
      isize local_index =
        br_string_index(br_string_view_make(s.data + start, s.len - start), old_string);
      split_at = start + (usize)local_index;
      next_start = split_at + old_string.len;
    }

    if (split_at > start) {
      memcpy((u8 *)alloc.ptr + write_offset, s.data + start, split_at - start);
      write_offset += split_at - start;
    }
    if (new_string.len > 0u) {
      memcpy((u8 *)alloc.ptr + write_offset, new_string.data, new_string.len);
      write_offset += new_string.len;
    }
    start = next_start;
  }

  if (start < s.len) {
    memcpy((u8 *)alloc.ptr + write_offset, s.data + start, s.len - start);
    write_offset += s.len - start;
  }

  return br__string_rewrite_owned_result(alloc.ptr, write_offset, BR_STATUS_OK);
}

br_string_rewrite_result br_string_replace_all(br_string_view s,
                                               br_string_view old_string,
                                               br_string_view new_string,
                                               br_allocator allocator) {
  return br_string_replace(s, old_string, new_string, -1, allocator);
}

br_string_rewrite_result
br_string_remove(br_string_view s, br_string_view key, isize n, br_allocator allocator) {
  return br_string_replace(s, key, br_string_view_make(NULL, 0u), n, allocator);
}

br_string_rewrite_result
br_string_remove_all(br_string_view s, br_string_view key, br_allocator allocator) {
  return br_string_remove(s, key, -1, allocator);
}

br_string_view br_string_truncate_to_byte(br_string_view s, u8 byte_value) {
  br_bytes_view truncated;

  truncated = br_bytes_truncate_to_byte(br_string_view_as_bytes(s), byte_value);
  return br_string_view_make(truncated.data, truncated.len);
}

br_string_view br_string_truncate_to_rune(br_string_view s, br_rune value) {
  isize index = br_string_index_rune(s, value);

  if (index < 0) {
    return s;
  }
  return br_string_view_make(s.data, (usize)index);
}

br_string_view br_string_trim_prefix(br_string_view s, br_string_view prefix) {
  br_bytes_view trimmed;

  trimmed = br_bytes_trim_prefix(br_string_view_as_bytes(s), br_string_view_as_bytes(prefix));
  return br_string_view_make(trimmed.data, trimmed.len);
}

br_string_view br_string_trim_suffix(br_string_view s, br_string_view suffix) {
  br_bytes_view trimmed;

  trimmed = br_bytes_trim_suffix(br_string_view_as_bytes(s), br_string_view_as_bytes(suffix));
  return br_string_view_make(trimmed.data, trimmed.len);
}

/* ==== src/sync/atomic.c ==== */

void br_cpu_relax(void) {
#if defined(__i386__) || defined(__x86_64__)
  __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
  __asm__ __volatile__("yield");
#else
  br_atomic_signal_fence(BR_ATOMIC_SEQ_CST);
#endif
}

/* ==== src/sync/extended.c ==== */

br_status br_wait_group_init(br_wait_group *wg) {
  br_status status;

  if (wg == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  wg->counter = 0;
  status = br_mutex_init(&wg->mutex);
  if (status != BR_STATUS_OK) {
    return status;
  }

  status = br_cond_init(&wg->cond);
  if (status != BR_STATUS_OK) {
    br_mutex_destroy(&wg->mutex);
    return status;
  }

  return BR_STATUS_OK;
}

void br_wait_group_destroy(br_wait_group *wg) {
  if (wg == NULL) {
    return;
  }
  br_cond_destroy(&wg->cond);
  br_mutex_destroy(&wg->mutex);
}

void br_wait_group_add(br_wait_group *wg, i32 delta) {
  if (wg == NULL || delta == 0) {
    return;
  }

  br_mutex_lock(&wg->mutex);
  wg->counter += delta;
  if (wg->counter == 0) {
    br_cond_broadcast(&wg->cond);
  }
  br_mutex_unlock(&wg->mutex);
}

void br_wait_group_done(br_wait_group *wg) {
  br_wait_group_add(wg, -1);
}

void br_wait_group_wait(br_wait_group *wg) {
  if (wg == NULL) {
    return;
  }

  br_mutex_lock(&wg->mutex);
  while (wg->counter != 0) {
    br_cond_wait(&wg->cond, &wg->mutex);
  }
  br_mutex_unlock(&wg->mutex);
}

bool br_wait_group_wait_with_timeout(br_wait_group *wg, br_duration duration) {
  br_tick start;
  bool ok = true;

  if (wg == NULL || duration <= 0) {
    return false;
  }

  start = br_tick_now();
  br_mutex_lock(&wg->mutex);
  while (wg->counter != 0) {
    br_duration remaining = duration - br_tick_since(start);
    if (remaining < 0) {
      ok = false;
      break;
    }

    /*
    Odin currently passes the full duration to every condition wait iteration.
    Bedrock tracks the remaining duration so a spurious wakeup does not restart
    the public wait-group timeout window.
    */
    if (!br_cond_wait_with_timeout(&wg->cond, &wg->mutex, remaining)) {
      ok = false;
      break;
    }
  }
  br_mutex_unlock(&wg->mutex);
  return ok;
}

br_status br_barrier_init(br_barrier *barrier, i32 thread_count) {
  br_status status;

  if (barrier == NULL || thread_count <= 0) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  status = br_mutex_init(&barrier->mutex);
  if (status != BR_STATUS_OK) {
    return status;
  }

  status = br_cond_init(&barrier->cond);
  if (status != BR_STATUS_OK) {
    br_mutex_destroy(&barrier->mutex);
    return status;
  }

  barrier->index = 0;
  barrier->generation = 0;
  barrier->thread_count = thread_count;
  return BR_STATUS_OK;
}

void br_barrier_destroy(br_barrier *barrier) {
  if (barrier == NULL) {
    return;
  }
  br_cond_destroy(&barrier->cond);
  br_mutex_destroy(&barrier->mutex);
}

bool br_barrier_wait(br_barrier *barrier) {
  i32 generation;

  if (barrier == NULL) {
    return false;
  }

  br_mutex_lock(&barrier->mutex);
  generation = barrier->generation;
  barrier->index += 1;
  if (barrier->index < barrier->thread_count) {
    while (generation == barrier->generation && barrier->index < barrier->thread_count) {
      br_cond_wait(&barrier->cond, &barrier->mutex);
    }
    br_mutex_unlock(&barrier->mutex);
    return false;
  }

  barrier->index = 0;
  barrier->generation += 1;
  br_cond_broadcast(&barrier->cond);
  br_mutex_unlock(&barrier->mutex);
  return true;
}

br_status br_once_init(br_once *once) {
  if (once == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  br_atomic_init(&once->done, false);
  return br_mutex_init(&once->mutex);
}

void br_once_destroy(br_once *once) {
  if (once == NULL) {
    return;
  }
  br_mutex_destroy(&once->mutex);
}

void br_once_do(br_once *once, br_once_fn fn, void *ctx) {
  if (once == NULL || fn == NULL) {
    return;
  }

  if (br_atomic_load_explicit(&once->done, BR_ATOMIC_ACQUIRE)) {
    return;
  }

  br_mutex_lock(&once->mutex);
  if (!br_atomic_load_explicit(&once->done, BR_ATOMIC_RELAXED)) {
    fn(ctx);
    br_atomic_store_explicit(&once->done, true, BR_ATOMIC_RELEASE);
  }
  br_mutex_unlock(&once->mutex);
}

typedef struct br__once_call0_ctx {
  br_once_fn0 fn;
} br__once_call0_ctx;

static void br__once_call0(void *ctx) {
  br__once_call0_ctx *call0 = (br__once_call0_ctx *)ctx;

  call0->fn();
}

void br_once_do0(br_once *once, br_once_fn0 fn) {
  br__once_call0_ctx ctx;

  if (fn == NULL) {
    return;
  }

  ctx.fn = fn;
  br_once_do(once, br__once_call0, &ctx);
}

br_status br_auto_reset_event_init(br_auto_reset_event *event) {
  if (event == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  br_atomic_init(&event->status, 0);
  return br_sema_init(&event->sema, 0u);
}

void br_auto_reset_event_destroy(br_auto_reset_event *event) {
  if (event == NULL) {
    return;
  }
  br_sema_destroy(&event->sema);
}

void br_auto_reset_event_signal(br_auto_reset_event *event) {
  i32 old_status;

  if (event == NULL) {
    return;
  }

  old_status = br_atomic_load_explicit(&event->status, BR_ATOMIC_RELAXED);
  for (;;) {
    i32 expected = old_status;
    i32 new_status = old_status < 1 ? old_status + 1 : 1;

    if (br_atomic_compare_exchange_weak_explicit(
          &event->status, &expected, new_status, BR_ATOMIC_RELEASE, BR_ATOMIC_RELAXED)) {
      break;
    }

    old_status = expected;
    br_cpu_relax();
  }

  if (old_status < 0) {
    br_sema_post(&event->sema, 1u);
  }
}

void br_auto_reset_event_wait(br_auto_reset_event *event) {
  i32 old_status;

  if (event == NULL) {
    return;
  }

  old_status = br_atomic_sub_explicit(&event->status, 1, BR_ATOMIC_ACQUIRE);
  if (old_status < 1) {
    br_sema_wait(&event->sema);
  }
}

#define BR__PARKER_EMPTY ((u32)0)
#define BR__PARKER_NOTIFIED ((u32)1)
#define BR__PARKER_PARKED UINT32_MAX

void br_parker_init(br_parker *parker) {
  if (parker == NULL) {
    return;
  }

  br_atomic_init(&parker->state, BR__PARKER_EMPTY);
}

void br_parker_park(br_parker *parker) {
  if (parker == NULL) {
    return;
  }

  if (br_atomic_sub_explicit(&parker->state, 1u, BR_ATOMIC_ACQUIRE) == BR__PARKER_NOTIFIED) {
    return;
  }

  for (;;) {
    u32 expected = BR__PARKER_NOTIFIED;

    BR_UNUSED(br_futex_wait(&parker->state, BR__PARKER_PARKED));
    if (br_atomic_compare_exchange_strong_explicit(
          &parker->state, &expected, BR__PARKER_EMPTY, BR_ATOMIC_ACQUIRE, BR_ATOMIC_ACQUIRE)) {
      return;
    }
  }
}

static bool br__parker_finish_timeout(br_parker *parker) {
  u32 expected = BR__PARKER_PARKED;

  if (br_atomic_compare_exchange_strong_explicit(
        &parker->state, &expected, BR__PARKER_EMPTY, BR_ATOMIC_ACQUIRE, BR_ATOMIC_ACQUIRE)) {
    return false;
  }

  expected = BR__PARKER_NOTIFIED;
  if (br_atomic_compare_exchange_strong_explicit(
        &parker->state, &expected, BR__PARKER_EMPTY, BR_ATOMIC_ACQUIRE, BR_ATOMIC_ACQUIRE)) {
    return true;
  }

  return false;
}

bool br_parker_park_with_timeout(br_parker *parker, br_duration duration) {
  br_tick start;
  br_duration remaining;

  if (parker == NULL) {
    return false;
  }

  if (br_atomic_sub_explicit(&parker->state, 1u, BR_ATOMIC_ACQUIRE) == BR__PARKER_NOTIFIED) {
    return true;
  }
  if (duration <= 0) {
    return br__parker_finish_timeout(parker);
  }

  start = br_tick_now();
  remaining = duration;
  for (;;) {
    u32 expected;

    if (!br_futex_wait_with_timeout(&parker->state, BR__PARKER_PARKED, remaining)) {
      return br__parker_finish_timeout(parker);
    }

    expected = BR__PARKER_NOTIFIED;
    if (br_atomic_compare_exchange_strong_explicit(
          &parker->state, &expected, BR__PARKER_EMPTY, BR_ATOMIC_ACQUIRE, BR_ATOMIC_ACQUIRE)) {
      return true;
    }

    remaining = duration - br_tick_since(start);
    if (remaining <= 0) {
      return br__parker_finish_timeout(parker);
    }
  }
}

void br_parker_unpark(br_parker *parker) {
  if (parker == NULL) {
    return;
  }

  if (br_atomic_exchange_explicit(&parker->state, BR__PARKER_NOTIFIED, BR_ATOMIC_RELEASE) ==
      BR__PARKER_PARKED) {
    br_futex_signal(&parker->state);
  }
}

void br_one_shot_event_init(br_one_shot_event *event) {
  if (event == NULL) {
    return;
  }

  br_atomic_init(&event->state, 0u);
}

void br_one_shot_event_wait(br_one_shot_event *event) {
  if (event == NULL) {
    return;
  }

  while (br_atomic_load_explicit(&event->state, BR_ATOMIC_ACQUIRE) == 0u) {
    BR_UNUSED(br_futex_wait(&event->state, 0u));
  }
}

void br_one_shot_event_signal(br_one_shot_event *event) {
  if (event == NULL) {
    return;
  }

  br_atomic_store_explicit(&event->state, 1u, BR_ATOMIC_RELEASE);
  br_futex_broadcast(&event->state);
}

void br_ticket_mutex_init(br_ticket_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }

  br_atomic_init(&mutex->ticket, 0u);
  br_atomic_init(&mutex->serving, 0u);
}

void br_ticket_mutex_lock(br_ticket_mutex *mutex) {
  u32 ticket;

  if (mutex == NULL) {
    return;
  }

  ticket = br_atomic_add_explicit(&mutex->ticket, 1u, BR_ATOMIC_RELAXED);
  while (ticket != br_atomic_load_explicit(&mutex->serving, BR_ATOMIC_ACQUIRE)) {
    br_cpu_relax();
  }
}

void br_ticket_mutex_unlock(br_ticket_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }

  br_atomic_add_explicit(&mutex->serving, 1u, BR_ATOMIC_RELEASE);
}

/* ==== src/sync/futex_darwin.c ==== */

#if defined(__APPLE__) && defined(__MACH__)


#include <errno.h>

#define BR__DARWIN_OS_SYNC_FLAGS 0u
#define BR__DARWIN_MACH_ABSOLUTE_TIME 32u
#define BR__DARWIN_UL_COMPARE_AND_WAIT 1u
#define BR__DARWIN_ULF_WAKE_ALL 0x00000100u
#define BR__DARWIN_ULF_NO_ERRNO 0x01000000u

#if defined(__has_attribute)
#if __has_attribute(weak_import)
#define BR__DARWIN_WEAK_IMPORT __attribute__((weak_import))
#endif
#endif
#ifndef BR__DARWIN_WEAK_IMPORT
#define BR__DARWIN_WEAK_IMPORT
#endif

extern i32
os_sync_wait_on_address(void *addr, u64 value, usize size, u32 flags) BR__DARWIN_WEAK_IMPORT;
extern i32 os_sync_wait_on_address_with_timeout(void *addr,
                                                u64 value,
                                                usize size,
                                                u32 flags,
                                                u32 clock_id,
                                                u64 timeout_ns) BR__DARWIN_WEAK_IMPORT;
extern i32 os_sync_wake_by_address_any(void *addr, usize size, u32 flags) BR__DARWIN_WEAK_IMPORT;
extern i32 os_sync_wake_by_address_all(void *addr, usize size, u32 flags) BR__DARWIN_WEAK_IMPORT;
extern i32 __ulock_wait2(u32 operation, void *addr, u64 value, u64 timeout_ns, u64 value2)
  BR__DARWIN_WEAK_IMPORT;
extern i32
__ulock_wait(u32 operation, void *addr, u64 value, u32 timeout_us) BR__DARWIN_WEAK_IMPORT;
extern i32 __ulock_wake(u32 operation, void *addr, u64 wake_value) BR__DARWIN_WEAK_IMPORT;

static bool br__darwin_os_wait(br_futex *futex, u32 expected, bool *handled) {
  i32 rc;

  if (os_sync_wait_on_address == NULL) {
    *handled = false;
    return false;
  }

  *handled = true;
  rc = os_sync_wait_on_address(futex, (u64)expected, sizeof(*futex), BR__DARWIN_OS_SYNC_FLAGS);
  if (rc >= 0) {
    return true;
  }

  return errno == EINTR || errno == EFAULT;
}

static bool br__darwin_os_wait_with_timeout(br_futex *futex,
                                            u32 expected,
                                            br_duration duration,
                                            bool *handled) {
  i32 rc;

  if (os_sync_wait_on_address_with_timeout == NULL) {
    *handled = false;
    return false;
  }

  *handled = true;
  rc = os_sync_wait_on_address_with_timeout(futex,
                                            (u64)expected,
                                            sizeof(*futex),
                                            BR__DARWIN_OS_SYNC_FLAGS,
                                            BR__DARWIN_MACH_ABSOLUTE_TIME,
                                            (u64)duration);
  if (rc >= 0) {
    return true;
  }

  if (errno == ETIMEDOUT) {
    return false;
  }
  return errno == EINTR || errno == EFAULT;
}

static bool br__darwin_ulock_wait(br_futex *futex, u32 expected) {
  i32 rc;

  if (__ulock_wait2 != NULL) {
    rc = __ulock_wait2(
      BR__DARWIN_UL_COMPARE_AND_WAIT | BR__DARWIN_ULF_NO_ERRNO, futex, (u64)expected, 0u, 0u);
  } else {
    if (__ulock_wait == NULL) {
      return false;
    }
    rc = __ulock_wait(
      BR__DARWIN_UL_COMPARE_AND_WAIT | BR__DARWIN_ULF_NO_ERRNO, futex, (u64)expected, 0u);
  }

  if (rc >= 0) {
    return true;
  }

  return rc == -EINTR || rc == -EFAULT;
}

static bool
br__darwin_ulock_wait_with_timeout(br_futex *futex, u32 expected, br_duration duration) {
  i32 rc;

  if (__ulock_wait2 != NULL) {
    rc = __ulock_wait2(BR__DARWIN_UL_COMPARE_AND_WAIT | BR__DARWIN_ULF_NO_ERRNO,
                       futex,
                       (u64)expected,
                       (u64)duration,
                       0u);
  } else {
    br_duration timeout_us_value;
    u32 timeout_us;

    if (__ulock_wait == NULL) {
      return false;
    }

    /*
    Odin truncates nanoseconds to microseconds for the legacy __ulock_wait path.
    Bedrock rounds tiny positive durations up so a timed wait cannot accidentally
    become the no-timeout `0` form on older Darwin targets.
    */
    timeout_us_value = duration / BR_MICROSECOND;
    if (timeout_us_value <= 0) {
      timeout_us = 1u;
    } else if (timeout_us_value > (br_duration)UINT32_MAX) {
      timeout_us = UINT32_MAX;
    } else {
      timeout_us = (u32)timeout_us_value;
    }
    rc = __ulock_wait(
      BR__DARWIN_UL_COMPARE_AND_WAIT | BR__DARWIN_ULF_NO_ERRNO, futex, (u64)expected, timeout_us);
  }

  if (rc >= 0) {
    return true;
  }

  if (rc == -ETIMEDOUT) {
    return false;
  }
  return rc == -EINTR || rc == -EFAULT;
}

static bool br__darwin_os_wake(br_futex *futex, bool broadcast, bool *handled) {
  i32 rc;

  if ((!broadcast && os_sync_wake_by_address_any == NULL) ||
      (broadcast && os_sync_wake_by_address_all == NULL)) {
    *handled = false;
    return false;
  }

  *handled = true;
  do {
    if (broadcast) {
      rc = os_sync_wake_by_address_all(futex, sizeof(*futex), BR__DARWIN_OS_SYNC_FLAGS);
    } else {
      rc = os_sync_wake_by_address_any(futex, sizeof(*futex), BR__DARWIN_OS_SYNC_FLAGS);
    }
    if (rc >= 0 || errno == ENOENT) {
      return true;
    }
  } while (errno == EINTR || errno == EFAULT);

  return false;
}

static void br__darwin_ulock_wake(br_futex *futex, bool broadcast) {
  u32 operation;
  i32 rc;

  if (__ulock_wake == NULL) {
    return;
  }

  operation = BR__DARWIN_UL_COMPARE_AND_WAIT | BR__DARWIN_ULF_NO_ERRNO;
  if (broadcast) {
    operation |= BR__DARWIN_ULF_WAKE_ALL;
  }

  do {
    rc = __ulock_wake(operation, futex, 0u);
  } while (rc == -EINTR || rc == -EFAULT);
}

bool br_futex_wait(br_futex *futex, u32 expected) {
  bool handled;
  bool ok;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  /*
  Match Odin's preference order while avoiding hard minimum-OS or SDK debt:
  Apple's newer os_sync_* APIs first, then __ulock_wait2, then __ulock_wait.
  Timeout support will use the parallel timeout symbols once Bedrock has time.
  */
  handled = false;
  ok = br__darwin_os_wait(futex, expected, &handled);
  if (handled) {
    return ok;
  }

  return br__darwin_ulock_wait(futex, expected);
}

bool br_futex_wait_with_timeout(br_futex *futex, u32 expected, br_duration duration) {
  bool handled;
  bool ok;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }
  if (duration <= 0) {
    return false;
  }

  handled = false;
  ok = br__darwin_os_wait_with_timeout(futex, expected, duration, &handled);
  if (handled) {
    return ok;
  }

  return br__darwin_ulock_wait_with_timeout(futex, expected, duration);
}

void br_futex_signal(br_futex *futex) {
  bool handled;

  if (futex == NULL) {
    return;
  }

  handled = false;
  if (br__darwin_os_wake(futex, false, &handled) || handled) {
    return;
  }

  br__darwin_ulock_wake(futex, false);
}

void br_futex_broadcast(br_futex *futex) {
  bool handled;

  if (futex == NULL) {
    return;
  }

  handled = false;
  if (br__darwin_os_wake(futex, true, &handled) || handled) {
    return;
  }

  br__darwin_ulock_wake(futex, true);
}

#else
typedef u8 br__sync_futex_darwin_translation_unit;
#endif

/* ==== src/sync/futex_freebsd.c ==== */

#if defined(__FreeBSD__)


#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/umtx.h>
#include <time.h>

static bool br__futex_duration_to_timespec(br_duration duration, struct timespec *timeout) {
  if (duration <= 0 || timeout == NULL) {
    return false;
  }

  timeout->tv_sec = (time_t)(duration / BR_SECOND);
  timeout->tv_nsec = (long)(duration % BR_SECOND);
  return true;
}

bool br_futex_wait(br_futex *futex, u32 expected) {
  int rc;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  /*
  Odin simulates an infinite wait with a 4-hour timeout loop. Bedrock uses
  FreeBSD's native no-timeout form: the _umtx_op timeout arguments are optional,
  and timeout behavior is only requested when uaddr/uaddr2 describe a timespec.
  */
  rc = _umtx_op(futex, UMTX_OP_WAIT_UINT, (unsigned long)expected, NULL, NULL);
  if (rc == 0) {
    return true;
  }

  return errno == EINTR || errno == EAGAIN || errno == EBUSY;
}

bool br_futex_wait_with_timeout(br_futex *futex, u32 expected, br_duration duration) {
  struct timespec timeout;
  void *timeout_size;
  int rc;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }
  if (!br__futex_duration_to_timespec(duration, &timeout)) {
    return false;
  }

  timeout_size = (void *)(uptr)sizeof(timeout);
  rc = _umtx_op(futex, UMTX_OP_WAIT_UINT, (unsigned long)expected, timeout_size, &timeout);
  if (rc == 0) {
    return true;
  }

  if (errno == ETIMEDOUT) {
    return false;
  }
  return errno == EINTR || errno == EAGAIN || errno == EBUSY;
}

void br_futex_signal(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  BR_UNUSED(_umtx_op(futex, UMTX_OP_WAKE, 1ul, NULL, NULL));
}

void br_futex_broadcast(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  BR_UNUSED(_umtx_op(futex, UMTX_OP_WAKE, (unsigned long)INT_MAX, NULL, NULL));
}

#else
typedef u8 br__sync_futex_freebsd_translation_unit;
#endif

/* ==== src/sync/futex_linux.c ==== */


#if defined(__linux__)


#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

static long br__futex_syscall(br_futex *futex, i32 op, u32 value, const struct timespec *timeout) {
  return syscall(SYS_futex, (u32 *)futex, op, value, timeout, NULL, 0);
}

static bool br__futex_duration_to_timespec(br_duration duration, struct timespec *ts) {
  if (duration <= 0 || ts == NULL) {
    return false;
  }

  ts->tv_sec = (time_t)(duration / BR_SECOND);
  ts->tv_nsec = (long)(duration % BR_SECOND);
  return true;
}

bool br_futex_wait(br_futex *futex, u32 expected) {
  long rc;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  rc = br__futex_syscall(futex, FUTEX_WAIT_PRIVATE, expected, NULL);
  if (rc == 0) {
    return true;
  }

  /*
  Odin treats interrupted waits and value mismatches as normal wake paths.
  Bedrock reports only real backend failures as `false`.
  */
  return errno == EINTR || errno == EAGAIN;
}

bool br_futex_wait_with_timeout(br_futex *futex, u32 expected, br_duration duration) {
  struct timespec timeout;
  long rc;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }
  if (!br__futex_duration_to_timespec(duration, &timeout)) {
    return false;
  }

  rc = br__futex_syscall(futex, FUTEX_WAIT_PRIVATE, expected, &timeout);
  if (rc == 0) {
    return true;
  }

  if (errno == ETIMEDOUT) {
    return false;
  }
  return errno == EINTR || errno == EAGAIN;
}

void br_futex_signal(br_futex *futex) {
  if (futex == NULL) {
    return;
  }

  BR_UNUSED(br__futex_syscall(futex, FUTEX_WAKE_PRIVATE, 1u, NULL));
}

void br_futex_broadcast(br_futex *futex) {
  if (futex == NULL) {
    return;
  }

  BR_UNUSED(br__futex_syscall(futex, FUTEX_WAKE_PRIVATE, (u32)INT_MAX, NULL));
}

#else
typedef u8 br__sync_futex_linux_translation_unit;
#endif

/* ==== src/sync/futex_netbsd.c ==== */

#if defined(__NetBSD__)


#include <errno.h>
#include <limits.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define BR__NETBSD_FUTEX_PRIVATE_FLAG 128
#define BR__NETBSD_FUTEX_WAIT_PRIVATE (0 | BR__NETBSD_FUTEX_PRIVATE_FLAG)
#define BR__NETBSD_FUTEX_WAKE_PRIVATE (1 | BR__NETBSD_FUTEX_PRIVATE_FLAG)

static bool br__futex_duration_to_timespec(br_duration duration, struct timespec *timeout) {
  if (duration <= 0 || timeout == NULL) {
    return false;
  }

  timeout->tv_sec = (time_t)(duration / BR_SECOND);
  timeout->tv_nsec = (long)(duration % BR_SECOND);
  return true;
}

bool br_futex_wait(br_futex *futex, u32 expected) {
  long rc;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  rc = syscall(SYS___futex, futex, BR__NETBSD_FUTEX_WAIT_PRIVATE, expected, NULL, NULL, 0);
  if (rc == 0) {
    return true;
  }

  return errno == EINTR || errno == EAGAIN;
}

bool br_futex_wait_with_timeout(br_futex *futex, u32 expected, br_duration duration) {
  struct timespec timeout;
  long rc;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }
  if (!br__futex_duration_to_timespec(duration, &timeout)) {
    return false;
  }

  rc = syscall(SYS___futex, futex, BR__NETBSD_FUTEX_WAIT_PRIVATE, expected, &timeout, NULL, 0);
  if (rc == 0) {
    return true;
  }

  if (errno == ETIMEDOUT) {
    return false;
  }
  return errno == EINTR || errno == EAGAIN;
}

void br_futex_signal(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  BR_UNUSED(syscall(SYS___futex, futex, BR__NETBSD_FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0));
}

void br_futex_broadcast(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  BR_UNUSED(syscall(SYS___futex, futex, BR__NETBSD_FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0));
}

#else
typedef u8 br__sync_futex_netbsd_translation_unit;
#endif

/* ==== src/sync/futex_openbsd.c ==== */

#if defined(__OpenBSD__)


#include <errno.h>
#include <limits.h>
#include <time.h>

#define BR__OPENBSD_FUTEX_WAIT 1
#define BR__OPENBSD_FUTEX_WAKE 2
#define BR__OPENBSD_FUTEX_PRIVATE_FLAG 128
#define BR__OPENBSD_FUTEX_WAIT_PRIVATE (BR__OPENBSD_FUTEX_WAIT | BR__OPENBSD_FUTEX_PRIVATE_FLAG)
#define BR__OPENBSD_FUTEX_WAKE_PRIVATE (BR__OPENBSD_FUTEX_WAKE | BR__OPENBSD_FUTEX_PRIVATE_FLAG)

extern int futex(br_futex *futex, int op, u32 value, const void *timeout);

static bool br__futex_duration_to_timespec(br_duration duration, struct timespec *timeout) {
  if (duration <= 0 || timeout == NULL) {
    return false;
  }

  timeout->tv_sec = (time_t)(duration / BR_SECOND);
  timeout->tv_nsec = (long)(duration % BR_SECOND);
  return true;
}

bool br_futex_wait(br_futex *futex_word, u32 expected) {
  int rc;

  if (futex_word == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex_word, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  rc = futex(futex_word, BR__OPENBSD_FUTEX_WAIT_PRIVATE, expected, NULL);
  if (rc != -1) {
    return true;
  }

  return errno == EINTR || errno == EAGAIN || errno == ETIMEDOUT;
}

bool br_futex_wait_with_timeout(br_futex *futex_word, u32 expected, br_duration duration) {
  struct timespec timeout;
  int rc;

  if (futex_word == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex_word, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }
  if (!br__futex_duration_to_timespec(duration, &timeout)) {
    return false;
  }

  rc = futex(futex_word, BR__OPENBSD_FUTEX_WAIT_PRIVATE, expected, &timeout);
  if (rc != -1) {
    return true;
  }

  if (errno == ETIMEDOUT) {
    return false;
  }
  return errno == EINTR || errno == EAGAIN;
}

void br_futex_signal(br_futex *futex_word) {
  if (futex_word == NULL) {
    return;
  }
  BR_UNUSED(futex(futex_word, BR__OPENBSD_FUTEX_WAKE_PRIVATE, 1u, NULL));
}

void br_futex_broadcast(br_futex *futex_word) {
  if (futex_word == NULL) {
    return;
  }
  BR_UNUSED(futex(futex_word, BR__OPENBSD_FUTEX_WAKE_PRIVATE, (u32)INT_MAX, NULL));
}

#else
typedef u8 br__sync_futex_openbsd_translation_unit;
#endif

/* ==== src/sync/futex_other.c ==== */

#if !defined(__linux__) && !defined(_WIN32) && !(defined(__APPLE__) && defined(__MACH__)) &&       \
  !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)

#error "Bedrock sync requires a futex backend for this target."

#else
typedef u8 br__sync_futex_other_translation_unit;
#endif

/* ==== src/sync/futex_windows.c ==== */

#if defined(_WIN32)



#include <windows.h>

typedef LONG(WINAPI *br__rtl_wait_on_address_fn)(volatile void *address,
                                                 void *compare_address,
                                                 SIZE_T address_size,
                                                 const LARGE_INTEGER *timeout);
typedef ULONG(WINAPI *br__rtl_nt_status_to_dos_error_fn)(LONG status);
typedef VOID(WINAPI *br__wake_by_address_fn)(void *address);

static bool br__windows_symbol(const char *module_name, const char *name, FARPROC *out_symbol) {
  HMODULE module;

  if (out_symbol == NULL) {
    return false;
  }

  module = GetModuleHandleA(module_name);
  if (module == NULL) {
    return false;
  }

  *out_symbol = GetProcAddress(module, name);
  return *out_symbol != NULL;
}

static bool br__windows_ntdll_symbol(const char *name, FARPROC *out_symbol) {
  return br__windows_symbol("ntdll.dll", name, out_symbol);
}

static bool br__windows_sync_symbol(const char *name, FARPROC *out_symbol) {
  if (br__windows_symbol("api-ms-win-core-synch-l1-2-0.dll", name, out_symbol)) {
    return true;
  }
  if (br__windows_symbol("KernelBase.dll", name, out_symbol)) {
    return true;
  }
  return br__windows_symbol("kernel32.dll", name, out_symbol);
}

static bool br__windows_duration_to_timeout(br_duration duration, LARGE_INTEGER *timeout) {
  if (duration <= 0 || timeout == NULL) {
    return false;
  }

  /*
  RtlWaitOnAddress expects relative timeouts as negative 100ns intervals, which
  matches Odin's CustomWaitOnAddress path.
  */
  timeout->QuadPart = -(LONGLONG)(duration / 100);
  return true;
}

static bool br__windows_rtl_wait_on_address(br_futex *futex,
                                            u32 *compare,
                                            SIZE_T address_size,
                                            const LARGE_INTEGER *timeout) {
  FARPROC wait_symbol;
  FARPROC status_symbol;
  br__rtl_wait_on_address_fn wait_on_address = NULL;
  br__rtl_nt_status_to_dos_error_fn nt_status_to_dos_error = NULL;
  LONG status;

  if (!br__windows_ntdll_symbol("RtlWaitOnAddress", &wait_symbol)) {
    return false;
  }
  if (!br__windows_ntdll_symbol("RtlNtStatusToDosError", &status_symbol)) {
    return false;
  }

  BR_STATIC_ASSERT(sizeof(wait_on_address) == sizeof(wait_symbol), "unexpected FARPROC size");
  BR_STATIC_ASSERT(sizeof(nt_status_to_dos_error) == sizeof(status_symbol),
                   "unexpected FARPROC size");
  memcpy(&wait_on_address, &wait_symbol, sizeof(wait_on_address));
  memcpy(&nt_status_to_dos_error, &status_symbol, sizeof(nt_status_to_dos_error));

  status = wait_on_address((volatile void *)futex, compare, address_size, timeout);
  if (status != 0) {
    SetLastError(nt_status_to_dos_error(status));
  }

  return status == 0;
}

static void br__windows_wake_by_address(br_futex *futex, bool broadcast) {
  FARPROC wake_symbol;
  br__wake_by_address_fn wake_by_address = NULL;

  if (!br__windows_sync_symbol(broadcast ? "WakeByAddressAll" : "WakeByAddressSingle",
                               &wake_symbol)) {
    return;
  }

  BR_STATIC_ASSERT(sizeof(wake_by_address) == sizeof(wake_symbol), "unexpected FARPROC size");
  memcpy(&wake_by_address, &wake_symbol, sizeof(wake_by_address));
  wake_by_address((void *)futex);
}

bool br_futex_wait(br_futex *futex, u32 expected) {
  u32 compare;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  compare = expected;
  /*
  Odin avoids public WaitOnAddress because its wrapper must translate NTSTATUS
  into GetLastError-compatible codes. Bedrock keeps that behavior and resolves
  all wait/wake symbols at runtime so users do not need Windows sync import
  libraries when linking a static Bedrock build.
  */
  return br__windows_rtl_wait_on_address(futex, &compare, sizeof(compare), NULL);
}

bool br_futex_wait_with_timeout(br_futex *futex, u32 expected, br_duration duration) {
  LARGE_INTEGER timeout;
  u32 compare;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }
  if (!br__windows_duration_to_timeout(duration, &timeout)) {
    return false;
  }

  compare = expected;
  return br__windows_rtl_wait_on_address(futex, &compare, sizeof(compare), &timeout);
}

void br_futex_signal(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  br__windows_wake_by_address(futex, false);
}

void br_futex_broadcast(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  br__windows_wake_by_address(futex, true);
}

#else
typedef u8 br__sync_futex_windows_translation_unit;
#endif

/* ==== src/sync/primitives.c ==== */


#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__APPLE__) && defined(__MACH__)
#include <pthread.h>
#elif defined(__FreeBSD__)
#include <pthread_np.h>
#elif defined(__NetBSD__)
#include <lwp.h>
#elif defined(__OpenBSD__)
#include <unistd.h>
#elif defined(__HAIKU__)
#include <OS.h>
#endif

br_thread_id br_current_thread_id(void) {
#if defined(_WIN32)
  return (br_thread_id)GetCurrentThreadId();
#elif defined(__linux__)
  long tid = syscall(SYS_gettid);
  return tid > 0 ? (br_thread_id)tid : BR_THREAD_ID_INVALID;
#elif defined(__APPLE__) && defined(__MACH__)
  u64 tid = 0;
  if (pthread_threadid_np(NULL, &tid) != 0) {
    return BR_THREAD_ID_INVALID;
  }
  return (br_thread_id)tid;
#elif defined(__FreeBSD__)
  return (br_thread_id)pthread_getthreadid_np();
#elif defined(__NetBSD__)
  return (br_thread_id)_lwp_self();
#elif defined(__OpenBSD__)
  return (br_thread_id)getthrid();
#elif defined(__HAIKU__)
  return (br_thread_id)find_thread(NULL);
#else
  return BR_THREAD_ID_INVALID;
#endif
}

/* ==== src/sync/primitives_atomic.c ==== */
#include <assert.h>


static u32 br__atomic_mutex_spin(br_atomic_mutex *mutex) {
  for (usize spin = 100u; spin > 0u; --spin) {
    /*
    Rust's futex mutex only loads while spinning so contenders do not keep
    bouncing the cache line with CAS/exchange attempts.
    */
    u32 state = br_atomic_load_explicit(&mutex->state, BR_ATOMIC_RELAXED);

    if (state != BR_ATOMIC_MUTEX_LOCKED) {
      return state;
    }

    br_cpu_relax();
  }

  return br_atomic_load_explicit(&mutex->state, BR_ATOMIC_RELAXED);
}

static void br__atomic_mutex_lock_slow(br_atomic_mutex *mutex) {
  u32 state = br__atomic_mutex_spin(mutex);

  if (state == BR_ATOMIC_MUTEX_UNLOCKED) {
    u32 expected = BR_ATOMIC_MUTEX_UNLOCKED;
    if (br_atomic_compare_exchange_strong_explicit(
          &mutex->state, &expected, BR_ATOMIC_MUTEX_LOCKED, BR_ATOMIC_ACQUIRE, BR_ATOMIC_RELAXED)) {
      return;
    }
    state = expected;
  }

  for (;;) {
    if (state != BR_ATOMIC_MUTEX_WAITING &&
        br_atomic_exchange_explicit(&mutex->state, BR_ATOMIC_MUTEX_WAITING, BR_ATOMIC_ACQUIRE) ==
          BR_ATOMIC_MUTEX_UNLOCKED) {
      return;
    }

    BR_UNUSED(br_futex_wait(&mutex->state, BR_ATOMIC_MUTEX_WAITING));
    state = br__atomic_mutex_spin(mutex);
  }
}

void br_atomic_mutex_init(br_atomic_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }

  br_atomic_init(&mutex->state, BR_ATOMIC_MUTEX_UNLOCKED);
}

void br_atomic_mutex_lock(br_atomic_mutex *mutex) {
  u32 expected;

  if (mutex == NULL) {
    return;
  }

  expected = BR_ATOMIC_MUTEX_UNLOCKED;
  if (!br_atomic_compare_exchange_strong_explicit(
        &mutex->state, &expected, BR_ATOMIC_MUTEX_LOCKED, BR_ATOMIC_ACQUIRE, BR_ATOMIC_RELAXED)) {
    br__atomic_mutex_lock_slow(mutex);
  }
}

void br_atomic_mutex_unlock(br_atomic_mutex *mutex) {
  u32 state;

  if (mutex == NULL) {
    return;
  }

  state = br_atomic_exchange_explicit(&mutex->state, BR_ATOMIC_MUTEX_UNLOCKED, BR_ATOMIC_RELEASE);
  if (state == BR_ATOMIC_MUTEX_WAITING) {
    br_futex_signal(&mutex->state);
  }
}

bool br_atomic_mutex_try_lock(br_atomic_mutex *mutex) {
  u32 expected = BR_ATOMIC_MUTEX_UNLOCKED;

  if (mutex == NULL) {
    return false;
  }

  return br_atomic_compare_exchange_strong_explicit(
    &mutex->state, &expected, BR_ATOMIC_MUTEX_LOCKED, BR_ATOMIC_ACQUIRE, BR_ATOMIC_RELAXED);
}

void br_atomic_rw_mutex_init(br_atomic_rw_mutex *rw) {
  if (rw == NULL) {
    return;
  }

  br_atomic_init(&rw->state, 0);
  br_atomic_mutex_init(&rw->mutex);
  br_atomic_sema_init(&rw->sema, 0u);
}

void br_atomic_rw_mutex_lock(br_atomic_rw_mutex *rw) {
  usize state;

  if (rw == NULL) {
    return;
  }

  br_atomic_mutex_lock(&rw->mutex);

  state = br_atomic_or(&rw->state, BR_ATOMIC_RW_MUTEX_STATE_IS_WRITING);
  if ((state & BR_ATOMIC_RW_MUTEX_STATE_READER_MASK) != 0u) {
    /*
    Mirrors Odin: the exclusive mutex prevents new readers while the writer
    waits for the last active reader to post the semaphore.
    */
    br_atomic_sema_wait(&rw->sema);
  }
}

void br_atomic_rw_mutex_unlock(br_atomic_rw_mutex *rw) {
  if (rw == NULL) {
    return;
  }

  BR_UNUSED(br_atomic_and(&rw->state, ~BR_ATOMIC_RW_MUTEX_STATE_IS_WRITING));
  br_atomic_mutex_unlock(&rw->mutex);
}

bool br_atomic_rw_mutex_try_lock(br_atomic_rw_mutex *rw) {
  usize state;

  if (rw == NULL) {
    return false;
  }

  if (br_atomic_mutex_try_lock(&rw->mutex)) {
    state = br_atomic_load(&rw->state);
    if ((state & BR_ATOMIC_RW_MUTEX_STATE_READER_MASK) == 0u) {
      usize expected = state;
      if (br_atomic_compare_exchange_strong(
            &rw->state, &expected, state | BR_ATOMIC_RW_MUTEX_STATE_IS_WRITING)) {
        return true;
      }
    }

    br_atomic_mutex_unlock(&rw->mutex);
  }

  return false;
}

void br_atomic_rw_mutex_shared_lock(br_atomic_rw_mutex *rw) {
  usize state;

  if (rw == NULL) {
    return;
  }

  state = br_atomic_load(&rw->state);
  while ((state & BR_ATOMIC_RW_MUTEX_STATE_IS_WRITING) == 0u) {
    usize expected = state;
    if (br_atomic_compare_exchange_weak(
          &rw->state, &expected, state + BR_ATOMIC_RW_MUTEX_STATE_READER)) {
      return;
    }
    state = expected;
  }

  br_atomic_mutex_lock(&rw->mutex);
  BR_UNUSED(br_atomic_add(&rw->state, BR_ATOMIC_RW_MUTEX_STATE_READER));
  br_atomic_mutex_unlock(&rw->mutex);
}

void br_atomic_rw_mutex_shared_unlock(br_atomic_rw_mutex *rw) {
  usize state;

  if (rw == NULL) {
    return;
  }

  state = br_atomic_sub(&rw->state, BR_ATOMIC_RW_MUTEX_STATE_READER);
  if ((state & BR_ATOMIC_RW_MUTEX_STATE_READER_MASK) == BR_ATOMIC_RW_MUTEX_STATE_READER &&
      (state & BR_ATOMIC_RW_MUTEX_STATE_IS_WRITING) != 0u) {
    br_atomic_sema_post(&rw->sema, 1u);
  }
}

bool br_atomic_rw_mutex_try_shared_lock(br_atomic_rw_mutex *rw) {
  usize state;

  if (rw == NULL) {
    return false;
  }

  state = br_atomic_load(&rw->state);
  while ((state & BR_ATOMIC_RW_MUTEX_STATE_IS_WRITING) == 0u) {
    usize expected = state;
    if (br_atomic_compare_exchange_weak(
          &rw->state, &expected, state + BR_ATOMIC_RW_MUTEX_STATE_READER)) {
      return true;
    }
    state = expected;
  }

  if (br_atomic_mutex_try_lock(&rw->mutex)) {
    BR_UNUSED(br_atomic_add(&rw->state, BR_ATOMIC_RW_MUTEX_STATE_READER));
    br_atomic_mutex_unlock(&rw->mutex);
    return true;
  }

  return false;
}

void br_atomic_recursive_mutex_init(br_atomic_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }

  br_atomic_init(&mutex->owner, BR_THREAD_ID_INVALID);
  mutex->recursion = 0u;
  br_atomic_mutex_init(&mutex->mutex);
}

void br_atomic_recursive_mutex_lock(br_atomic_recursive_mutex *mutex) {
  br_thread_id tid;
  br_thread_id owner;

  if (mutex == NULL) {
    return;
  }

  tid = br_current_thread_id();
  owner = br_atomic_load_explicit(&mutex->owner, BR_ATOMIC_ACQUIRE);
  if (tid != owner) {
    br_atomic_mutex_lock(&mutex->mutex);
    br_atomic_store_explicit(&mutex->owner, tid, BR_ATOMIC_RELEASE);
  }

  mutex->recursion += 1u;
}

void br_atomic_recursive_mutex_unlock(br_atomic_recursive_mutex *mutex) {
  br_thread_id tid;

  if (mutex == NULL) {
    return;
  }

  tid = br_current_thread_id();
  if (tid != br_atomic_load_explicit(&mutex->owner, BR_ATOMIC_RELAXED)) {
    assert(false);
    return;
  }

  mutex->recursion -= 1u;
  if (mutex->recursion == 0u) {
    br_atomic_store_explicit(&mutex->owner, BR_THREAD_ID_INVALID, BR_ATOMIC_RELEASE);
    br_atomic_mutex_unlock(&mutex->mutex);
  }
}

bool br_atomic_recursive_mutex_try_lock(br_atomic_recursive_mutex *mutex) {
  br_thread_id tid;

  if (mutex == NULL) {
    return false;
  }

  tid = br_current_thread_id();
  if (tid == br_atomic_load_explicit(&mutex->owner, BR_ATOMIC_ACQUIRE)) {
    /*
    Odin's `Atomic_Recursive_Mutex` currently calls mutex_try_lock here, which
    makes recursive try-lock fail against the already-held non-recursive mutex.
    Bedrock follows the documented recursive mutex behavior instead.
    */
    mutex->recursion += 1u;
    return true;
  }

  if (!br_atomic_mutex_try_lock(&mutex->mutex)) {
    return false;
  }

  br_atomic_store_explicit(&mutex->owner, tid, BR_ATOMIC_RELEASE);
  mutex->recursion += 1u;
  return true;
}

void br_atomic_cond_init(br_atomic_cond *cond) {
  if (cond == NULL) {
    return;
  }

  br_atomic_init(&cond->state, 0u);
}

bool br_atomic_cond_wait(br_atomic_cond *cond, br_atomic_mutex *mutex) {
  u32 state;
  bool ok;

  if (cond == NULL || mutex == NULL) {
    return false;
  }

  state = br_atomic_load_explicit(&cond->state, BR_ATOMIC_RELAXED);
  br_atomic_mutex_unlock(mutex);
  ok = br_futex_wait(&cond->state, state);
  br_atomic_mutex_lock(mutex);
  return ok;
}

bool br_atomic_cond_wait_with_timeout(br_atomic_cond *cond,
                                      br_atomic_mutex *mutex,
                                      br_duration duration) {
  u32 state;
  bool ok;

  if (cond == NULL || mutex == NULL) {
    return false;
  }

  state = br_atomic_load_explicit(&cond->state, BR_ATOMIC_RELAXED);
  br_atomic_mutex_unlock(mutex);
  ok = br_futex_wait_with_timeout(&cond->state, state, duration);
  br_atomic_mutex_lock(mutex);
  return ok;
}

void br_atomic_cond_signal(br_atomic_cond *cond) {
  if (cond == NULL) {
    return;
  }

  BR_UNUSED(br_atomic_add_explicit(&cond->state, 1u, BR_ATOMIC_RELEASE));
  br_futex_signal(&cond->state);
}

void br_atomic_cond_broadcast(br_atomic_cond *cond) {
  if (cond == NULL) {
    return;
  }

  BR_UNUSED(br_atomic_add_explicit(&cond->state, 1u, BR_ATOMIC_RELEASE));
  br_futex_broadcast(&cond->state);
}

void br_atomic_sema_init(br_atomic_sema *sema, u32 count) {
  if (sema == NULL) {
    return;
  }

  br_atomic_init(&sema->count, count);
}

void br_atomic_sema_post(br_atomic_sema *sema, u32 count) {
  if (sema == NULL) {
    return;
  }

  br_atomic_add_explicit(&sema->count, count, BR_ATOMIC_RELEASE);
  if (count == 1u) {
    br_futex_signal(&sema->count);
  } else {
    br_futex_broadcast(&sema->count);
  }
}

void br_atomic_sema_wait(br_atomic_sema *sema) {
  u32 original_count;

  if (sema == NULL) {
    return;
  }

  for (;;) {
    original_count = br_atomic_load_explicit(&sema->count, BR_ATOMIC_RELAXED);
    while (original_count == 0u) {
      BR_UNUSED(br_futex_wait(&sema->count, original_count));
      br_cpu_relax();
      original_count = br_atomic_load_explicit(&sema->count, BR_ATOMIC_RELAXED);
    }

    if (br_atomic_compare_exchange_strong_explicit(&sema->count,
                                                   &original_count,
                                                   original_count - 1u,
                                                   BR_ATOMIC_ACQUIRE,
                                                   BR_ATOMIC_ACQUIRE)) {
      return;
    }
  }
}

bool br_atomic_sema_wait_with_timeout(br_atomic_sema *sema, br_duration duration) {
  u32 original_count;

  if (sema == NULL || duration <= 0) {
    return false;
  }

  for (;;) {
    original_count = br_atomic_load_explicit(&sema->count, BR_ATOMIC_RELAXED);
    for (br_tick start = br_tick_now(); original_count == 0u;) {
      br_duration remaining = duration - br_tick_since(start);
      if (remaining < 0) {
        return false;
      }

      if (!br_futex_wait_with_timeout(&sema->count, original_count, remaining)) {
        return false;
      }
      br_cpu_relax();
      original_count = br_atomic_load_explicit(&sema->count, BR_ATOMIC_RELAXED);
    }

    if (br_atomic_compare_exchange_strong_explicit(&sema->count,
                                                   &original_count,
                                                   original_count - 1u,
                                                   BR_ATOMIC_ACQUIRE,
                                                   BR_ATOMIC_ACQUIRE)) {
      return true;
    }
  }
}

/* ==== src/sync/primitives_internal.c ==== */

br_status br_mutex_init(br_mutex *mutex) {
  if (mutex == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  br_atomic_mutex_init(&mutex->impl);
  return BR_STATUS_OK;
}

void br_mutex_destroy(br_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_mutex_lock(br_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_mutex_lock(&mutex->impl);
}

void br_mutex_unlock(br_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_mutex_unlock(&mutex->impl);
}

bool br_mutex_try_lock(br_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return br_atomic_mutex_try_lock(&mutex->impl);
}

br_status br_rw_mutex_init(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  br_atomic_rw_mutex_init(&mutex->impl);
  return BR_STATUS_OK;
}

void br_rw_mutex_destroy(br_rw_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_rw_mutex_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_rw_mutex_lock(&mutex->impl);
}

void br_rw_mutex_unlock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_rw_mutex_unlock(&mutex->impl);
}

bool br_rw_mutex_try_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return br_atomic_rw_mutex_try_lock(&mutex->impl);
}

void br_rw_mutex_shared_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_rw_mutex_shared_lock(&mutex->impl);
}

void br_rw_mutex_shared_unlock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_rw_mutex_shared_unlock(&mutex->impl);
}

bool br_rw_mutex_try_shared_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return br_atomic_rw_mutex_try_shared_lock(&mutex->impl);
}

br_status br_recursive_mutex_init(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  br_atomic_recursive_mutex_init(&mutex->impl);
  return BR_STATUS_OK;
}

void br_recursive_mutex_destroy(br_recursive_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_recursive_mutex_lock(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_recursive_mutex_lock(&mutex->impl);
}

void br_recursive_mutex_unlock(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_recursive_mutex_unlock(&mutex->impl);
}

bool br_recursive_mutex_try_lock(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return br_atomic_recursive_mutex_try_lock(&mutex->impl);
}

br_status br_cond_init(br_cond *cond) {
  if (cond == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  br_atomic_cond_init(&cond->impl);
  return BR_STATUS_OK;
}

void br_cond_destroy(br_cond *cond) {
  BR_UNUSED(cond);
}

void br_cond_wait(br_cond *cond, br_mutex *mutex) {
  if (cond == NULL || mutex == NULL) {
    return;
  }
  BR_UNUSED(br_atomic_cond_wait(&cond->impl, &mutex->impl));
}

bool br_cond_wait_with_timeout(br_cond *cond, br_mutex *mutex, br_duration duration) {
  if (cond == NULL || mutex == NULL || duration <= 0) {
    return false;
  }
  return br_atomic_cond_wait_with_timeout(&cond->impl, &mutex->impl, duration);
}

void br_cond_signal(br_cond *cond) {
  if (cond == NULL) {
    return;
  }
  br_atomic_cond_signal(&cond->impl);
}

void br_cond_broadcast(br_cond *cond) {
  if (cond == NULL) {
    return;
  }
  br_atomic_cond_broadcast(&cond->impl);
}

br_status br_sema_init(br_sema *sema, u32 count) {
  if (sema == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  br_atomic_sema_init(&sema->impl, count);
  return BR_STATUS_OK;
}

void br_sema_destroy(br_sema *sema) {
  BR_UNUSED(sema);
}

void br_sema_post(br_sema *sema, u32 count) {
  if (sema == NULL) {
    return;
  }
  br_atomic_sema_post(&sema->impl, count);
}

void br_sema_wait(br_sema *sema) {
  if (sema == NULL) {
    return;
  }
  br_atomic_sema_wait(&sema->impl);
}

bool br_sema_wait_with_timeout(br_sema *sema, br_duration duration) {
  if (sema == NULL) {
    return false;
  }
  return br_atomic_sema_wait_with_timeout(&sema->impl, duration);
}

/* ==== src/time/time.c ==== */

br_tick br_tick_add(br_tick tick, br_duration duration) {
  tick.nsec += duration;
  return tick;
}

br_duration br_tick_diff(br_tick start, br_tick end) {
  return end.nsec - start.nsec;
}

br_duration br_tick_since(br_tick start) {
  return br_tick_diff(start, br_tick_now());
}

br_duration br_tick_lap_time(br_tick *prev) {
  br_tick now;
  br_duration duration = 0;

  now = br_tick_now();
  if (prev->nsec != 0) {
    duration = br_tick_diff(*prev, now);
  }
  *prev = now;
  return duration;
}

br_duration br_time_diff(br_time start, br_time end) {
  return end.nsec - start.nsec;
}

br_duration br_time_since(br_time start) {
  return br_time_diff(start, br_time_now());
}

i64 br_duration_nanoseconds(br_duration duration) {
  return duration;
}

f64 br_duration_microseconds(br_duration duration) {
  return br_duration_seconds(duration) * 1e6;
}

f64 br_duration_milliseconds(br_duration duration) {
  return br_duration_seconds(duration) * 1e3;
}

f64 br_duration_seconds(br_duration duration) {
  br_duration sec = duration / BR_SECOND;
  br_duration nsec = duration % BR_SECOND;
  return (f64)sec + (f64)nsec / 1e9;
}

f64 br_duration_minutes(br_duration duration) {
  br_duration min = duration / BR_MINUTE;
  br_duration nsec = duration % BR_MINUTE;
  return (f64)min + (f64)nsec / (60.0 * 1e9);
}

f64 br_duration_hours(br_duration duration) {
  br_duration hour = duration / BR_HOUR;
  br_duration nsec = duration % BR_HOUR;
  return (f64)hour + (f64)nsec / (60.0 * 60.0 * 1e9);
}

/* ==== src/time/time_linux.c ==== */


#if defined(__linux__)

#include <errno.h>
#include <sched.h>
#include <time.h>

bool br_time_is_supported(void) {
  return true;
}

br_time br_time_now(void) {
  struct timespec ts;
  br_time result = {0};

  if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
    result.nsec = (i64)ts.tv_sec * BR_SECOND + (i64)ts.tv_nsec;
  }
  return result;
}

void br_sleep(br_duration duration) {
  struct timespec ts;

  if (duration <= 0) {
    return;
  }

  ts.tv_sec = (time_t)(duration / BR_SECOND);
  ts.tv_nsec = (long)(duration % BR_SECOND);
  while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
  }
}

br_tick br_tick_now(void) {
  struct timespec ts;
  br_tick result = {0};

  if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0) {
    result.nsec = (i64)ts.tv_sec * BR_SECOND + (i64)ts.tv_nsec;
  }
  return result;
}

void br_yield(void) {
  sched_yield();
}

#else
typedef u8 br__time_linux_translation_unit;
#endif

/* ==== src/time/time_other.c ==== */

#if !defined(__linux__) && !defined(_WIN32) && !(defined(__APPLE__) && defined(__MACH__)) &&       \
  !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)

bool br_time_is_supported(void) {
  return false;
}

br_time br_time_now(void) {
  br_time result = {0};
  return result;
}

void br_sleep(br_duration duration) {
  BR_UNUSED(duration);
}

br_tick br_tick_now(void) {
  br_tick result = {0};
  return result;
}

void br_yield(void) {}

#else
typedef u8 br__time_other_translation_unit;
#endif

/* ==== src/time/time_unix.c ==== */


#if (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__) || defined(__NetBSD__) ||    \
  defined(__OpenBSD__)

#include <errno.h>
#include <sched.h>
#include <time.h>

#if defined(__APPLE__) && defined(__MACH__)
#define BR__TIME_TICK_CLOCK 4
#else
#define BR__TIME_TICK_CLOCK CLOCK_MONOTONIC
#endif

bool br_time_is_supported(void) {
  return true;
}

br_time br_time_now(void) {
  struct timespec ts;
  br_time result = {0};

  if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
    result.nsec = (i64)ts.tv_sec * BR_SECOND + (i64)ts.tv_nsec;
  }
  return result;
}

void br_sleep(br_duration duration) {
  struct timespec ts;

  if (duration <= 0) {
    return;
  }

  ts.tv_sec = (time_t)(duration / BR_SECOND);
  ts.tv_nsec = (long)(duration % BR_SECOND);
  while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
  }
}

br_tick br_tick_now(void) {
  struct timespec ts;
  br_tick result = {0};

  if (clock_gettime(BR__TIME_TICK_CLOCK, &ts) == 0) {
    result.nsec = (i64)ts.tv_sec * BR_SECOND + (i64)ts.tv_nsec;
  }
  return result;
}

void br_yield(void) {
  sched_yield();
}

#else
typedef u8 br__time_unix_translation_unit;
#endif

/* ==== src/time/time_windows.c ==== */

#if defined(_WIN32)


/* GetSystemTimePreciseAsFileTime requires Windows 8 (0x0602). */

#include <windows.h>

#define BR__WINDOWS_EPOCH_OFFSET_100NS ((u64)116444736000000000ull)

static i64 br__time_mul_div_i64(i64 value, i64 numerator, i64 denominator) {
  i64 quotient = value / denominator;
  i64 remainder = value % denominator;
  return quotient * numerator + remainder * numerator / denominator;
}

bool br_time_is_supported(void) {
  return true;
}

br_time br_time_now(void) {
  FILETIME file_time;
  ULARGE_INTEGER ticks;
  br_time result;

  GetSystemTimePreciseAsFileTime(&file_time);
  ticks.LowPart = file_time.dwLowDateTime;
  ticks.HighPart = file_time.dwHighDateTime;

  result.nsec = (i64)((ticks.QuadPart - BR__WINDOWS_EPOCH_OFFSET_100NS) * 100u);
  return result;
}

void br_sleep(br_duration duration) {
  if (duration <= 0) {
    return;
  }

  Sleep((DWORD)(duration / BR_MILLISECOND));
}

br_tick br_tick_now(void) {
  LARGE_INTEGER frequency;
  LARGE_INTEGER now;
  br_tick result = {0};

  if (QueryPerformanceFrequency(&frequency) == 0) {
    return result;
  }
  if (QueryPerformanceCounter(&now) == 0) {
    return result;
  }

  result.nsec = br__time_mul_div_i64(now.QuadPart, BR_SECOND, frequency.QuadPart);
  return result;
}

void br_yield(void) {
  SwitchToThread();
}

#else
typedef u8 br__time_windows_translation_unit;
#endif

/* ==== src/unicode/utf8.c ==== */

#define BR__UTF8_LOCB ((u8)0x80)
#define BR__UTF8_HICB ((u8)0xbf)
#define BR__UTF8_MASKX ((u8)0x3f)
#define BR__UTF8_MASK2 ((u8)0x1f)
#define BR__UTF8_MASK3 ((u8)0x0f)
#define BR__UTF8_MASK4 ((u8)0x07)
#define BR__UTF8_SURROGATE_MIN ((br_rune)0xd800)
#define BR__UTF8_SURROGATE_MAX ((br_rune)0xdfff)

typedef struct br__utf8_lead_info {
  usize width;
  u8 lo;
  u8 hi;
  bool ascii;
  bool invalid;
} br__utf8_lead_info;

static br_utf8_decode_result br__utf8_decode_result(br_rune value, usize width) {
  br_utf8_decode_result result;

  result.value = value;
  result.width = width;
  return result;
}

static br_utf8_encode_result br__utf8_encode_result(u8 b0, u8 b1, u8 b2, u8 b3, usize len) {
  br_utf8_encode_result result;

  result.bytes[0] = b0;
  result.bytes[1] = b1;
  result.bytes[2] = b2;
  result.bytes[3] = b3;
  result.len = len;
  return result;
}

static br__utf8_lead_info br__utf8_classify_lead(u8 lead) {
  br__utf8_lead_info info;

  info.width = 1u;
  info.lo = BR__UTF8_LOCB;
  info.hi = BR__UTF8_HICB;
  info.ascii = false;
  info.invalid = false;

  if (lead <= 0x7fu) {
    info.ascii = true;
    return info;
  }
  if (lead < 0xc2u) {
    info.invalid = true;
    return info;
  }
  if (lead <= 0xdfu) {
    info.width = 2u;
    return info;
  }
  if (lead == 0xe0u) {
    info.width = 3u;
    info.lo = 0xa0u;
    return info;
  }
  if (lead <= 0xecu) {
    info.width = 3u;
    return info;
  }
  if (lead == 0xedu) {
    info.width = 3u;
    info.hi = 0x9fu;
    return info;
  }
  if (lead <= 0xefu) {
    info.width = 3u;
    return info;
  }
  if (lead == 0xf0u) {
    info.width = 4u;
    info.lo = 0x90u;
    return info;
  }
  if (lead <= 0xf3u) {
    info.width = 4u;
    return info;
  }
  if (lead == 0xf4u) {
    info.width = 4u;
    info.hi = 0x8fu;
    return info;
  }

  info.invalid = true;
  return info;
}

static bool br__utf8_is_continuation(u8 byte_value) {
  return byte_value >= BR__UTF8_LOCB && byte_value <= BR__UTF8_HICB;
}

br_utf8_encode_result br_utf8_encode(br_rune value) {
  u32 rune_value;

  if (!br_utf8_valid_rune(value)) {
    value = BR_RUNE_ERROR;
  }

  rune_value = (u32)value;
  if (rune_value <= 0x7fu) {
    return br__utf8_encode_result((u8)rune_value, 0u, 0u, 0u, 1u);
  }
  if (rune_value <= 0x7ffu) {
    return br__utf8_encode_result(
      (u8)(0xc0u | (rune_value >> 6)), (u8)(0x80u | (rune_value & BR__UTF8_MASKX)), 0u, 0u, 2u);
  }
  if (rune_value <= 0xffffu) {
    return br__utf8_encode_result((u8)(0xe0u | (rune_value >> 12)),
                                  (u8)(0x80u | ((rune_value >> 6) & BR__UTF8_MASKX)),
                                  (u8)(0x80u | (rune_value & BR__UTF8_MASKX)),
                                  0u,
                                  3u);
  }

  return br__utf8_encode_result((u8)(0xf0u | (rune_value >> 18)),
                                (u8)(0x80u | ((rune_value >> 12) & BR__UTF8_MASKX)),
                                (u8)(0x80u | ((rune_value >> 6) & BR__UTF8_MASKX)),
                                (u8)(0x80u | (rune_value & BR__UTF8_MASKX)),
                                4u);
}

br_utf8_decode_result br_utf8_decode(br_bytes_view s) {
  br__utf8_lead_info info;
  u8 b0;
  u8 b1;
  u8 b2;
  u8 b3;

  if (s.len == 0u) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 0u);
  }

  b0 = s.data[0];
  info = br__utf8_classify_lead(b0);
  if (info.ascii) {
    return br__utf8_decode_result((br_rune)b0, 1u);
  }
  if (info.invalid) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 1u);
  }
  if (s.len < info.width) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 1u);
  }

  b1 = s.data[1];
  if (b1 < info.lo || b1 > info.hi) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 1u);
  }
  if (info.width == 2u) {
    return br__utf8_decode_result(
      (br_rune)(((u32)(b0 & BR__UTF8_MASK2) << 6) | (u32)(b1 & BR__UTF8_MASKX)), 2u);
  }

  b2 = s.data[2];
  if (!br__utf8_is_continuation(b2)) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 1u);
  }
  if (info.width == 3u) {
    return br__utf8_decode_result((br_rune)(((u32)(b0 & BR__UTF8_MASK3) << 12) |
                                            ((u32)(b1 & BR__UTF8_MASKX) << 6) |
                                            (u32)(b2 & BR__UTF8_MASKX)),
                                  3u);
  }

  b3 = s.data[3];
  if (!br__utf8_is_continuation(b3)) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 1u);
  }

  return br__utf8_decode_result(
    (br_rune)(((u32)(b0 & BR__UTF8_MASK4) << 18) | ((u32)(b1 & BR__UTF8_MASKX) << 12) |
              ((u32)(b2 & BR__UTF8_MASKX) << 6) | (u32)(b3 & BR__UTF8_MASKX)),
    4u);
}

br_utf8_decode_result br_utf8_decode_last(br_bytes_view s) {
  usize end;
  isize start;
  isize limit;
  br_utf8_decode_result result;

  end = s.len;
  if (end == 0u) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 0u);
  }
  if (s.data[end - 1u] < (u8)BR_RUNE_SELF) {
    return br__utf8_decode_result((br_rune)s.data[end - 1u], 1u);
  }

  limit = end > (usize)BR_UTF8_MAX ? (isize)(end - (usize)BR_UTF8_MAX) : 0;
  start = (isize)end - 2;
  while (start >= limit) {
    if (br_utf8_rune_start(s.data[(usize)start])) {
      break;
    }
    start -= 1;
  }
  if (start < limit) {
    start = limit;
  }

  result = br_utf8_decode(br_bytes_view_make(s.data + (usize)start, end - (usize)start));
  if ((usize)start + result.width != end) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 1u);
  }
  return result;
}

bool br_utf8_valid_rune(br_rune value) {
  if (value < 0) {
    return false;
  }
  if (value >= BR__UTF8_SURROGATE_MIN && value <= BR__UTF8_SURROGATE_MAX) {
    return false;
  }
  if (value > BR_RUNE_MAX) {
    return false;
  }
  return true;
}

bool br_utf8_valid(br_bytes_view s) {
  usize i = 0u;

  while (i < s.len) {
    br__utf8_lead_info info;
    u8 b1;

    info = br__utf8_classify_lead(s.data[i]);
    if (info.ascii) {
      i += 1u;
      continue;
    }
    if (info.invalid || i + info.width > s.len) {
      return false;
    }

    b1 = s.data[i + 1u];
    if (b1 < info.lo || b1 > info.hi) {
      return false;
    }
    if (info.width > 2u && !br__utf8_is_continuation(s.data[i + 2u])) {
      return false;
    }
    if (info.width > 3u && !br__utf8_is_continuation(s.data[i + 3u])) {
      return false;
    }

    i += info.width;
  }

  return true;
}

bool br_utf8_rune_start(u8 byte_value) {
  return (byte_value & 0xc0u) != 0x80u;
}

usize br_utf8_rune_count(br_bytes_view s) {
  usize count = 0u;
  usize i = 0u;

  while (i < s.len) {
    br__utf8_lead_info info;
    usize width = 1u;

    count += 1u;
    info = br__utf8_classify_lead(s.data[i]);
    if (info.ascii || info.invalid) {
      i += 1u;
      continue;
    }
    if (i + info.width > s.len) {
      i += 1u;
      continue;
    }
    if (s.data[i + 1u] < info.lo || s.data[i + 1u] > info.hi) {
      i += 1u;
      continue;
    }

    width = info.width;
    if (width > 2u && !br__utf8_is_continuation(s.data[i + 2u])) {
      width = 1u;
    } else if (width > 3u && !br__utf8_is_continuation(s.data[i + 3u])) {
      width = 1u;
    }

    i += width;
  }

  return count;
}

i32 br_utf8_rune_size(br_rune value) {
  if (value < 0) {
    return -1;
  }
  if (value <= 0x7f) {
    return 1;
  }
  if (value <= 0x7ff) {
    return 2;
  }
  if (value >= BR__UTF8_SURROGATE_MIN && value <= BR__UTF8_SURROGATE_MAX) {
    return -1;
  }
  if (value <= 0xffff) {
    return 3;
  }
  if (value <= BR_RUNE_MAX) {
    return 4;
  }
  return -1;
}

bool br_utf8_full_rune(br_bytes_view s) {
  br__utf8_lead_info info;

  if (s.len == 0u) {
    return false;
  }

  info = br__utf8_classify_lead(s.data[0]);
  if (info.ascii || info.invalid || s.len >= info.width) {
    return true;
  }
  if (s.len > 1u && (s.data[1] < info.lo || s.data[1] > info.hi)) {
    return true;
  }
  if (s.len > 2u && !br__utf8_is_continuation(s.data[2])) {
    return true;
  }

  return false;
}

#endif /* BEDROCK_IMPLEMENTATION */

#endif /* BEDROCK_SINGLE_H */
