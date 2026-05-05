#include <assert.h>

#include <bedrock.h>

static void test_mutex_allocator_default_backing(void) {
  br_mutex_allocator mutex_allocator;
  br_allocator allocator;
  br_alloc_result allocation;

  br_mutex_allocator_init(&mutex_allocator, (br_allocator){0});
  allocator = br_mutex_allocator_allocator(&mutex_allocator);

  allocation = br_allocator_alloc(allocator, 32u, 16u);
  assert(allocation.status == BR_STATUS_OK);
  assert(allocation.ptr != NULL);
  assert(allocation.size == 32u);
  assert(br_allocator_free(allocator, allocation.ptr, allocation.size) == BR_STATUS_OK);
}

static void test_mutex_allocator_forwards_reset(void) {
  static _Alignas(64) u8 storage[256];
  br_arena arena;
  br_mutex_allocator mutex_allocator;
  br_allocator allocator;
  br_alloc_result allocation;

  br_arena_init(&arena, storage, sizeof(storage));
  br_mutex_allocator_init(&mutex_allocator, br_arena_allocator(&arena));
  allocator = br_mutex_allocator_allocator(&mutex_allocator);

  allocation = br_allocator_alloc(allocator, 64u, 16u);
  assert(allocation.status == BR_STATUS_OK);
  assert(arena.offset != 0u);

  assert(br_allocator_reset(allocator) == BR_STATUS_OK);
  assert(arena.offset == 0u);
}

#if !defined(_WIN32)
#include <pthread.h>

typedef struct test_mutex_allocator_backing {
  br_allocator backing;
  br_atomic_i32 inside;
  br_atomic_i32 calls;
} test_mutex_allocator_backing;

typedef struct test_mutex_allocator_thread {
  br_allocator allocator;
  br_atomic_bool *start;
  i32 iterations;
} test_mutex_allocator_thread;

static void test_spin_until_bool(const br_atomic_bool *value, bool expected) {
  while (br_atomic_load_explicit(value, BR_ATOMIC_ACQUIRE) != expected) {
    br_cpu_relax();
  }
}

static br_alloc_result test_mutex_allocator_backing_fn(void *ctx, const br_alloc_request *req) {
  test_mutex_allocator_backing *backing = (test_mutex_allocator_backing *)ctx;
  br_alloc_result result;
  i32 inside;

  assert(backing != NULL);
  inside = br_atomic_add_explicit(&backing->inside, 1, BR_ATOMIC_ACQ_REL);
  assert(inside == 0);
  br_atomic_add_explicit(&backing->calls, 1, BR_ATOMIC_RELAXED);

  for (usize spin = 0u; spin < 64u; ++spin) {
    br_cpu_relax();
  }

  result = br_allocator_call(backing->backing, req);
  assert(br_atomic_sub_explicit(&backing->inside, 1, BR_ATOMIC_ACQ_REL) == 1);
  return result;
}

static void *test_mutex_allocator_worker(void *ctx) {
  test_mutex_allocator_thread *thread = (test_mutex_allocator_thread *)ctx;

  test_spin_until_bool(thread->start, true);
  for (i32 i = 0; i < thread->iterations; ++i) {
    br_alloc_result allocation = br_allocator_alloc_uninit(thread->allocator, 24u, 8u);

    assert(allocation.status == BR_STATUS_OK);
    assert(allocation.ptr != NULL);
    assert(br_allocator_free(thread->allocator, allocation.ptr, allocation.size) == BR_STATUS_OK);
  }

  return NULL;
}

static void test_mutex_allocator_serializes_backing_calls(void) {
  enum { THREAD_COUNT = 4, ITERATIONS = 1000 };
  test_mutex_allocator_backing backing;
  br_allocator checked_backing;
  br_mutex_allocator mutex_allocator;
  br_allocator allocator;
  br_atomic_bool start;
  test_mutex_allocator_thread thread;
  pthread_t threads[THREAD_COUNT];

  backing.backing = br_allocator_heap();
  br_atomic_init(&backing.inside, 0);
  br_atomic_init(&backing.calls, 0);
  checked_backing.fn = test_mutex_allocator_backing_fn;
  checked_backing.ctx = &backing;

  br_mutex_allocator_init(&mutex_allocator, checked_backing);
  allocator = br_mutex_allocator_allocator(&mutex_allocator);
  br_atomic_init(&start, false);
  thread.allocator = allocator;
  thread.start = &start;
  thread.iterations = ITERATIONS;

  for (i32 i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[(usize)i], NULL, test_mutex_allocator_worker, &thread) == 0);
  }

  br_atomic_store_explicit(&start, true, BR_ATOMIC_RELEASE);

  for (i32 i = 0; i < THREAD_COUNT; ++i) {
    assert(pthread_join(threads[(usize)i], NULL) == 0);
  }

  assert(br_atomic_load_explicit(&backing.inside, BR_ATOMIC_ACQUIRE) == 0);
  assert(br_atomic_load_explicit(&backing.calls, BR_ATOMIC_ACQUIRE) ==
         THREAD_COUNT * ITERATIONS * 2);
}
#endif

int main(void) {
  test_mutex_allocator_default_backing();
  test_mutex_allocator_forwards_reset();
#if !defined(_WIN32)
  test_mutex_allocator_serializes_backing_calls();
#endif
  return 0;
}
