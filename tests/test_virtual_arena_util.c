#include <assert.h>

#include <bedrock.h>

typedef struct test_virtual_arena_util_item {
  u32 a;
  u32 b;
} test_virtual_arena_util_item;

static void test_virtual_arena_util_new_and_clone(void) {
  br_virtual_arena arena;
  test_virtual_arena_util_item *item = NULL;
  test_virtual_arena_util_item *clone = NULL;
  br_status status;

  br_virtual_arena_init(&arena);
  assert(br_virtual_arena_init_static(&arena, 65536u, 4096u) == BR_STATUS_OK);

  status = br_virtual_arena_new(&arena, test_virtual_arena_util_item, item);
  assert(status == BR_STATUS_OK);
  assert(item != NULL);
  assert(item->a == 0u);
  assert(item->b == 0u);

  status = br_virtual_arena_new_clone(
    &arena, test_virtual_arena_util_item, ((test_virtual_arena_util_item){11u, 29u}), clone);
  assert(status == BR_STATUS_OK);
  assert(clone != NULL);
  assert(clone->a == 11u);
  assert(clone->b == 29u);

  br_virtual_arena_destroy(&arena);
}

static void test_virtual_arena_util_alignment_and_make(void) {
  br_virtual_arena arena;
  u64 *aligned = NULL;
  u16 *slice = NULL;
  usize len = 0u;
  br_status status;
  usize i;

  br_virtual_arena_init(&arena);
  assert(br_virtual_arena_init_static(&arena, 65536u, 4096u) == BR_STATUS_OK);

  status = br_virtual_arena_new_aligned(&arena, u64, 64u, aligned);
  assert(status == BR_STATUS_OK);
  assert(aligned != NULL);
  assert((((uptr)(void *)aligned) & 63u) == 0u);

  status = br_virtual_arena_make_slice(&arena, u16, 8u, slice, len);
  assert(status == BR_STATUS_OK);
  assert(slice != NULL);
  assert(len == 8u);
  for (i = 0u; i < len; ++i) {
    assert(slice[i] == 0u);
  }

  br_virtual_arena_destroy(&arena);
}

static void test_virtual_arena_util_make_multi_pointer(void) {
  br_virtual_arena arena;
  test_virtual_arena_util_item *items = NULL;
  br_status status;

  br_virtual_arena_init(&arena);
  assert(br_virtual_arena_init_growing(&arena, 65536u) == BR_STATUS_OK);

  status = br_virtual_arena_make_multi_pointer(&arena, test_virtual_arena_util_item, 4u, items);
  assert(status == BR_STATUS_OK);
  assert(items != NULL);
  assert(items[0].a == 0u);
  assert(items[0].b == 0u);
  items[3].a = 77u;
  items[3].b = 88u;
  assert(items[3].a == 77u);
  assert(items[3].b == 88u);

  br_virtual_arena_destroy(&arena);
}

int main(void) {
  test_virtual_arena_util_new_and_clone();
  test_virtual_arena_util_alignment_and_make();
  test_virtual_arena_util_make_multi_pointer();
  return 0;
}
