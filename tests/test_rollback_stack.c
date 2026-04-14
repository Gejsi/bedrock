#include <assert.h>

#include <bedrock.h>

static void test_rollback_stack_buffered_tail_collapse(void) {
  u8 storage[512];
  br_rollback_stack stack;
  br_alloc_result a;
  br_alloc_result b;
  br_alloc_result c;
  br_alloc_result d;

  assert(br_rollback_stack_init_buffered(&stack, storage, sizeof(storage)) == BR_STATUS_OK);

  a = br_rollback_stack_alloc_uninit(&stack, 16u, 8u);
  b = br_rollback_stack_alloc_uninit(&stack, 24u, 8u);
  c = br_rollback_stack_alloc_uninit(&stack, 32u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);
  assert(c.status == BR_STATUS_OK);

  assert(br_rollback_stack_free(&stack, b.ptr) == BR_STATUS_OK);
  assert(br_rollback_stack_free(&stack, c.ptr) == BR_STATUS_OK);

  d = br_rollback_stack_alloc_uninit(&stack, 24u, 8u);
  assert(d.status == BR_STATUS_OK);
  assert(d.ptr == b.ptr);

  br_rollback_stack_destroy(&stack);
}

static void test_rollback_stack_resize_last_in_place(void) {
  u8 storage[512];
  br_rollback_stack stack;
  br_alloc_result a;
  br_alloc_result b;
  br_alloc_result resized;

  assert(br_rollback_stack_init_buffered(&stack, storage, sizeof(storage)) == BR_STATUS_OK);

  a = br_rollback_stack_alloc_uninit(&stack, 16u, 8u);
  b = br_rollback_stack_alloc_uninit(&stack, 32u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);

  resized = br_rollback_stack_resize_uninit(&stack, b.ptr, 32u, 64u, 8u);
  assert(resized.status == BR_STATUS_OK);
  assert(resized.ptr == b.ptr);

  resized = br_rollback_stack_resize_uninit(&stack, resized.ptr, 64u, 20u, 8u);
  assert(resized.status == BR_STATUS_OK);
  assert(resized.ptr == b.ptr);

  br_rollback_stack_destroy(&stack);
}

static void test_rollback_stack_resize_non_last_falls_back(void) {
  u8 storage[640];
  br_rollback_stack stack;
  br_alloc_result a;
  br_alloc_result b;
  br_alloc_result resized;
  u8 expected[16];

  assert(br_rollback_stack_init_buffered(&stack, storage, sizeof(storage)) == BR_STATUS_OK);

  a = br_rollback_stack_alloc_uninit(&stack, 16u, 8u);
  b = br_rollback_stack_alloc_uninit(&stack, 24u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);

  memset(a.ptr, 0x5a, a.size);
  memset(expected, 0x5a, sizeof(expected));
  resized = br_rollback_stack_resize_uninit(&stack, a.ptr, a.size, 40u, 8u);
  assert(resized.status == BR_STATUS_OK);
  assert(resized.ptr != a.ptr);
  assert(memcmp(resized.ptr, expected, sizeof(expected)) == 0);

  br_rollback_stack_destroy(&stack);
}

static void test_rollback_stack_dynamic_singleton_block(void) {
  br_rollback_stack stack;
  br_alloc_result small;
  br_alloc_result large;
  br_alloc_result after;

  assert(br_rollback_stack_init_dynamic(&stack, 128u, br_allocator_heap()) == BR_STATUS_OK);

  small = br_rollback_stack_alloc_uninit(&stack, 16u, 8u);
  large = br_rollback_stack_alloc_uninit(&stack, 512u, 8u);
  assert(small.status == BR_STATUS_OK);
  assert(large.status == BR_STATUS_OK);

  assert(br_rollback_stack_free(&stack, large.ptr) == BR_STATUS_OK);
  after = br_rollback_stack_alloc_uninit(&stack, 24u, 8u);
  assert(after.status == BR_STATUS_OK);

  br_rollback_stack_destroy(&stack);
}

static void test_rollback_stack_allocator_reset_and_double_free(void) {
  br_rollback_stack stack;
  br_allocator allocator;
  br_alloc_result a;
  br_alloc_result b;

  assert(br_rollback_stack_init_dynamic(&stack, 256u, br_allocator_heap()) == BR_STATUS_OK);
  allocator = br_rollback_stack_allocator(&stack);

  a = br_allocator_alloc(allocator, 32u, 8u);
  b = br_allocator_alloc(allocator, 64u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);

  assert(br_allocator_free(allocator, b.ptr, b.size) == BR_STATUS_OK);
  assert(br_allocator_free(allocator, b.ptr, b.size) == BR_STATUS_INVALID_ARGUMENT);

  assert(br_allocator_reset(allocator) == BR_STATUS_OK);
  a = br_allocator_alloc(allocator, 32u, 8u);
  assert(a.status == BR_STATUS_OK);

  br_rollback_stack_destroy(&stack);
}

int main(void) {
  test_rollback_stack_buffered_tail_collapse();
  test_rollback_stack_resize_last_in_place();
  test_rollback_stack_resize_non_last_falls_back();
  test_rollback_stack_dynamic_singleton_block();
  test_rollback_stack_allocator_reset_and_double_free();
  return 0;
}
