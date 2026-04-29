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

typedef struct atomic_mutex_state {
  br_atomic_mutex *mutex;
  br_atomic_bool *start;
  br_atomic_i32 *inside;
  br_atomic_i32 *counter;
  i32 iterations;
} atomic_mutex_state;

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
#endif

int main(void) {
#if BR_SYNC_HAS_FUTEX && !defined(_WIN32)
  test_futex_wait_signal();
  test_atomic_mutex_try_lock();
  test_atomic_mutex_contention();
  test_atomic_sema_wait_post_one();
  test_atomic_sema_post_many();
#endif
  return 0;
}
