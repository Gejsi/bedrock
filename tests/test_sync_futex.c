#include <assert.h>

#include <bedrock.h>

#if BR_SYNC_HAS_FUTEX && !defined(_WIN32)
#include <pthread.h>

static void spin_until_bool(const br_atomic_bool *value, bool expected) {
  while (br_atomic_load_explicit(value, BR_ATOMIC_ACQUIRE) != expected) {
    br_cpu_relax();
  }
}

static void spin_until_i32(const br_atomic_i32 *value, i32 expected) {
  while (br_atomic_load_explicit(value, BR_ATOMIC_ACQUIRE) != expected) {
    br_cpu_relax();
  }
}

typedef struct futex_wait_state {
  br_futex *futex;
  br_atomic_bool waiting;
  br_atomic_bool done;
} futex_wait_state;

typedef struct sema_wait_state {
  br_atomic_sema *sema;
  br_atomic_i32 *waiting_count;
  br_atomic_i32 *done_count;
} sema_wait_state;

typedef struct atomic_cond_wait_state {
  br_atomic_cond *cond;
  br_atomic_mutex *mutex;
  bool *ready;
  br_atomic_i32 *waiting_count;
  br_atomic_i32 *done_count;
} atomic_cond_wait_state;

typedef struct atomic_mutex_state {
  br_atomic_mutex *mutex;
  br_atomic_bool *start;
  br_atomic_i32 *inside;
  br_atomic_i32 *counter;
  i32 iterations;
} atomic_mutex_state;

typedef struct atomic_rw_reader_state {
  br_atomic_rw_mutex *rw;
  br_atomic_bool *release;
  br_atomic_i32 *inside;
  br_atomic_i32 *entered;
} atomic_rw_reader_state;

typedef struct atomic_rw_writer_state {
  br_atomic_rw_mutex *rw;
  br_atomic_bool *release;
  br_atomic_bool *locked;
} atomic_rw_writer_state;

typedef struct atomic_recursive_try_state {
  br_atomic_recursive_mutex *mutex;
  br_atomic_bool *start;
  br_atomic_bool *done;
  br_atomic_bool *result;
} atomic_recursive_try_state;

static void *futex_wait_worker(void *ctx) {
  futex_wait_state *state = (futex_wait_state *)ctx;

  br_atomic_store_explicit(&state->waiting, true, BR_ATOMIC_RELEASE);
  while (br_atomic_load_explicit(state->futex, BR_ATOMIC_ACQUIRE) == 0u) {
    assert(br_futex_wait(state->futex, 0u));
  }
  br_atomic_store_explicit(&state->done, true, BR_ATOMIC_RELEASE);
  return NULL;
}

static void *sema_wait_worker(void *ctx) {
  sema_wait_state *state = (sema_wait_state *)ctx;

  br_atomic_add_explicit(state->waiting_count, 1, BR_ATOMIC_RELEASE);
  br_atomic_sema_wait(state->sema);
  br_atomic_add_explicit(state->done_count, 1, BR_ATOMIC_RELEASE);
  return NULL;
}

static void *atomic_cond_wait_worker(void *ctx) {
  atomic_cond_wait_state *state = (atomic_cond_wait_state *)ctx;

  br_atomic_mutex_lock(state->mutex);
  br_atomic_add_explicit(state->waiting_count, 1, BR_ATOMIC_RELEASE);
  while (!*state->ready) {
    assert(br_atomic_cond_wait(state->cond, state->mutex));
  }
  br_atomic_add_explicit(state->done_count, 1, BR_ATOMIC_RELEASE);
  br_atomic_mutex_unlock(state->mutex);
  return NULL;
}

static void *atomic_mutex_worker(void *ctx) {
  atomic_mutex_state *state = (atomic_mutex_state *)ctx;
  i32 i;

  spin_until_bool(state->start, true);
  for (i = 0; i < state->iterations; ++i) {
    i32 inside;

    br_atomic_mutex_lock(state->mutex);
    inside = br_atomic_add_explicit(state->inside, 1, BR_ATOMIC_ACQ_REL);
    assert(inside == 0);
    br_atomic_add_explicit(state->counter, 1, BR_ATOMIC_RELAXED);
    assert(br_atomic_sub_explicit(state->inside, 1, BR_ATOMIC_ACQ_REL) == 1);
    br_atomic_mutex_unlock(state->mutex);
  }
  return NULL;
}

static void *atomic_rw_reader_worker(void *ctx) {
  atomic_rw_reader_state *state = (atomic_rw_reader_state *)ctx;

  br_atomic_rw_mutex_shared_lock(state->rw);
  br_atomic_add_explicit(state->inside, 1, BR_ATOMIC_ACQ_REL);
  br_atomic_add_explicit(state->entered, 1, BR_ATOMIC_RELEASE);
  spin_until_bool(state->release, true);
  br_atomic_sub_explicit(state->inside, 1, BR_ATOMIC_ACQ_REL);
  br_atomic_rw_mutex_shared_unlock(state->rw);
  return NULL;
}

static void *atomic_rw_writer_worker(void *ctx) {
  atomic_rw_writer_state *state = (atomic_rw_writer_state *)ctx;

  br_atomic_rw_mutex_lock(state->rw);
  br_atomic_store_explicit(state->locked, true, BR_ATOMIC_RELEASE);
  spin_until_bool(state->release, true);
  br_atomic_rw_mutex_unlock(state->rw);
  return NULL;
}

static void *atomic_recursive_try_worker(void *ctx) {
  atomic_recursive_try_state *state = (atomic_recursive_try_state *)ctx;
  bool result;

  spin_until_bool(state->start, true);
  result = br_atomic_recursive_mutex_try_lock(state->mutex);
  if (result) {
    br_atomic_recursive_mutex_unlock(state->mutex);
  }
  br_atomic_store_explicit(state->result, result, BR_ATOMIC_RELEASE);
  br_atomic_store_explicit(state->done, true, BR_ATOMIC_RELEASE);
  return NULL;
}

static bool atomic_recursive_try_from_other_thread(br_atomic_recursive_mutex *mutex) {
  br_atomic_bool start;
  br_atomic_bool done;
  br_atomic_bool result;
  atomic_recursive_try_state state;
  pthread_t thread;

  br_atomic_init(&start, false);
  br_atomic_init(&done, false);
  br_atomic_init(&result, false);
  state.mutex = mutex;
  state.start = &start;
  state.done = &done;
  state.result = &result;

  assert(pthread_create(&thread, NULL, atomic_recursive_try_worker, &state) == 0);
  br_atomic_store_explicit(&start, true, BR_ATOMIC_RELEASE);
  spin_until_bool(&done, true);
  assert(pthread_join(thread, NULL) == 0);
  return br_atomic_load_explicit(&result, BR_ATOMIC_ACQUIRE);
}

static void test_futex_wait_signal(void) {
  br_futex futex;
  futex_wait_state state;
  pthread_t thread;

  br_atomic_init(&futex, 0u);
  state.futex = &futex;
  br_atomic_init(&state.waiting, false);
  br_atomic_init(&state.done, false);

  assert(pthread_create(&thread, NULL, futex_wait_worker, &state) == 0);
  spin_until_bool(&state.waiting, true);

  br_atomic_store_explicit(&futex, 1u, BR_ATOMIC_RELEASE);
  br_futex_signal(&futex);

  assert(pthread_join(thread, NULL) == 0);
  assert(br_atomic_load_explicit(&state.done, BR_ATOMIC_ACQUIRE));
}

static void test_atomic_sema_wait_post_one(void) {
  br_atomic_sema sema;
  br_atomic_i32 waiting_count;
  br_atomic_i32 done_count;
  sema_wait_state state;
  pthread_t thread;

  br_atomic_sema_init(&sema, 0u);
  br_atomic_init(&waiting_count, 0);
  br_atomic_init(&done_count, 0);
  state.sema = &sema;
  state.waiting_count = &waiting_count;
  state.done_count = &done_count;

  assert(pthread_create(&thread, NULL, sema_wait_worker, &state) == 0);
  spin_until_i32(&waiting_count, 1);
  assert(br_atomic_load_explicit(&done_count, BR_ATOMIC_ACQUIRE) == 0);

  br_atomic_sema_post(&sema, 1u);

  assert(pthread_join(thread, NULL) == 0);
  assert(br_atomic_load_explicit(&done_count, BR_ATOMIC_ACQUIRE) == 1);
}

static void test_atomic_sema_post_many(void) {
  enum { THREAD_COUNT = 3 };
  br_atomic_sema sema;
  br_atomic_i32 waiting_count;
  br_atomic_i32 done_count;
  sema_wait_state state;
  pthread_t threads[THREAD_COUNT];
  i32 i;

  br_atomic_sema_init(&sema, 0u);
  br_atomic_init(&waiting_count, 0);
  br_atomic_init(&done_count, 0);
  state.sema = &sema;
  state.waiting_count = &waiting_count;
  state.done_count = &done_count;

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[(usize)i], NULL, sema_wait_worker, &state) == 0);
  }
  spin_until_i32(&waiting_count, THREAD_COUNT);

  br_atomic_sema_post(&sema, THREAD_COUNT);

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[(usize)i], NULL) == 0);
  }
  assert(br_atomic_load_explicit(&done_count, BR_ATOMIC_ACQUIRE) == THREAD_COUNT);
}

static void test_atomic_cond_signal(void) {
  br_atomic_cond cond = BR_ATOMIC_COND_INIT;
  br_atomic_mutex mutex = BR_ATOMIC_MUTEX_INIT;
  br_atomic_i32 waiting_count;
  br_atomic_i32 done_count;
  atomic_cond_wait_state state;
  pthread_t thread;
  bool ready = false;

  br_atomic_init(&waiting_count, 0);
  br_atomic_init(&done_count, 0);
  state.cond = &cond;
  state.mutex = &mutex;
  state.ready = &ready;
  state.waiting_count = &waiting_count;
  state.done_count = &done_count;

  assert(pthread_create(&thread, NULL, atomic_cond_wait_worker, &state) == 0);
  spin_until_i32(&waiting_count, 1);
  assert(br_atomic_load_explicit(&done_count, BR_ATOMIC_ACQUIRE) == 0);

  br_atomic_mutex_lock(&mutex);
  ready = true;
  br_atomic_cond_signal(&cond);
  br_atomic_mutex_unlock(&mutex);

  assert(pthread_join(thread, NULL) == 0);
  assert(br_atomic_load_explicit(&done_count, BR_ATOMIC_ACQUIRE) == 1);
}

static void test_atomic_cond_broadcast(void) {
  enum { THREAD_COUNT = 4 };
  br_atomic_cond cond = BR_ATOMIC_COND_INIT;
  br_atomic_mutex mutex = BR_ATOMIC_MUTEX_INIT;
  br_atomic_i32 waiting_count;
  br_atomic_i32 done_count;
  atomic_cond_wait_state state;
  pthread_t threads[THREAD_COUNT];
  bool ready = false;
  i32 i;

  br_atomic_init(&waiting_count, 0);
  br_atomic_init(&done_count, 0);
  state.cond = &cond;
  state.mutex = &mutex;
  state.ready = &ready;
  state.waiting_count = &waiting_count;
  state.done_count = &done_count;

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[(usize)i], NULL, atomic_cond_wait_worker, &state) == 0);
  }
  spin_until_i32(&waiting_count, THREAD_COUNT);
  assert(br_atomic_load_explicit(&done_count, BR_ATOMIC_ACQUIRE) == 0);

  br_atomic_mutex_lock(&mutex);
  ready = true;
  br_atomic_cond_broadcast(&cond);
  br_atomic_mutex_unlock(&mutex);

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[(usize)i], NULL) == 0);
  }
  assert(br_atomic_load_explicit(&done_count, BR_ATOMIC_ACQUIRE) == THREAD_COUNT);
}

static void test_atomic_mutex_try_lock(void) {
  br_atomic_mutex mutex = BR_ATOMIC_MUTEX_INIT;

  assert(br_atomic_mutex_try_lock(&mutex));
  assert(!br_atomic_mutex_try_lock(&mutex));
  br_atomic_mutex_unlock(&mutex);
  assert(br_atomic_mutex_try_lock(&mutex));
  br_atomic_mutex_unlock(&mutex);
}

static void test_atomic_mutex_contention(void) {
  enum { THREAD_COUNT = 4, ITERATIONS = 5000 };
  br_atomic_mutex mutex = BR_ATOMIC_MUTEX_INIT;
  br_atomic_bool start;
  br_atomic_i32 inside;
  br_atomic_i32 counter;
  atomic_mutex_state state;
  pthread_t threads[THREAD_COUNT];
  i32 i;

  br_atomic_init(&start, false);
  br_atomic_init(&inside, 0);
  br_atomic_init(&counter, 0);
  state.mutex = &mutex;
  state.start = &start;
  state.inside = &inside;
  state.counter = &counter;
  state.iterations = ITERATIONS;

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[(usize)i], NULL, atomic_mutex_worker, &state) == 0);
  }

  br_atomic_store_explicit(&start, true, BR_ATOMIC_RELEASE);

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[(usize)i], NULL) == 0);
  }
  assert(br_atomic_load_explicit(&inside, BR_ATOMIC_ACQUIRE) == 0);
  assert(br_atomic_load_explicit(&counter, BR_ATOMIC_ACQUIRE) == THREAD_COUNT * ITERATIONS);
}

static void test_atomic_rw_mutex_try_lock(void) {
  br_atomic_rw_mutex rw = BR_ATOMIC_RW_MUTEX_INIT;

  assert(br_atomic_rw_mutex_try_lock(&rw));
  assert(!br_atomic_rw_mutex_try_lock(&rw));
  assert(!br_atomic_rw_mutex_try_shared_lock(&rw));
  br_atomic_rw_mutex_unlock(&rw);

  assert(br_atomic_rw_mutex_try_shared_lock(&rw));
  assert(br_atomic_rw_mutex_try_shared_lock(&rw));
  assert(!br_atomic_rw_mutex_try_lock(&rw));
  br_atomic_rw_mutex_shared_unlock(&rw);
  br_atomic_rw_mutex_shared_unlock(&rw);

  assert(br_atomic_rw_mutex_try_lock(&rw));
  br_atomic_rw_mutex_unlock(&rw);
}

static void test_atomic_rw_mutex_readers_share(void) {
  enum { THREAD_COUNT = 4 };
  br_atomic_rw_mutex rw = BR_ATOMIC_RW_MUTEX_INIT;
  br_atomic_bool release;
  br_atomic_i32 inside;
  br_atomic_i32 entered;
  atomic_rw_reader_state state;
  pthread_t threads[THREAD_COUNT];
  i32 i;

  br_atomic_init(&release, false);
  br_atomic_init(&inside, 0);
  br_atomic_init(&entered, 0);
  state.rw = &rw;
  state.release = &release;
  state.inside = &inside;
  state.entered = &entered;

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[(usize)i], NULL, atomic_rw_reader_worker, &state) == 0);
  }

  spin_until_i32(&entered, THREAD_COUNT);
  assert(br_atomic_load_explicit(&inside, BR_ATOMIC_ACQUIRE) == THREAD_COUNT);
  assert(!br_atomic_rw_mutex_try_lock(&rw));

  br_atomic_store_explicit(&release, true, BR_ATOMIC_RELEASE);
  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[(usize)i], NULL) == 0);
  }
  assert(br_atomic_load_explicit(&inside, BR_ATOMIC_ACQUIRE) == 0);
}

static void test_atomic_rw_mutex_writer_waits_for_readers(void) {
  br_atomic_rw_mutex rw = BR_ATOMIC_RW_MUTEX_INIT;
  br_atomic_bool release;
  br_atomic_bool locked;
  atomic_rw_writer_state state;
  pthread_t thread;

  br_atomic_init(&release, false);
  br_atomic_init(&locked, false);
  state.rw = &rw;
  state.release = &release;
  state.locked = &locked;

  br_atomic_rw_mutex_shared_lock(&rw);
  assert(pthread_create(&thread, NULL, atomic_rw_writer_worker, &state) == 0);
  while ((br_atomic_load_explicit(&rw.state, BR_ATOMIC_ACQUIRE) &
          BR_ATOMIC_RW_MUTEX_STATE_IS_WRITING) == 0u) {
    br_cpu_relax();
  }
  assert(!br_atomic_load_explicit(&locked, BR_ATOMIC_ACQUIRE));

  br_atomic_rw_mutex_shared_unlock(&rw);
  spin_until_bool(&locked, true);
  assert(!br_atomic_rw_mutex_try_lock(&rw));
  assert(!br_atomic_rw_mutex_try_shared_lock(&rw));

  br_atomic_store_explicit(&release, true, BR_ATOMIC_RELEASE);
  assert(pthread_join(thread, NULL) == 0);
  assert(br_atomic_rw_mutex_try_shared_lock(&rw));
  br_atomic_rw_mutex_shared_unlock(&rw);
}

static void test_atomic_recursive_mutex_reentrant(void) {
  br_atomic_recursive_mutex mutex = BR_ATOMIC_RECURSIVE_MUTEX_INIT;

  assert(br_current_thread_id() != BR_THREAD_ID_INVALID);

  br_atomic_recursive_mutex_lock(&mutex);
  br_atomic_recursive_mutex_lock(&mutex);
  br_atomic_recursive_mutex_unlock(&mutex);
  br_atomic_recursive_mutex_unlock(&mutex);

  assert(br_atomic_recursive_mutex_try_lock(&mutex));
  assert(br_atomic_recursive_mutex_try_lock(&mutex));
  assert(!atomic_recursive_try_from_other_thread(&mutex));
  br_atomic_recursive_mutex_unlock(&mutex);
  assert(!atomic_recursive_try_from_other_thread(&mutex));
  br_atomic_recursive_mutex_unlock(&mutex);
  assert(atomic_recursive_try_from_other_thread(&mutex));
}
#endif

int main(void) {
#if BR_SYNC_HAS_FUTEX && !defined(_WIN32)
  test_futex_wait_signal();
  test_atomic_mutex_try_lock();
  test_atomic_mutex_contention();
  test_atomic_rw_mutex_try_lock();
  test_atomic_rw_mutex_readers_share();
  test_atomic_rw_mutex_writer_waits_for_readers();
  test_atomic_recursive_mutex_reentrant();
  test_atomic_cond_signal();
  test_atomic_cond_broadcast();
  test_atomic_sema_wait_post_one();
  test_atomic_sema_post_many();
#endif
  return 0;
}
