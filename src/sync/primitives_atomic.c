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
