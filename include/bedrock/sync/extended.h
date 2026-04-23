#ifndef BEDROCK_SYNC_EXTENDED_H
#define BEDROCK_SYNC_EXTENDED_H

#include <stdatomic.h>

#include <bedrock/sync/primitives.h>

BR_EXTERN_C_BEGIN

typedef struct br_wait_group {
  int counter;
  br_mutex mutex;
  br_cond cond;
} br_wait_group;

typedef struct br_barrier {
  br_mutex mutex;
  br_cond cond;
  int index;
  int generation;
  int thread_count;
} br_barrier;

typedef void (*br_once_fn)(void *ctx);
typedef void (*br_once_fn0)(void);

typedef struct br_once {
  br_mutex mutex;
  atomic_bool done;
} br_once;

typedef struct br_ticket_mutex {
  atomic_uint ticket;
  atomic_uint serving;
} br_ticket_mutex;

#define BR_WAIT_GROUP_INIT {.counter = 0, .mutex = BR_MUTEX_INIT, .cond = BR_COND_INIT}

#define BR_ONCE_INIT {.mutex = BR_MUTEX_INIT, .done = ATOMIC_VAR_INIT(false)}

#define BR_TICKET_MUTEX_INIT {.ticket = ATOMIC_VAR_INIT(0u), .serving = ATOMIC_VAR_INIT(0u)}

br_status br_wait_group_init(br_wait_group *wg);
void br_wait_group_destroy(br_wait_group *wg);
void br_wait_group_add(br_wait_group *wg, int delta);
void br_wait_group_done(br_wait_group *wg);
void br_wait_group_wait(br_wait_group *wg);

br_status br_barrier_init(br_barrier *barrier, int thread_count);
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
