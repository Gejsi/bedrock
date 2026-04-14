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

int main(void) {
  test_tracking_allocator_stats();
  test_tracking_allocator_bad_free();
  test_tracking_allocator_clear_on_reset();
  test_tracking_allocator_clear_and_reset();
  return 0;
}
