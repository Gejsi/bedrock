#ifndef BEDROCK_SYNC_PRIMITIVES_H
#define BEDROCK_SYNC_PRIMITIVES_H

#include <bedrock/base.h>
#include <bedrock/sync/primitives_atomic.h>
#include <bedrock/sync/thread.h>

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
void br_cond_signal(br_cond *cond);
void br_cond_broadcast(br_cond *cond);

br_status br_sema_init(br_sema *sema, u32 count);
void br_sema_destroy(br_sema *sema);
void br_sema_post(br_sema *sema, u32 count);
void br_sema_wait(br_sema *sema);

BR_EXTERN_C_END

#endif
