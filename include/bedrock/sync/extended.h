#ifndef BEDROCK_SYNC_EXTENDED_H
#define BEDROCK_SYNC_EXTENDED_H

#include <bedrock/sync/atomic.h>
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

#define BR_TICKET_MUTEX_INIT {.ticket = BR_ATOMIC_INIT((u32)0), .serving = BR_ATOMIC_INIT((u32)0)}

br_status br_wait_group_init(br_wait_group *wg);
void br_wait_group_destroy(br_wait_group *wg);
void br_wait_group_add(br_wait_group *wg, i32 delta);
void br_wait_group_done(br_wait_group *wg);
void br_wait_group_wait(br_wait_group *wg);
bool br_wait_group_wait_with_timeout(br_wait_group *wg, br_duration duration);

br_status br_barrier_init(br_barrier *barrier, i32 thread_count);
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
