#ifndef BEDROCK_SYNC_PRIMITIVES_ATOMIC_H
#define BEDROCK_SYNC_PRIMITIVES_ATOMIC_H

#include <bedrock/sync/futex.h>

BR_EXTERN_C_BEGIN

typedef enum br_atomic_mutex_state {
  BR_ATOMIC_MUTEX_UNLOCKED = 0,
  BR_ATOMIC_MUTEX_LOCKED,
  BR_ATOMIC_MUTEX_WAITING,
} br_atomic_mutex_state;

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

#define BR_ATOMIC_MUTEX_INIT {.state = BR_FUTEX_INIT(BR_ATOMIC_MUTEX_UNLOCKED)}
#define BR_ATOMIC_SEMA_INIT(count) {.count = BR_FUTEX_INIT(count)}

void br_atomic_mutex_init(br_atomic_mutex *mutex);
void br_atomic_mutex_lock(br_atomic_mutex *mutex);
void br_atomic_mutex_unlock(br_atomic_mutex *mutex);
bool br_atomic_mutex_try_lock(br_atomic_mutex *mutex);

void br_atomic_sema_init(br_atomic_sema *sema, u32 count);
void br_atomic_sema_post(br_atomic_sema *sema, u32 count);
void br_atomic_sema_wait(br_atomic_sema *sema);

BR_EXTERN_C_END

#endif
