#include <assert.h>

#include <bedrock.h>

static void test_once_increment(void *ctx) {
  br_atomic_add_explicit((br_atomic_i32 *)ctx, 1, BR_ATOMIC_RELAXED);
}

#if !defined(_WIN32)
#include <pthread.h>

static void test_spin_until_bool(const br_atomic_bool *value, bool expected) {
  while (br_atomic_load_explicit(value, BR_ATOMIC_ACQUIRE) != expected) {
    br_cpu_relax();
  }
}

static void test_spin_until_i32_eq(const br_atomic_i32 *value, i32 expected) {
  while (br_atomic_load_explicit(value, BR_ATOMIC_ACQUIRE) != expected) {
    br_cpu_relax();
  }
}

typedef struct test_cond_state {
  br_mutex mutex;
  br_cond cond;
  bool ready;
  br_atomic_i32 waiting;
  br_atomic_i32 seen;
} test_cond_state;

typedef struct test_rw_reader_state {
  br_rw_mutex *mutex;
  br_atomic_i32 entered;
  br_atomic_bool release;
} test_rw_reader_state;

typedef struct test_wait_group_state {
  br_wait_group *wg;
  br_atomic_i32 *counter;
} test_wait_group_state;

typedef struct test_barrier_state {
  br_barrier *barrier;
  br_atomic_i32 *leaders_per_round;
  br_atomic_i32 *after_per_round;
  i32 rounds;
} test_barrier_state;

typedef struct test_once_state {
  br_once *once;
  br_atomic_i32 *count;
  br_atomic_bool *start;
} test_once_state;

typedef struct test_ticket_state {
  br_ticket_mutex *mutex;
  br_atomic_i32 *counter;
  br_atomic_i32 *inside;
  br_atomic_bool *start;
  i32 iterations;
} test_ticket_state;

static void *test_cond_waiter(void *ctx) {
  test_cond_state *state = (test_cond_state *)ctx;

  br_mutex_lock(&state->mutex);
  br_atomic_add_explicit(&state->waiting, 1, BR_ATOMIC_RELEASE);
  while (!state->ready) {
    br_cond_wait(&state->cond, &state->mutex);
  }
  br_atomic_add_explicit(&state->seen, 1, BR_ATOMIC_RELAXED);
  br_mutex_unlock(&state->mutex);
  return NULL;
}

static void *test_rw_reader(void *ctx) {
  test_rw_reader_state *state = (test_rw_reader_state *)ctx;

  br_rw_mutex_shared_lock(state->mutex);
  br_atomic_add_explicit(&state->entered, 1, BR_ATOMIC_RELEASE);
  while (!br_atomic_load_explicit(&state->release, BR_ATOMIC_ACQUIRE)) {
    br_cpu_relax();
  }
  br_rw_mutex_shared_unlock(state->mutex);
  return NULL;
}

static void *test_wait_group_worker(void *ctx) {
  test_wait_group_state *state = (test_wait_group_state *)ctx;

  br_atomic_add_explicit(state->counter, 1, BR_ATOMIC_RELAXED);
  br_wait_group_done(state->wg);
  return NULL;
}

static void *test_barrier_worker(void *ctx) {
  test_barrier_state *state = (test_barrier_state *)ctx;
  i32 round;

  for (round = 0; round < state->rounds; ++round) {
    if (br_barrier_wait(state->barrier)) {
      br_atomic_add_explicit(&state->leaders_per_round[round], 1, BR_ATOMIC_RELAXED);
    }
    br_atomic_add_explicit(&state->after_per_round[round], 1, BR_ATOMIC_RELAXED);
  }
  return NULL;
}

static void *test_once_worker(void *ctx) {
  test_once_state *state = (test_once_state *)ctx;

  test_spin_until_bool(state->start, true);
  br_once_do(state->once, test_once_increment, state->count);
  return NULL;
}

static void *test_ticket_worker(void *ctx) {
  test_ticket_state *state = (test_ticket_state *)ctx;
  i32 i;

  test_spin_until_bool(state->start, true);
  for (i = 0; i < state->iterations; ++i) {
    i32 inside_before;

    br_ticket_mutex_lock(state->mutex);
    inside_before = br_atomic_add_explicit(state->inside, 1, BR_ATOMIC_ACQ_REL);
    assert(inside_before == 0);
    br_atomic_add_explicit(state->counter, 1, BR_ATOMIC_RELAXED);
    assert(br_atomic_sub_explicit(state->inside, 1, BR_ATOMIC_ACQ_REL) == 1);
    br_ticket_mutex_unlock(state->mutex);
  }
  return NULL;
}
#endif

static void test_sync_mutex_and_guard(void) {
  br_mutex mutex;
  br_status status;
  i32 value = 0;

  status = br_mutex_init(&mutex);
  assert(status == BR_STATUS_OK);

  br_guard(&mutex) {
    value = 7;
  }

  assert(value == 7);
  assert(br_mutex_try_lock(&mutex));
  br_mutex_unlock(&mutex);
  br_mutex_destroy(&mutex);
}

static void test_sync_recursive_mutex(void) {
  br_recursive_mutex mutex;
  br_status status;

  status = br_recursive_mutex_init(&mutex);
  assert(status == BR_STATUS_OK);

  br_recursive_mutex_lock(&mutex);
  br_recursive_mutex_lock(&mutex);
  assert(br_recursive_mutex_try_lock(&mutex));
  br_recursive_mutex_unlock(&mutex);
  br_recursive_mutex_unlock(&mutex);
  br_recursive_mutex_unlock(&mutex);

  br_recursive_mutex_destroy(&mutex);
}

#if BR_SYNC_HAS_FUTEX && !defined(_WIN32)
static void test_sync_zero_value_public_primitives(void) {
  br_mutex mutex = {0};
  br_rw_mutex rw_mutex = {0};
  br_recursive_mutex recursive_mutex = {0};

  br_mutex_lock(&mutex);
  br_mutex_unlock(&mutex);
  assert(br_mutex_try_lock(&mutex));
  br_mutex_unlock(&mutex);

  br_rw_mutex_shared_lock(&rw_mutex);
  assert(br_rw_mutex_try_shared_lock(&rw_mutex));
  br_rw_mutex_shared_unlock(&rw_mutex);
  br_rw_mutex_shared_unlock(&rw_mutex);
  assert(br_rw_mutex_try_lock(&rw_mutex));
  br_rw_mutex_unlock(&rw_mutex);

  br_recursive_mutex_lock(&recursive_mutex);
  br_recursive_mutex_lock(&recursive_mutex);
  assert(br_recursive_mutex_try_lock(&recursive_mutex));
  br_recursive_mutex_unlock(&recursive_mutex);
  br_recursive_mutex_unlock(&recursive_mutex);
  br_recursive_mutex_unlock(&recursive_mutex);
}

static void test_sync_zero_value_cond_signal(void) {
  test_cond_state state = {0};
  pthread_t thread;

  state.ready = false;
  br_atomic_init(&state.waiting, 0);
  br_atomic_init(&state.seen, 0);

  assert(pthread_create(&thread, NULL, test_cond_waiter, &state) == 0);
  test_spin_until_i32_eq(&state.waiting, 1);

  br_mutex_lock(&state.mutex);
  state.ready = true;
  br_cond_signal(&state.cond);
  br_mutex_unlock(&state.mutex);

  assert(pthread_join(thread, NULL) == 0);
  assert(br_atomic_load_explicit(&state.seen, BR_ATOMIC_RELAXED) == 1);
}
#endif

static void test_sync_once_basic(void) {
  br_once once;
  br_atomic_i32 count;

  assert(br_once_init(&once) == BR_STATUS_OK);
  br_atomic_init(&count, 0);

  br_once_do(&once, test_once_increment, &count);
  br_once_do(&once, test_once_increment, &count);

  assert(br_atomic_load_explicit(&count, BR_ATOMIC_RELAXED) == 1);
  br_once_destroy(&once);
}

#if !defined(_WIN32)
static void test_sync_cond_signal(void) {
  test_cond_state state;
  pthread_t thread;

  assert(br_mutex_init(&state.mutex) == BR_STATUS_OK);
  assert(br_cond_init(&state.cond) == BR_STATUS_OK);
  state.ready = false;
  br_atomic_init(&state.waiting, 0);
  br_atomic_init(&state.seen, 0);

  assert(pthread_create(&thread, NULL, test_cond_waiter, &state) == 0);
  test_spin_until_i32_eq(&state.waiting, 1);

  br_mutex_lock(&state.mutex);
  state.ready = true;
  br_cond_signal(&state.cond);
  br_mutex_unlock(&state.mutex);

  assert(pthread_join(thread, NULL) == 0);
  assert(br_atomic_load_explicit(&state.seen, BR_ATOMIC_RELAXED) == 1);

  br_cond_destroy(&state.cond);
  br_mutex_destroy(&state.mutex);
}

static void test_sync_cond_broadcast(void) {
  enum { THREAD_COUNT = 4 };
  test_cond_state state;
  pthread_t threads[THREAD_COUNT];
  i32 i;

  assert(br_mutex_init(&state.mutex) == BR_STATUS_OK);
  assert(br_cond_init(&state.cond) == BR_STATUS_OK);
  state.ready = false;
  br_atomic_init(&state.waiting, 0);
  br_atomic_init(&state.seen, 0);

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[(usize)i], NULL, test_cond_waiter, &state) == 0);
  }
  test_spin_until_i32_eq(&state.waiting, THREAD_COUNT);

  br_mutex_lock(&state.mutex);
  state.ready = true;
  br_cond_broadcast(&state.cond);
  br_mutex_unlock(&state.mutex);

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[(usize)i], NULL) == 0);
  }

  assert(br_atomic_load_explicit(&state.seen, BR_ATOMIC_RELAXED) == THREAD_COUNT);
  br_cond_destroy(&state.cond);
  br_mutex_destroy(&state.mutex);
}

static void test_sync_rw_mutex(void) {
  enum { THREAD_COUNT = 2 };
  br_rw_mutex mutex;
  test_rw_reader_state state;
  pthread_t threads[THREAD_COUNT];
  i32 i;

  assert(br_rw_mutex_init(&mutex) == BR_STATUS_OK);
  state.mutex = &mutex;
  br_atomic_init(&state.entered, 0);
  br_atomic_init(&state.release, false);

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[(usize)i], NULL, test_rw_reader, &state) == 0);
  }
  test_spin_until_i32_eq(&state.entered, THREAD_COUNT);

  assert(!br_rw_mutex_try_lock(&mutex));
  br_atomic_store_explicit(&state.release, true, BR_ATOMIC_RELEASE);
  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[(usize)i], NULL) == 0);
  }
  assert(br_rw_mutex_try_lock(&mutex));
  br_rw_mutex_unlock(&mutex);

  br_rw_mutex_destroy(&mutex);
}

static void test_sync_wait_group(void) {
  enum { THREAD_COUNT = 4 };
  br_wait_group wg;
  test_wait_group_state state;
  pthread_t threads[THREAD_COUNT];
  br_atomic_i32 counter;
  i32 i;

  assert(br_wait_group_init(&wg) == BR_STATUS_OK);
  br_atomic_init(&counter, 0);
  state.wg = &wg;
  state.counter = &counter;

  br_wait_group_add(&wg, THREAD_COUNT);
  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[(usize)i], NULL, test_wait_group_worker, &state) == 0);
  }

  br_wait_group_wait(&wg);
  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[(usize)i], NULL) == 0);
  }

  assert(br_atomic_load_explicit(&counter, BR_ATOMIC_RELAXED) == THREAD_COUNT);
  br_wait_group_destroy(&wg);
}

static void test_sync_barrier(void) {
  enum { THREAD_COUNT = 3, ROUNDS = 4 };
  br_barrier barrier;
  test_barrier_state state;
  pthread_t threads[THREAD_COUNT];
  br_atomic_i32 leaders_per_round[ROUNDS];
  br_atomic_i32 after_per_round[ROUNDS];
  i32 i;
  i32 round;

  assert(br_barrier_init(&barrier, THREAD_COUNT + 1) == BR_STATUS_OK);
  state.barrier = &barrier;
  state.leaders_per_round = leaders_per_round;
  state.after_per_round = after_per_round;
  state.rounds = ROUNDS;

  for (round = 0; round < ROUNDS; ++round) {
    br_atomic_init(&leaders_per_round[(usize)round], 0);
    br_atomic_init(&after_per_round[(usize)round], 0);
  }

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[(usize)i], NULL, test_barrier_worker, &state) == 0);
  }

  for (round = 0; round < ROUNDS; ++round) {
    if (br_barrier_wait(&barrier)) {
      br_atomic_add_explicit(&leaders_per_round[(usize)round], 1, BR_ATOMIC_RELAXED);
    }
    br_atomic_add_explicit(&after_per_round[(usize)round], 1, BR_ATOMIC_RELAXED);
  }

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[(usize)i], NULL) == 0);
  }

  for (round = 0; round < ROUNDS; ++round) {
    assert(br_atomic_load_explicit(&leaders_per_round[(usize)round], BR_ATOMIC_RELAXED) == 1);
    assert(br_atomic_load_explicit(&after_per_round[(usize)round], BR_ATOMIC_RELAXED) ==
           THREAD_COUNT + 1);
  }
  br_barrier_destroy(&barrier);
}

static void test_sync_once_threads(void) {
  enum { THREAD_COUNT = 6 };
  br_once once;
  test_once_state state;
  pthread_t threads[THREAD_COUNT];
  br_atomic_i32 count;
  br_atomic_bool start;
  i32 i;

  assert(br_once_init(&once) == BR_STATUS_OK);
  br_atomic_init(&count, 0);
  br_atomic_init(&start, false);
  state.once = &once;
  state.count = &count;
  state.start = &start;

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[(usize)i], NULL, test_once_worker, &state) == 0);
  }
  br_atomic_store_explicit(&start, true, BR_ATOMIC_RELEASE);
  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[(usize)i], NULL) == 0);
  }

  assert(br_atomic_load_explicit(&count, BR_ATOMIC_RELAXED) == 1);
  br_once_destroy(&once);
}

static void test_sync_ticket_mutex(void) {
  enum { THREAD_COUNT = 4, ITERATIONS = 1000 };
  br_ticket_mutex mutex;
  test_ticket_state state;
  pthread_t threads[THREAD_COUNT];
  br_atomic_i32 counter;
  br_atomic_i32 inside;
  br_atomic_bool start;
  i32 i;

  br_ticket_mutex_init(&mutex);
  br_atomic_init(&counter, 0);
  br_atomic_init(&inside, 0);
  br_atomic_init(&start, false);
  state.mutex = &mutex;
  state.counter = &counter;
  state.inside = &inside;
  state.start = &start;
  state.iterations = ITERATIONS;

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[(usize)i], NULL, test_ticket_worker, &state) == 0);
  }
  br_atomic_store_explicit(&start, true, BR_ATOMIC_RELEASE);
  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[(usize)i], NULL) == 0);
  }

  assert(br_atomic_load_explicit(&counter, BR_ATOMIC_RELAXED) == THREAD_COUNT * ITERATIONS);
  assert(br_atomic_load_explicit(&inside, BR_ATOMIC_RELAXED) == 0);
}
#endif

int main(void) {
  test_sync_mutex_and_guard();
  test_sync_recursive_mutex();
#if BR_SYNC_HAS_FUTEX && !defined(_WIN32)
  test_sync_zero_value_public_primitives();
  test_sync_zero_value_cond_signal();
#endif
  test_sync_once_basic();
#if !defined(_WIN32)
  test_sync_cond_signal();
  test_sync_cond_broadcast();
  test_sync_rw_mutex();
  test_sync_wait_group();
  test_sync_barrier();
  test_sync_once_threads();
  test_sync_ticket_mutex();
#endif
  return 0;
}
