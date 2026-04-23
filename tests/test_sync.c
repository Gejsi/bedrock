#include <assert.h>
#include <stdatomic.h>

#include <bedrock.h>

static void test_once_increment(void *ctx) {
  atomic_fetch_add_explicit((atomic_int *)ctx, 1, memory_order_relaxed);
}

#if !defined(_WIN32)
#include <pthread.h>

typedef struct test_signal_state {
  br_mutex mutex;
  br_cond cond;
  bool ready;
  int seen;
} test_signal_state;

typedef struct test_rw_reader_state {
  br_rw_mutex *mutex;
  atomic_bool entered;
  atomic_bool release;
} test_rw_reader_state;

typedef struct test_wait_group_state {
  br_wait_group *wg;
  atomic_int *counter;
} test_wait_group_state;

typedef struct test_barrier_state {
  br_barrier *barrier;
  atomic_int *leaders;
  atomic_int *after;
} test_barrier_state;

typedef struct test_once_state {
  br_once *once;
  atomic_int *count;
} test_once_state;

typedef struct test_ticket_state {
  br_ticket_mutex *mutex;
  atomic_int *counter;
  int iterations;
} test_ticket_state;

static void *test_cond_waiter(void *ctx) {
  test_signal_state *state = (test_signal_state *)ctx;

  br_mutex_lock(&state->mutex);
  while (!state->ready) {
    br_cond_wait(&state->cond, &state->mutex);
  }
  state->seen += 1;
  br_mutex_unlock(&state->mutex);
  return NULL;
}

static void *test_rw_reader(void *ctx) {
  test_rw_reader_state *state = (test_rw_reader_state *)ctx;

  br_rw_mutex_shared_lock(state->mutex);
  atomic_store_explicit(&state->entered, true, memory_order_release);
  while (!atomic_load_explicit(&state->release, memory_order_acquire)) {
  }
  br_rw_mutex_shared_unlock(state->mutex);
  return NULL;
}

static void *test_wait_group_worker(void *ctx) {
  test_wait_group_state *state = (test_wait_group_state *)ctx;

  atomic_fetch_add_explicit(state->counter, 1, memory_order_relaxed);
  br_wait_group_done(state->wg);
  return NULL;
}

static void *test_barrier_worker(void *ctx) {
  test_barrier_state *state = (test_barrier_state *)ctx;

  if (br_barrier_wait(state->barrier)) {
    atomic_fetch_add_explicit(state->leaders, 1, memory_order_relaxed);
  }
  atomic_fetch_add_explicit(state->after, 1, memory_order_relaxed);
  return NULL;
}

static void *test_once_worker(void *ctx) {
  test_once_state *state = (test_once_state *)ctx;

  br_once_do(state->once, test_once_increment, state->count);
  return NULL;
}

static void *test_ticket_worker(void *ctx) {
  test_ticket_state *state = (test_ticket_state *)ctx;
  int i;

  for (i = 0; i < state->iterations; ++i) {
    br_ticket_mutex_lock(state->mutex);
    atomic_fetch_add_explicit(state->counter, 1, memory_order_relaxed);
    br_ticket_mutex_unlock(state->mutex);
  }
  return NULL;
}
#endif

static void test_sync_mutex_and_guard(void) {
  br_mutex mutex;
  br_status status;
  int value = 0;

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

static void test_sync_once_basic(void) {
  br_once once;
  atomic_int count;

  assert(br_once_init(&once) == BR_STATUS_OK);
  atomic_init(&count, 0);

  br_once_do(&once, test_once_increment, &count);
  br_once_do(&once, test_once_increment, &count);

  assert(atomic_load_explicit(&count, memory_order_relaxed) == 1);
  br_once_destroy(&once);
}

#if !defined(_WIN32)
static void test_sync_cond(void) {
  test_signal_state state;
  pthread_t thread;

  assert(br_mutex_init(&state.mutex) == BR_STATUS_OK);
  assert(br_cond_init(&state.cond) == BR_STATUS_OK);
  state.ready = false;
  state.seen = 0;

  assert(pthread_create(&thread, NULL, test_cond_waiter, &state) == 0);

  br_mutex_lock(&state.mutex);
  state.ready = true;
  br_cond_signal(&state.cond);
  br_mutex_unlock(&state.mutex);

  assert(pthread_join(thread, NULL) == 0);
  assert(state.seen == 1);

  br_cond_destroy(&state.cond);
  br_mutex_destroy(&state.mutex);
}

static void test_sync_rw_mutex(void) {
  br_rw_mutex mutex;
  test_rw_reader_state state;
  pthread_t thread;

  assert(br_rw_mutex_init(&mutex) == BR_STATUS_OK);
  state.mutex = &mutex;
  atomic_init(&state.entered, false);
  atomic_init(&state.release, false);

  assert(pthread_create(&thread, NULL, test_rw_reader, &state) == 0);
  while (!atomic_load_explicit(&state.entered, memory_order_acquire)) {
  }

  assert(!br_rw_mutex_try_lock(&mutex));
  atomic_store_explicit(&state.release, true, memory_order_release);
  assert(pthread_join(thread, NULL) == 0);
  assert(br_rw_mutex_try_lock(&mutex));
  br_rw_mutex_unlock(&mutex);

  br_rw_mutex_destroy(&mutex);
}

static void test_sync_wait_group(void) {
  enum { THREAD_COUNT = 4 };
  br_wait_group wg;
  test_wait_group_state state;
  pthread_t threads[THREAD_COUNT];
  atomic_int counter;
  int i;

  assert(br_wait_group_init(&wg) == BR_STATUS_OK);
  atomic_init(&counter, 0);
  state.wg = &wg;
  state.counter = &counter;

  br_wait_group_add(&wg, THREAD_COUNT);
  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[i], NULL, test_wait_group_worker, &state) == 0);
  }

  br_wait_group_wait(&wg);
  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[i], NULL) == 0);
  }

  assert(atomic_load_explicit(&counter, memory_order_relaxed) == THREAD_COUNT);
  br_wait_group_destroy(&wg);
}

static void test_sync_barrier(void) {
  enum { THREAD_COUNT = 2 };
  br_barrier barrier;
  test_barrier_state state;
  pthread_t threads[THREAD_COUNT];
  atomic_int leaders;
  atomic_int after;
  int i;

  assert(br_barrier_init(&barrier, THREAD_COUNT + 1) == BR_STATUS_OK);
  atomic_init(&leaders, 0);
  atomic_init(&after, 0);
  state.barrier = &barrier;
  state.leaders = &leaders;
  state.after = &after;

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[i], NULL, test_barrier_worker, &state) == 0);
  }

  if (br_barrier_wait(&barrier)) {
    atomic_fetch_add_explicit(&leaders, 1, memory_order_relaxed);
  }
  atomic_fetch_add_explicit(&after, 1, memory_order_relaxed);

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[i], NULL) == 0);
  }

  assert(atomic_load_explicit(&leaders, memory_order_relaxed) == 1);
  assert(atomic_load_explicit(&after, memory_order_relaxed) == THREAD_COUNT + 1);
  br_barrier_destroy(&barrier);
}

static void test_sync_once_threads(void) {
  enum { THREAD_COUNT = 6 };
  br_once once;
  test_once_state state;
  pthread_t threads[THREAD_COUNT];
  atomic_int count;
  int i;

  assert(br_once_init(&once) == BR_STATUS_OK);
  atomic_init(&count, 0);
  state.once = &once;
  state.count = &count;

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[i], NULL, test_once_worker, &state) == 0);
  }
  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[i], NULL) == 0);
  }

  assert(atomic_load_explicit(&count, memory_order_relaxed) == 1);
  br_once_destroy(&once);
}

static void test_sync_ticket_mutex(void) {
  enum { THREAD_COUNT = 4, ITERATIONS = 1000 };
  br_ticket_mutex mutex;
  test_ticket_state state;
  pthread_t threads[THREAD_COUNT];
  atomic_int counter;
  int i;

  br_ticket_mutex_init(&mutex);
  atomic_init(&counter, 0);
  state.mutex = &mutex;
  state.counter = &counter;
  state.iterations = ITERATIONS;

  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[i], NULL, test_ticket_worker, &state) == 0);
  }
  for (i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[i], NULL) == 0);
  }

  assert(atomic_load_explicit(&counter, memory_order_relaxed) == THREAD_COUNT * ITERATIONS);
}
#endif

int main(void) {
  test_sync_mutex_and_guard();
  test_sync_recursive_mutex();
  test_sync_once_basic();
#if !defined(_WIN32)
  test_sync_cond();
  test_sync_rw_mutex();
  test_sync_wait_group();
  test_sync_barrier();
  test_sync_once_threads();
  test_sync_ticket_mutex();
#endif
  return 0;
}
