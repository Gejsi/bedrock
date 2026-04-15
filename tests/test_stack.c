#include <assert.h>
#include <string.h>

#include <bedrock.h>

static void test_stack_alloc_and_free_last(void) {
  u8 storage[256];
  br_stack stack;
  br_alloc_result a;
  br_alloc_result b;

  br_stack_init(&stack, storage, sizeof(storage));

  a = br_stack_alloc(&stack, 16u, 8u);
  b = br_stack_alloc_uninit(&stack, 24u, 16u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);
  assert(((uptr)b.ptr & 15u) == 0u);

  assert(br_stack_free(&stack, b.ptr) == BR_STATUS_OK);
  assert(br_stack_free(&stack, a.ptr) == BR_STATUS_OK);
  assert(stack.curr_offset == 0u);
  assert(stack.prev_offset == 0u);
}

static void test_stack_out_of_order_free_is_rejected(void) {
  u8 storage[256];
  br_stack stack;
  br_alloc_result a;
  br_alloc_result b;

  br_stack_init(&stack, storage, sizeof(storage));

  a = br_stack_alloc_uninit(&stack, 16u, 8u);
  b = br_stack_alloc_uninit(&stack, 16u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);

  assert(br_stack_free(&stack, a.ptr) == BR_STATUS_INVALID_ARGUMENT);
  assert(br_stack_free(&stack, b.ptr) == BR_STATUS_OK);
  assert(br_stack_free(&stack, b.ptr) == BR_STATUS_OK);
}

static void test_stack_resize_last_in_place(void) {
  u8 storage[256];
  br_stack stack;
  br_alloc_result a;
  br_alloc_result b;
  br_alloc_result grown;
  br_alloc_result shrunk;

  br_stack_init(&stack, storage, sizeof(storage));

  a = br_stack_alloc_uninit(&stack, 16u, 8u);
  b = br_stack_alloc(&stack, 32u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);
  memset(b.ptr, 0x4c, b.size);

  grown = br_stack_resize(&stack, b.ptr, b.size, 48u, 8u);
  assert(grown.status == BR_STATUS_OK);
  assert(grown.ptr == b.ptr);
  assert(((u8 *)grown.ptr)[31] == 0x4c);
  assert(((u8 *)grown.ptr)[32] == 0u);

  shrunk = br_stack_resize_uninit(&stack, grown.ptr, grown.size, 20u, 8u);
  assert(shrunk.status == BR_STATUS_OK);
  assert(shrunk.ptr == b.ptr);
}

static void test_stack_resize_non_last_reallocates(void) {
  u8 storage[320];
  br_stack stack;
  br_alloc_result a;
  br_alloc_result b;
  br_alloc_result resized;
  u8 expected[16];

  br_stack_init(&stack, storage, sizeof(storage));

  a = br_stack_alloc_uninit(&stack, 16u, 8u);
  b = br_stack_alloc_uninit(&stack, 24u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);
  memset(a.ptr, 0x5a, a.size);
  memset(expected, 0x5a, sizeof(expected));

  resized = br_stack_resize_uninit(&stack, a.ptr, a.size, 40u, 8u);
  assert(resized.status == BR_STATUS_OK);
  assert(resized.ptr != NULL);
  assert(resized.ptr != a.ptr);
  assert(memcmp(resized.ptr, expected, sizeof(expected)) == 0);
}

static void test_stack_allocator_reset(void) {
  u8 storage[256];
  br_stack stack;
  br_allocator allocator;
  br_alloc_result a;
  br_alloc_result b;
  br_alloc_result c;

  br_stack_init(&stack, storage, sizeof(storage));
  allocator = br_stack_allocator(&stack);

  a = br_allocator_alloc(allocator, 32u, 8u);
  b = br_allocator_alloc(allocator, 48u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);

  assert(br_allocator_reset(allocator) == BR_STATUS_OK);
  c = br_allocator_alloc(allocator, 24u, 8u);
  assert(c.status == BR_STATUS_OK);
  assert(c.ptr == a.ptr);
}

int main(void) {
  test_stack_alloc_and_free_last();
  test_stack_out_of_order_free_is_rejected();
  test_stack_resize_last_in_place();
  test_stack_resize_non_last_reallocates();
  test_stack_allocator_reset();
  return 0;
}
