#include <bedrock/sync/extended.h>

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
