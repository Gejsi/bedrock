#include <assert.h>

#include <bedrock/sync/primitives_atomic.h>

static void br__atomic_mutex_lock_slow(br_atomic_mutex *mutex, u32 state) {
  u32 new_state = state;
  i32 spin;
  usize relax_count;

  for (spin = 0; spin < 100; ++spin) {
    u32 expected = BR_ATOMIC_MUTEX_UNLOCKED;
    if (br_atomic_compare_exchange_weak_explicit(
          &mutex->state, &expected, new_state, BR_ATOMIC_ACQUIRE, BR_ATOMIC_CONSUME)) {
      return;
    }

    state = expected;
    if (state == BR_ATOMIC_MUTEX_WAITING) {
      break;
    }

    for (relax_count = br_min_size((usize)spin + 1u, 32u); relax_count > 0u; --relax_count) {
      br_cpu_relax();
    }
  }

  new_state = BR_ATOMIC_MUTEX_WAITING;
  for (;;) {
    if (br_atomic_exchange_explicit(&mutex->state, BR_ATOMIC_MUTEX_WAITING, BR_ATOMIC_ACQUIRE) ==
        BR_ATOMIC_MUTEX_UNLOCKED) {
      return;
    }

    (void)br_futex_wait(&mutex->state, new_state);
    br_cpu_relax();
  }
}

void br_atomic_mutex_init(br_atomic_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }

  br_atomic_init(&mutex->state, BR_ATOMIC_MUTEX_UNLOCKED);
}

void br_atomic_mutex_lock(br_atomic_mutex *mutex) {
  u32 state;

  if (mutex == NULL) {
    return;
  }

  state = br_atomic_exchange_explicit(&mutex->state, BR_ATOMIC_MUTEX_LOCKED, BR_ATOMIC_ACQUIRE);
  if (state != BR_ATOMIC_MUTEX_UNLOCKED) {
    br__atomic_mutex_lock_slow(mutex, state);
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
    &mutex->state, &expected, BR_ATOMIC_MUTEX_LOCKED, BR_ATOMIC_ACQUIRE, BR_ATOMIC_CONSUME);
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

  (void)br_atomic_and(&rw->state, ~BR_ATOMIC_RW_MUTEX_STATE_IS_WRITING);
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
  (void)br_atomic_add(&rw->state, BR_ATOMIC_RW_MUTEX_STATE_READER);
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
    (void)br_atomic_add(&rw->state, BR_ATOMIC_RW_MUTEX_STATE_READER);
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
  mutex->recursion = 0;
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

  mutex->recursion += 1;
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

  mutex->recursion -= 1;
  if (mutex->recursion == 0) {
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
    mutex->recursion += 1;
    return true;
  }

  if (!br_atomic_mutex_try_lock(&mutex->mutex)) {
    return false;
  }

  br_atomic_store_explicit(&mutex->owner, tid, BR_ATOMIC_RELEASE);
  mutex->recursion += 1;
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

void br_atomic_cond_signal(br_atomic_cond *cond) {
  if (cond == NULL) {
    return;
  }

  (void)br_atomic_add_explicit(&cond->state, 1u, BR_ATOMIC_RELEASE);
  br_futex_signal(&cond->state);
}

void br_atomic_cond_broadcast(br_atomic_cond *cond) {
  if (cond == NULL) {
    return;
  }

  (void)br_atomic_add_explicit(&cond->state, 1u, BR_ATOMIC_RELEASE);
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
      (void)br_futex_wait(&sema->count, original_count);
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
