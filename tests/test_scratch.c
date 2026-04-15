#include <assert.h>
#include <string.h>

#include <bedrock.h>

static void test_scratch_alloc_and_free_last(void) {
  br_scratch scratch;
  br_alloc_result a;
  br_alloc_result b;

  memset(&scratch, 0, sizeof(scratch));
  assert(br_scratch_init(&scratch, 128u, br_allocator_heap()) == BR_STATUS_OK);

  a = br_scratch_alloc(&scratch, 16u, 8u);
  b = br_scratch_alloc_uninit(&scratch, 24u, 16u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);
  assert(scratch.curr_offset != 0u);

  assert(br_scratch_free(&scratch, b.ptr) == BR_STATUS_OK);
  assert(scratch.prev_allocation == NULL);
  assert(br_scratch_free(&scratch, a.ptr) == BR_STATUS_OK);

  br_scratch_destroy(&scratch);
}

static void test_scratch_non_last_free_is_noop(void) {
  br_scratch scratch;
  br_alloc_result a;
  br_alloc_result b;
  usize before_offset;

  memset(&scratch, 0, sizeof(scratch));
  assert(br_scratch_init(&scratch, 128u, br_allocator_heap()) == BR_STATUS_OK);

  a = br_scratch_alloc(&scratch, 16u, 8u);
  b = br_scratch_alloc(&scratch, 16u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);

  before_offset = scratch.curr_offset;
  assert(br_scratch_free(&scratch, a.ptr) == BR_STATUS_OK);
  assert(scratch.curr_offset == before_offset);
  assert(scratch.prev_allocation == b.ptr);

  br_scratch_destroy(&scratch);
}

static void test_scratch_leaked_allocation(void) {
  br_scratch scratch;
  br_alloc_result a;
  br_alloc_result leak;

  memset(&scratch, 0, sizeof(scratch));
  assert(br_scratch_init(&scratch, 32u, br_allocator_heap()) == BR_STATUS_OK);

  a = br_scratch_alloc(&scratch, 24u, 8u);
  assert(a.status == BR_STATUS_OK);
  leak = br_scratch_alloc(&scratch, 64u, 8u);
  assert(leak.status == BR_STATUS_OK);
  assert(scratch.leaked_count == 1u);
  assert(scratch.leaked_allocations[0].memory == leak.ptr);

  assert(br_scratch_free(&scratch, leak.ptr) == BR_STATUS_OK);
  assert(scratch.leaked_count == 0u);

  br_scratch_destroy(&scratch);
}

static void test_scratch_resize_last_in_place(void) {
  br_scratch scratch;
  br_alloc_result a;
  br_alloc_result grown;

  memset(&scratch, 0, sizeof(scratch));
  assert(br_scratch_init(&scratch, 256u, br_allocator_heap()) == BR_STATUS_OK);

  a = br_scratch_alloc(&scratch, 32u, 16u);
  assert(a.status == BR_STATUS_OK);
  memset(a.ptr, 0x3c, a.size);

  grown = br_scratch_resize(&scratch, a.ptr, a.size, 48u, 16u);
  assert(grown.status == BR_STATUS_OK);
  assert(grown.ptr == a.ptr);
  assert(((u8 *)grown.ptr)[0] == 0x3c);
  assert(((u8 *)grown.ptr)[31] == 0x3c);
  assert(((u8 *)grown.ptr)[32] == 0);

  br_scratch_destroy(&scratch);
}

static void test_scratch_resize_non_last_reallocates(void) {
  br_scratch scratch;
  br_alloc_result a;
  br_alloc_result b;
  br_alloc_result resized;

  memset(&scratch, 0, sizeof(scratch));
  assert(br_scratch_init(&scratch, 256u, br_allocator_heap()) == BR_STATUS_OK);

  a = br_scratch_alloc_uninit(&scratch, 16u, 8u);
  b = br_scratch_alloc_uninit(&scratch, 16u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);
  memset(a.ptr, 0x5a, a.size);

  resized = br_scratch_resize_uninit(&scratch, a.ptr, a.size, 32u, 8u);
  assert(resized.status == BR_STATUS_OK);
  assert(resized.ptr != NULL);
  assert(memcmp(resized.ptr, a.ptr, a.size) == 0);

  br_scratch_destroy(&scratch);
}

static void test_scratch_allocator_lazy_init(void) {
  br_scratch scratch;
  br_allocator allocator;
  br_alloc_result a;

  memset(&scratch, 0, sizeof(scratch));
  allocator = br_scratch_allocator(&scratch);
  a = br_allocator_alloc(allocator, 64u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(scratch.data != NULL);
  assert(scratch.capacity >= BR_SCRATCH_DEFAULT_BACKING_SIZE);

  br_scratch_destroy(&scratch);
}

static void test_scratch_free_all(void) {
  br_scratch scratch;
  br_alloc_result a;
  br_alloc_result leak;

  memset(&scratch, 0, sizeof(scratch));
  assert(br_scratch_init(&scratch, 32u, br_allocator_heap()) == BR_STATUS_OK);

  a = br_scratch_alloc(&scratch, 24u, 8u);
  leak = br_scratch_alloc(&scratch, 64u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(leak.status == BR_STATUS_OK);
  assert(scratch.leaked_count == 1u);

  br_scratch_free_all(&scratch);
  assert(scratch.curr_offset == 0u);
  assert(scratch.prev_allocation == NULL);
  assert(scratch.leaked_count == 0u);

  br_scratch_destroy(&scratch);
}

int main(void) {
  test_scratch_alloc_and_free_last();
  test_scratch_non_last_free_is_noop();
  test_scratch_leaked_allocation();
  test_scratch_resize_last_in_place();
  test_scratch_resize_non_last_reallocates();
  test_scratch_allocator_lazy_init();
  test_scratch_free_all();
  return 0;
}
