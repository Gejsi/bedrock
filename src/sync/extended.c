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

  br_atomic_store_explicit(&once->done, false, BR_ATOMIC_RELAXED);
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

void br_ticket_mutex_init(br_ticket_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }

  br_atomic_store_explicit(&mutex->ticket, 0u, BR_ATOMIC_RELAXED);
  br_atomic_store_explicit(&mutex->serving, 0u, BR_ATOMIC_RELAXED);
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
