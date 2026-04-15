#include <assert.h>
#include <string.h>

#include <bedrock.h>

static void test_small_stack_out_of_order_free_rewinds_offset(void) {
  u8 storage[256];
  br_small_stack stack;
  br_alloc_result a;
  br_alloc_result b;
  br_alloc_result c;
  br_alloc_result d;

  br_small_stack_init(&stack, storage, sizeof(storage));

  a = br_small_stack_alloc_uninit(&stack, 16u, 8u);
  b = br_small_stack_alloc_uninit(&stack, 24u, 8u);
  c = br_small_stack_alloc_uninit(&stack, 32u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);
  assert(c.status == BR_STATUS_OK);

  assert(br_small_stack_free(&stack, b.ptr) == BR_STATUS_OK);
  d = br_small_stack_alloc_uninit(&stack, 20u, 8u);
  assert(d.status == BR_STATUS_OK);
  assert(d.ptr == b.ptr);
}

static void test_small_stack_resize_reallocates_and_copies(void) {
  u8 storage[256];
  br_small_stack stack;
  br_alloc_result a;
  br_alloc_result resized;
  u8 expected[16];

  br_small_stack_init(&stack, storage, sizeof(storage));

  a = br_small_stack_alloc_uninit(&stack, 16u, 8u);
  assert(a.status == BR_STATUS_OK);
  memset(a.ptr, 0x29, a.size);
  memset(expected, 0x29, sizeof(expected));

  resized = br_small_stack_resize(&stack, a.ptr, a.size, 32u, 8u);
  assert(resized.status == BR_STATUS_OK);
  assert(resized.ptr != NULL);
  assert(resized.ptr != a.ptr);
  assert(memcmp(resized.ptr, expected, sizeof(expected)) == 0);
  assert(((u8 *)resized.ptr)[16] == 0u);
}

static void test_small_stack_double_free_and_reset(void) {
  u8 storage[256];
  br_small_stack stack;
  br_allocator allocator;
  br_alloc_result a;
  br_alloc_result b;

  br_small_stack_init(&stack, storage, sizeof(storage));
  allocator = br_small_stack_allocator(&stack);

  a = br_allocator_alloc(allocator, 16u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(br_allocator_free(allocator, a.ptr, a.size) == BR_STATUS_OK);
  assert(br_allocator_free(allocator, a.ptr, a.size) == BR_STATUS_OK);

  b = br_allocator_alloc(allocator, 24u, 8u);
  assert(b.status == BR_STATUS_OK);
  assert(br_allocator_reset(allocator) == BR_STATUS_OK);
  b = br_allocator_alloc(allocator, 24u, 8u);
  assert(b.status == BR_STATUS_OK);
  assert(b.ptr == a.ptr);
}

int main(void) {
  test_small_stack_out_of_order_free_rewinds_offset();
  test_small_stack_resize_reallocates_and_copies();
  test_small_stack_double_free_and_reset();
  return 0;
}
