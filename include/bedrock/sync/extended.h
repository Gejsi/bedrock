#ifndef BEDROCK_SYNC_EXTENDED_H
#define BEDROCK_SYNC_EXTENDED_H

#include <stdatomic.h>

#include <bedrock/sync/primitives.h>

BR_EXTERN_C_BEGIN

typedef struct br_wait_group {
  i32 counter;
  br_mutex mutex;
  br_cond cond;
} br_wait_group;

typedef struct br_barrier {
  br_mutex mutex;
  br_cond cond;
  i32 index;
  i32 generation;
  i32 thread_count;
} br_barrier;

typedef void (*br_once_fn)(void *ctx);
typedef void (*br_once_fn0)(void);

typedef struct br_once {
  br_mutex mutex;
  atomic_bool done;
} br_once;

typedef struct br_ticket_mutex {
  _Atomic(u32) ticket;
  _Atomic(u32) serving;
} br_ticket_mutex;

#define BR_WAIT_GROUP_INIT {.counter = 0, .mutex = BR_MUTEX_INIT, .cond = BR_COND_INIT}

#define BR_ONCE_INIT {.mutex = BR_MUTEX_INIT, .done = ATOMIC_VAR_INIT(false)}

#define BR_TICKET_MUTEX_INIT {.ticket = ATOMIC_VAR_INIT((u32)0), .serving = ATOMIC_VAR_INIT((u32)0)}

br_status br_wait_group_init(br_wait_group *wg);
void br_wait_group_destroy(br_wait_group *wg);
void br_wait_group_add(br_wait_group *wg, i32 delta);
void br_wait_group_done(br_wait_group *wg);
void br_wait_group_wait(br_wait_group *wg);

br_status br_barrier_init(br_barrier *barrier, i32 thread_count);
void br_barrier_destroy(br_barrier *barrier);
bool br_barrier_wait(br_barrier *barrier);

br_status br_once_init(br_once *once);
void br_once_destroy(br_once *once);
void br_once_do(br_once *once, br_once_fn fn, void *ctx);
void br_once_do0(br_once *once, br_once_fn0 fn);

void br_ticket_mutex_init(br_ticket_mutex *mutex);
void br_ticket_mutex_lock(br_ticket_mutex *mutex);
void br_ticket_mutex_unlock(br_ticket_mutex *mutex);

BR_EXTERN_C_END

#endif
