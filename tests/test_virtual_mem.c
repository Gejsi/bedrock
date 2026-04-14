#include <assert.h>
#include <string.h>

#include <bedrock.h>

static void test_vm_reserve_commit_release(void) {
  br_vm_region_result region;
  usize page_size = br_vm_page_size();

  if (page_size == 0u) {
    return;
  }

  region = br_vm_reserve(page_size * 2u);
  assert(region.status == BR_STATUS_OK);
  assert(region.value.data != NULL);

  assert(br_vm_commit(region.value.data, page_size) == BR_STATUS_OK);
  memset(region.value.data, 0x5a, page_size);

  assert(br_vm_protect(region.value.data, page_size, BR_VM_PROTECT_READ | BR_VM_PROTECT_WRITE));

  br_vm_decommit(region.value.data, page_size);
  br_vm_release(region.value.data, region.value.size);

  region = br_vm_reserve_commit(page_size);
  assert(region.status == BR_STATUS_OK);
  assert(region.value.data != NULL);
  memset(region.value.data, 0x11, page_size);
  br_vm_release(region.value.data, region.value.size);
}

static void test_virtual_arena_static(void) {
  br_virtual_arena arena;
  br_virtual_arena_mark mark;
  br_alloc_result first;
  br_alloc_result second;
  usize page_size = br_vm_page_size();

  if (page_size == 0u) {
    return;
  }

  br_virtual_arena_init(&arena);
  arena.default_commit_size = page_size;
  assert(br_virtual_arena_init_static(&arena, page_size * 4u, page_size) == BR_STATUS_OK);
  assert(arena.kind == BR_VIRTUAL_ARENA_KIND_STATIC);
  assert(arena.total_reserved >= page_size * 4u);

  first = br_virtual_arena_alloc(&arena, page_size / 2u, 16u);
  assert(first.status == BR_STATUS_OK);
  assert(first.ptr != NULL);
  memset(first.ptr, 0xab, first.size);

  mark = br_virtual_arena_mark_save(&arena);
  second = br_virtual_arena_alloc_uninit(&arena, page_size * 2u, 64u);
  assert(second.status == BR_STATUS_OK);
  assert(second.ptr != NULL);
  assert(arena.total_used >= first.size + second.size);

  assert(br_virtual_arena_rewind(&arena, mark) == BR_STATUS_OK);
  assert(arena.total_used == mark.used);

  br_virtual_arena_destroy(&arena);
  assert(arena.kind == BR_VIRTUAL_ARENA_KIND_NONE);
}

static void test_virtual_arena_growing(void) {
  br_virtual_arena arena;
  br_virtual_arena_mark mark;
  br_alloc_result first;
  br_alloc_result second;
  br_alloc_result third;
  usize page_size = br_vm_page_size();
  usize initial_reserved;

  if (page_size == 0u) {
    return;
  }

  br_virtual_arena_init(&arena);
  arena.default_commit_size = page_size;
  arena.minimum_block_size = page_size * 2u;
  assert(br_virtual_arena_init_growing(&arena, page_size * 2u) == BR_STATUS_OK);
  initial_reserved = arena.total_reserved;

  first = br_virtual_arena_alloc(&arena, page_size, 32u);
  assert(first.status == BR_STATUS_OK);
  assert(first.ptr != NULL);

  mark = br_virtual_arena_mark_save(&arena);
  second = br_virtual_arena_alloc(&arena, page_size * 2u, 64u);
  assert(second.status == BR_STATUS_OK);
  assert(second.ptr != NULL);
  assert(arena.total_reserved > initial_reserved);

  assert(br_virtual_arena_rewind(&arena, mark) == BR_STATUS_OK);
  assert(arena.total_used == mark.used);
  assert(arena.total_reserved == initial_reserved);

  third =
    br_allocator_alloc(br_virtual_arena_allocator(&arena), page_size / 2u, (usize) _Alignof(u64));
  assert(third.status == BR_STATUS_OK);
  assert(third.ptr != NULL);

  br_virtual_arena_reset(&arena);
  assert(arena.total_used == 0u);
  assert(arena.total_reserved == initial_reserved);

  br_virtual_arena_destroy(&arena);
}

int main(void) {
  test_vm_reserve_commit_release();
  test_virtual_arena_static();
  test_virtual_arena_growing();
  return 0;
}
