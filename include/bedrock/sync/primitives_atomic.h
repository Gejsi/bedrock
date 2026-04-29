#ifndef BEDROCK_SYNC_PRIMITIVES_ATOMIC_H
#define BEDROCK_SYNC_PRIMITIVES_ATOMIC_H

#include <bedrock/sync/futex.h>
#include <bedrock/sync/primitives.h>

BR_EXTERN_C_BEGIN

typedef enum br_atomic_mutex_state {
  BR_ATOMIC_MUTEX_UNLOCKED = 0,
  BR_ATOMIC_MUTEX_LOCKED,
  BR_ATOMIC_MUTEX_WAITING,
} br_atomic_mutex_state;

typedef usize br_atomic_rw_mutex_state;

#define BR_ATOMIC_RW_MUTEX_STATE_IS_WRITING ((usize)1 << (sizeof(usize) * 8u - 1u))
#define BR_ATOMIC_RW_MUTEX_STATE_READER ((usize)1)
#define BR_ATOMIC_RW_MUTEX_STATE_READER_MASK (~BR_ATOMIC_RW_MUTEX_STATE_IS_WRITING)

/*
An Atomic_Mutex is a mutual exclusion lock.
The zero value for an Atomic_Mutex is an unlocked mutex.

An Atomic_Mutex must not be copied after first use.

Odin's lower atomic primitives are zero-value-ready. Bedrock keeps that property
inside this layer even though the current public `br_mutex` still uses explicit
init/destroy while it wraps native OS primitives.
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

Odin stores a public Mutex here. Bedrock uses the lower br_atomic_mutex until
the public br_mutex backend is moved from pthread wrappers to the atomic/futex
path; this keeps the atomic layer zero-value-ready.
*/
typedef struct br_atomic_recursive_mutex {
  br_atomic_u64 owner;
  i32 recursion;
  br_atomic_mutex mutex;
} br_atomic_recursive_mutex;

#define BR_ATOMIC_MUTEX_INIT {.state = BR_FUTEX_INIT(BR_ATOMIC_MUTEX_UNLOCKED)}
#define BR_ATOMIC_SEMA_INIT(initial_count) {.count = BR_FUTEX_INIT(initial_count)}
#define BR_ATOMIC_RW_MUTEX_INIT                                                                    \
  {.state = BR_ATOMIC_INIT(0), .mutex = BR_ATOMIC_MUTEX_INIT, .sema = BR_ATOMIC_SEMA_INIT(0u)}
#define BR_ATOMIC_RECURSIVE_MUTEX_INIT                                                             \
  {.owner = BR_ATOMIC_INIT(BR_THREAD_ID_INVALID), .recursion = 0, .mutex = BR_ATOMIC_MUTEX_INIT}

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

void br_atomic_sema_init(br_atomic_sema *sema, u32 count);
void br_atomic_sema_post(br_atomic_sema *sema, u32 count);
void br_atomic_sema_wait(br_atomic_sema *sema);

BR_EXTERN_C_END

#endif
