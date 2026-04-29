#include <bedrock/sync/primitives_atomic.h>

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
