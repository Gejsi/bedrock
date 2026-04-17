#include <assert.h>
#include <string.h>

#include <bedrock.h>

static void test_dynamic_arena_basic_resize(void) {
  br_dynamic_arena arena;
  br_alloc_result a;
  br_alloc_result grown;

  memset(&arena, 0, sizeof(arena));
  assert(br_dynamic_arena_init(&arena, br_allocator_heap(), br_allocator_heap(), 64u, 48u, 8u) ==
         BR_STATUS_OK);

  a = br_dynamic_arena_alloc(&arena, 16u);
  assert(a.status == BR_STATUS_OK);
  assert(a.ptr != NULL);
  memset(a.ptr, 0x37, a.size);

  grown = br_dynamic_arena_resize(&arena, a.ptr, a.size, 32u);
  assert(grown.status == BR_STATUS_OK);
  assert(grown.ptr != NULL);
  assert(memcmp(grown.ptr, a.ptr, a.size) == 0);
  assert(((u8 *)grown.ptr)[16] == 0u);

  br_dynamic_arena_destroy(&arena);
}

static void test_dynamic_arena_reset_reuses_blocks(void) {
  br_tracking_allocator block_tracking;
  br_tracking_allocator array_tracking;
  br_dynamic_arena arena;
  br_alloc_result a;
  br_alloc_result b;
  br_alloc_result c;

  memset(&block_tracking, 0, sizeof(block_tracking));
  memset(&array_tracking, 0, sizeof(array_tracking));
  memset(&arena, 0, sizeof(arena));

  br_tracking_allocator_init(&block_tracking, br_allocator_heap(), br_allocator_heap());
  br_tracking_allocator_init(&array_tracking, br_allocator_heap(), br_allocator_heap());

  assert(br_dynamic_arena_init(&arena,
                               br_tracking_allocator_allocator(&block_tracking),
                               br_tracking_allocator_allocator(&array_tracking),
                               64u,
                               128u,
                               8u) == BR_STATUS_OK);

  a = br_dynamic_arena_alloc_uninit(&arena, 48u);
  b = br_dynamic_arena_alloc_uninit(&arena, 24u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);
  assert(block_tracking.stats.live_allocation_count == 2u);

  br_dynamic_arena_reset(&arena);
  assert(arena.current_block == NULL);
  assert(arena.used_count == 0u);
  assert(arena.unused_count == 2u);
  assert(block_tracking.stats.live_allocation_count == 2u);

  c = br_dynamic_arena_alloc_uninit(&arena, 24u);
  assert(c.status == BR_STATUS_OK);
  assert(block_tracking.stats.total_allocation_count == 2u);

  br_dynamic_arena_destroy(&arena);
  assert(block_tracking.stats.live_allocation_count == 0u);
  assert(array_tracking.stats.live_allocation_count == 0u);
  br_tracking_allocator_destroy(&block_tracking);
  br_tracking_allocator_destroy(&array_tracking);
}

static void test_dynamic_arena_out_band_and_allocator_adapter(void) {
  br_tracking_allocator block_tracking;
  br_tracking_allocator array_tracking;
  br_dynamic_arena arena;
  br_allocator allocator;
  br_alloc_result out_band;
  br_alloc_result via_allocator;

  memset(&block_tracking, 0, sizeof(block_tracking));
  memset(&array_tracking, 0, sizeof(array_tracking));
  memset(&arena, 0, sizeof(arena));

  br_tracking_allocator_init(&block_tracking, br_allocator_heap(), br_allocator_heap());
  br_tracking_allocator_init(&array_tracking, br_allocator_heap(), br_allocator_heap());

  assert(br_dynamic_arena_init(&arena,
                               br_tracking_allocator_allocator(&block_tracking),
                               br_tracking_allocator_allocator(&array_tracking),
                               64u,
                               48u,
                               8u) == BR_STATUS_OK);

  out_band = br_dynamic_arena_alloc_uninit(&arena, 64u);
  assert(out_band.status == BR_STATUS_OK);
  assert(out_band.ptr != NULL);
  assert(arena.out_band_count == 1u);

  allocator = br_dynamic_arena_allocator(&arena);
  via_allocator = br_allocator_alloc(allocator, 16u, 32u);
  assert(via_allocator.status == BR_STATUS_OK);
  assert(via_allocator.ptr != NULL);
  assert(br_allocator_free(allocator, via_allocator.ptr, via_allocator.size) ==
         BR_STATUS_NOT_SUPPORTED);
  assert(br_allocator_reset(allocator) == BR_STATUS_OK);

  assert(block_tracking.stats.live_allocation_count == 0u);
  assert(arena.current_block == NULL);
  assert(arena.out_band_count == 0u);

  br_dynamic_arena_destroy(&arena);
  assert(array_tracking.stats.live_allocation_count == 0u);
  br_tracking_allocator_destroy(&block_tracking);
  br_tracking_allocator_destroy(&array_tracking);
}

int main(void) {
  test_dynamic_arena_basic_resize();
  test_dynamic_arena_reset_reuses_blocks();
  test_dynamic_arena_out_band_and_allocator_adapter();
  return 0;
}
