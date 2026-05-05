#include <assert.h>

#include <bedrock.h>

static void test_tracking_allocator_stats(void) {
  br_tracking_allocator tracking;
  br_allocator allocator;
  br_alloc_result a;
  br_alloc_result b;

  br_tracking_allocator_init(&tracking, br_allocator_heap(), br_allocator_heap());
  allocator = br_tracking_allocator_allocator(&tracking);

  a = br_allocator_alloc(allocator, 16u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(tracking.stats.total_memory_allocated == 16u);
  assert(tracking.stats.total_allocation_count == 1u);
  assert(tracking.stats.current_memory_allocated == 16u);
  assert(tracking.stats.live_allocation_count == 1u);
  assert(tracking.stats.peak_memory_allocated == 16u);

  b = br_allocator_resize(allocator, a.ptr, 16u, 40u, 8u);
  assert(b.status == BR_STATUS_OK);
  assert(tracking.stats.total_memory_allocated == 56u);
  assert(tracking.stats.total_allocation_count == 2u);
  assert(tracking.stats.total_memory_freed == 16u);
  assert(tracking.stats.total_free_count == 1u);
  assert(tracking.stats.current_memory_allocated == 40u);
  assert(tracking.stats.live_allocation_count == 1u);
  assert(tracking.stats.peak_memory_allocated == 40u);

  assert(br_allocator_free(allocator, b.ptr, 40u) == BR_STATUS_OK);
  assert(tracking.stats.total_memory_freed == 56u);
  assert(tracking.stats.total_free_count == 2u);
  assert(tracking.stats.current_memory_allocated == 0u);
  assert(tracking.stats.live_allocation_count == 0u);

  br_tracking_allocator_destroy(&tracking);
}

static void test_tracking_allocator_bad_free(void) {
  br_tracking_allocator tracking;
  br_allocator allocator;
  br_alloc_result foreign;

  br_tracking_allocator_init(&tracking, br_allocator_heap(), br_allocator_heap());
  allocator = br_tracking_allocator_allocator(&tracking);

  foreign = br_allocator_alloc(br_allocator_heap(), 24u, 8u);
  assert(foreign.status == BR_STATUS_OK);
  assert(br_allocator_free(allocator, foreign.ptr, 24u) == BR_STATUS_INVALID_ARGUMENT);
  assert(tracking.bad_free_count == 1u);
  assert(tracking.bad_frees[0].memory == foreign.ptr);
  assert(tracking.bad_frees[0].size == 24u);

  assert(br_allocator_free(br_allocator_heap(), foreign.ptr, 24u) == BR_STATUS_OK);
  br_tracking_allocator_destroy(&tracking);
}

static void test_tracking_allocator_clear_on_reset(void) {
  u8 storage[128];
  br_arena arena;
  br_tracking_allocator tracking;
  br_allocator allocator;
  br_alloc_result a;
  br_alloc_result b;

  br_arena_init(&arena, storage, sizeof(storage));
  br_tracking_allocator_init(&tracking, br_arena_allocator(&arena), br_allocator_heap());
  tracking.clear_on_reset = true;
  allocator = br_tracking_allocator_allocator(&tracking);

  a = br_allocator_alloc(allocator, 16u, 8u);
  b = br_allocator_alloc(allocator, 24u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);
  assert(tracking.stats.current_memory_allocated == 40u);
  assert(tracking.stats.live_allocation_count == 2u);

  assert(br_allocator_reset(allocator) == BR_STATUS_OK);
  assert(tracking.stats.current_memory_allocated == 0u);
  assert(tracking.stats.live_allocation_count == 0u);
  assert(tracking.stats.total_memory_allocated == 40u);

  br_tracking_allocator_destroy(&tracking);
}

static void test_tracking_allocator_clear_and_reset(void) {
  br_tracking_allocator tracking;
  br_allocator allocator;
  br_alloc_result a;

  br_tracking_allocator_init(&tracking, br_allocator_heap(), br_allocator_heap());
  allocator = br_tracking_allocator_allocator(&tracking);

  a = br_allocator_alloc(allocator, 32u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(br_allocator_free(allocator, a.ptr, 32u) == BR_STATUS_OK);

  br_tracking_allocator_clear(&tracking);
  assert(tracking.bad_free_count == 0u);
  assert(tracking.entry_count == 0u);
  assert(tracking.stats.total_memory_allocated == 32u);
  assert(tracking.stats.total_free_count == 1u);

  br_tracking_allocator_reset(&tracking);
  assert(tracking.entry_count == 0u);
  assert(tracking.bad_free_count == 0u);
  assert(tracking.stats.total_memory_allocated == 0u);
  assert(tracking.stats.total_free_count == 0u);

  br_tracking_allocator_destroy(&tracking);
}

static void test_tracking_allocator_index_updates(void) {
  br_tracking_allocator tracking;
  br_allocator allocator;
  br_alloc_result a;
  br_alloc_result b;
  br_alloc_result c;
  br_alloc_result resized_c;

  br_tracking_allocator_init(&tracking, br_allocator_heap(), br_allocator_heap());
  allocator = br_tracking_allocator_allocator(&tracking);

  a = br_allocator_alloc(allocator, 8u, 8u);
  b = br_allocator_alloc(allocator, 16u, 8u);
  c = br_allocator_alloc(allocator, 24u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);
  assert(c.status == BR_STATUS_OK);

  assert(br_allocator_free(allocator, a.ptr, a.size) == BR_STATUS_OK);
  resized_c = br_allocator_resize(allocator, c.ptr, c.size, 48u, 8u);
  assert(resized_c.status == BR_STATUS_OK);
  assert(br_allocator_free(allocator, b.ptr, b.size) == BR_STATUS_OK);
  assert(br_allocator_free(allocator, resized_c.ptr, resized_c.size) == BR_STATUS_OK);
  assert(tracking.stats.current_memory_allocated == 0u);
  assert(tracking.stats.live_allocation_count == 0u);

  br_tracking_allocator_destroy(&tracking);
}

static void test_tracking_allocator_many_allocations(void) {
  br_tracking_allocator tracking;
  br_allocator allocator;
  br_alloc_result allocs[96];
  usize i;

  br_tracking_allocator_init(&tracking, br_allocator_heap(), br_allocator_heap());
  allocator = br_tracking_allocator_allocator(&tracking);

  for (i = 0u; i < BR_ARRAY_COUNT(allocs); ++i) {
    allocs[i] = br_allocator_alloc(allocator, (i % 11u) + 1u, 8u);
    assert(allocs[i].status == BR_STATUS_OK);
  }

  for (i = 0u; i < BR_ARRAY_COUNT(allocs); i += 2u) {
    assert(br_allocator_free(allocator, allocs[i].ptr, allocs[i].size) == BR_STATUS_OK);
  }
  for (i = 1u; i < BR_ARRAY_COUNT(allocs); i += 2u) {
    assert(br_allocator_free(allocator, allocs[i].ptr, allocs[i].size) == BR_STATUS_OK);
  }

  assert(tracking.entry_count == 0u);
  assert(tracking.stats.current_memory_allocated == 0u);
  assert(tracking.stats.live_allocation_count == 0u);

  br_tracking_allocator_destroy(&tracking);
}

#if !defined(_WIN32)
#include <pthread.h>

#define TEST_TRACKING_THREAD_COUNT 4u
#define TEST_TRACKING_ITERATIONS 1000u

typedef struct test_tracking_allocator_thread {
  br_allocator allocator;
  br_atomic_bool *start;
} test_tracking_allocator_thread;

static void test_tracking_spin_until_bool(const br_atomic_bool *value, bool expected) {
  while (br_atomic_load_explicit(value, BR_ATOMIC_ACQUIRE) != expected) {
    br_cpu_relax();
  }
}

static usize test_tracking_iteration_size(usize iteration) {
  return (iteration % 31u) + 1u;
}

static usize test_tracking_expected_bytes_per_thread(void) {
  usize total = 0u;

  for (usize i = 0u; i < TEST_TRACKING_ITERATIONS; ++i) {
    total += test_tracking_iteration_size(i);
  }

  return total;
}

static void *test_tracking_allocator_worker(void *ctx) {
  test_tracking_allocator_thread *thread = (test_tracking_allocator_thread *)ctx;

  test_tracking_spin_until_bool(thread->start, true);
  for (usize i = 0u; i < TEST_TRACKING_ITERATIONS; ++i) {
    usize size = test_tracking_iteration_size(i);
    br_alloc_result allocation = br_allocator_alloc_uninit(thread->allocator, size, 8u);

    assert(allocation.status == BR_STATUS_OK);
    assert(allocation.ptr != NULL);
    assert(br_allocator_free(thread->allocator, allocation.ptr, allocation.size) == BR_STATUS_OK);
  }

  return NULL;
}

static void test_tracking_allocator_serializes_state(void) {
  br_tracking_allocator tracking;
  br_allocator allocator;
  br_atomic_bool start;
  test_tracking_allocator_thread thread;
  pthread_t threads[TEST_TRACKING_THREAD_COUNT];
  usize expected_bytes = test_tracking_expected_bytes_per_thread() * TEST_TRACKING_THREAD_COUNT;

  br_tracking_allocator_init(&tracking, br_allocator_heap(), br_allocator_heap());
  allocator = br_tracking_allocator_allocator(&tracking);
  br_atomic_init(&start, false);
  thread.allocator = allocator;
  thread.start = &start;

  for (usize i = 0u; i < TEST_TRACKING_THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[i], NULL, test_tracking_allocator_worker, &thread) == 0);
  }

  br_atomic_store_explicit(&start, true, BR_ATOMIC_RELEASE);

  for (usize i = 0u; i < TEST_TRACKING_THREAD_COUNT; ++i) {
    assert(pthread_join(threads[i], NULL) == 0);
  }

  assert(tracking.entry_count == 0u);
  assert(tracking.bad_free_count == 0u);
  assert(tracking.stats.total_memory_allocated == expected_bytes);
  assert(tracking.stats.total_memory_freed == expected_bytes);
  assert(tracking.stats.total_allocation_count ==
         TEST_TRACKING_THREAD_COUNT * TEST_TRACKING_ITERATIONS);
  assert(tracking.stats.total_free_count == TEST_TRACKING_THREAD_COUNT * TEST_TRACKING_ITERATIONS);
  assert(tracking.stats.current_memory_allocated == 0u);
  assert(tracking.stats.live_allocation_count == 0u);

  br_tracking_allocator_destroy(&tracking);
}
#endif

int main(void) {
  test_tracking_allocator_stats();
  test_tracking_allocator_bad_free();
  test_tracking_allocator_clear_on_reset();
  test_tracking_allocator_clear_and_reset();
  test_tracking_allocator_index_updates();
  test_tracking_allocator_many_allocations();
#if !defined(_WIN32)
  test_tracking_allocator_serializes_state();
#endif
  return 0;
}
