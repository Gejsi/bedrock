#include <assert.h>
#include <string.h>

#include <bedrock.h>

#if !defined(_WIN32)
#include <pthread.h>
#endif

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

static void test_virtual_arena_temp_end(void) {
  br_virtual_arena arena;
  br_virtual_arena_temp_result temp;
  br_alloc_result first;
  br_alloc_result second;
  usize page_size = br_vm_page_size();
  usize initial_reserved;
  usize initial_used;

  if (page_size == 0u) {
    return;
  }

  br_virtual_arena_init(&arena);
  arena.default_commit_size = page_size;
  arena.minimum_block_size = page_size * 2u;
  assert(br_virtual_arena_init_growing(&arena, page_size * 2u) == BR_STATUS_OK);

  first = br_virtual_arena_alloc(&arena, page_size / 2u, 32u);
  assert(first.status == BR_STATUS_OK);
  initial_reserved = arena.total_reserved;
  initial_used = arena.total_used;

  temp = br_virtual_arena_temp_begin(&arena);
  assert(temp.status == BR_STATUS_OK);
  assert(br_virtual_arena_check_temp(&arena) == BR_STATUS_INVALID_STATE);

  second = br_virtual_arena_alloc(&arena, page_size * 2u, 64u);
  assert(second.status == BR_STATUS_OK);
  assert(arena.total_reserved > initial_reserved);

  assert(br_virtual_arena_temp_end(temp.value) == BR_STATUS_OK);
  assert(br_virtual_arena_check_temp(&arena) == BR_STATUS_OK);
  assert(arena.total_reserved == initial_reserved);
  assert(arena.total_used == initial_used);
  assert(br_virtual_arena_temp_end(temp.value) == BR_STATUS_INVALID_STATE);

  br_virtual_arena_destroy(&arena);
}

static void test_virtual_arena_temp_ignore(void) {
  br_virtual_arena arena;
  br_virtual_arena_temp_result temp;
  br_alloc_result alloc;
  usize page_size = br_vm_page_size();
  usize used_after_alloc;

  if (page_size == 0u) {
    return;
  }

  br_virtual_arena_init(&arena);
  arena.default_commit_size = page_size;
  assert(br_virtual_arena_init_static(&arena, page_size * 4u, page_size) == BR_STATUS_OK);

  temp = br_virtual_arena_temp_begin(&arena);
  assert(temp.status == BR_STATUS_OK);

  alloc = br_virtual_arena_alloc(&arena, page_size / 2u, 16u);
  assert(alloc.status == BR_STATUS_OK);
  used_after_alloc = arena.total_used;

  assert(br_virtual_arena_temp_ignore(temp.value) == BR_STATUS_OK);
  assert(arena.total_used == used_after_alloc);
  assert(br_virtual_arena_check_temp(&arena) == BR_STATUS_OK);

  br_virtual_arena_destroy(&arena);
}

static void test_virtual_arena_temp_invalid(void) {
  br_virtual_arena arena;
  br_virtual_arena_temp zero_temp;
  br_virtual_arena_temp_result temp;
  usize page_size = br_vm_page_size();

  memset(&zero_temp, 0, sizeof(zero_temp));

  assert(br_virtual_arena_temp_begin(NULL).status == BR_STATUS_INVALID_ARGUMENT);
  assert(br_virtual_arena_check_temp(NULL) == BR_STATUS_INVALID_ARGUMENT);
  assert(br_virtual_arena_temp_end(zero_temp) == BR_STATUS_INVALID_ARGUMENT);
  assert(br_virtual_arena_temp_ignore(zero_temp) == BR_STATUS_INVALID_ARGUMENT);

  if (page_size == 0u) {
    return;
  }

  /*
  temp_begin on a still-empty arena now succeeds (option A lazy init), so the
  old INVALID_STATE case is gone; the positive lazy-init path is covered in
  test_virtual_arena_lazy_temp. The remaining invalid case here is ending a
  temp against a destroyed arena, which needs an explicitly initialized arena.
  */
  br_virtual_arena_init(&arena);
  assert(br_virtual_arena_init_static(&arena, page_size * 2u, page_size) == BR_STATUS_OK);
  temp = br_virtual_arena_temp_begin(&arena);
  assert(temp.status == BR_STATUS_OK);
  br_virtual_arena_destroy(&arena);
  assert(br_virtual_arena_temp_end(temp.value) == BR_STATUS_INVALID_STATE);
}

static void test_virtual_arena_lazy_alloc(void) {
  br_virtual_arena arena;
  br_alloc_result first;

  if (br_vm_page_size() == 0u) {
    return;
  }

  /* A zeroed arena (no init call) is a ready-to-use empty growing arena. */
  memset(&arena, 0, sizeof(arena));

  first = br_virtual_arena_alloc(&arena, 128u, 16u);
  assert(first.status == BR_STATUS_OK);
  assert(first.ptr != NULL);
  assert(((uptr)first.ptr % 16u) == 0u);

  /* First use promoted it to a growing arena and filled in the defaults. */
  assert(arena.kind == BR_VIRTUAL_ARENA_KIND_GROWING);
  assert(arena.curr_block != NULL);
  assert(arena.minimum_block_size != 0u);
  assert(arena.default_commit_size != 0u);
  assert(arena.total_used >= 128u);

  br_virtual_arena_destroy(&arena);
}

static void test_virtual_arena_lazy_prefilled_flags(void) {
  br_virtual_arena arena;
  br_alloc_result first;

  if (br_vm_page_size() == 0u) {
    return;
  }

  /* Fields prefilled on the zeroed struct are honored on first use. */
  memset(&arena, 0, sizeof(arena));
  arena.flags = BR_VIRTUAL_ARENA_FLAG_OVERFLOW_PROTECTION;

  first = br_virtual_arena_alloc(&arena, 64u, 16u);
  assert(first.status == BR_STATUS_OK);
  assert(first.ptr != NULL);
  assert(arena.flags == BR_VIRTUAL_ARENA_FLAG_OVERFLOW_PROTECTION);

  br_virtual_arena_destroy(&arena);
}

static void test_virtual_arena_lazy_mark_rewind(void) {
  br_virtual_arena arena;
  br_virtual_arena_mark mark;
  br_alloc_result alloc;

  if (br_vm_page_size() == 0u) {
    return;
  }

  /* A mark captured before the first allocation rewinds back to empty. */
  memset(&arena, 0, sizeof(arena));

  mark = br_virtual_arena_mark_save(&arena);
  assert(mark.block == NULL);
  assert(mark.used == 0u);

  alloc = br_virtual_arena_alloc(&arena, 256u, 16u);
  assert(alloc.status == BR_STATUS_OK);
  assert(arena.total_used >= 256u);

  assert(br_virtual_arena_rewind(&arena, mark) == BR_STATUS_OK);
  assert(arena.total_used == 0u);
  assert(arena.curr_block == NULL);

  br_virtual_arena_destroy(&arena);
}

static void test_virtual_arena_lazy_temp(void) {
  br_virtual_arena arena;
  br_virtual_arena_temp_result temp;
  br_alloc_result alloc;

  if (br_vm_page_size() == 0u) {
    return;
  }

  /* A temp scope opened before the first allocation ends back to empty. */
  memset(&arena, 0, sizeof(arena));

  temp = br_virtual_arena_temp_begin(&arena);
  assert(temp.status == BR_STATUS_OK);
  assert(arena.kind == BR_VIRTUAL_ARENA_KIND_GROWING);

  alloc = br_virtual_arena_alloc(&arena, 256u, 16u);
  assert(alloc.status == BR_STATUS_OK);
  assert(arena.total_used >= 256u);

  assert(br_virtual_arena_temp_end(temp.value) == BR_STATUS_OK);
  assert(arena.total_used == 0u);
  assert(arena.curr_block == NULL);
  assert(br_virtual_arena_check_temp(&arena) == BR_STATUS_OK);

  br_virtual_arena_destroy(&arena);
}

static void test_virtual_arena_overflow_protection(void) {
  br_virtual_arena arena;
  br_alloc_result full;
  br_alloc_result overflow;
  usize page_size = br_vm_page_size();
  usize reserved;

  if (page_size == 0u) {
    return;
  }

  reserved = page_size * 2u;

  br_virtual_arena_init(&arena);
  arena.flags = BR_VIRTUAL_ARENA_FLAG_OVERFLOW_PROTECTION;
  arena.default_commit_size = page_size;
  assert(br_virtual_arena_init_static(&arena, reserved, page_size) == BR_STATUS_OK);
  assert(arena.total_reserved == reserved);

  full = br_virtual_arena_alloc_uninit(&arena, reserved, 1u);
  assert(full.status == BR_STATUS_OK);
  assert(full.ptr != NULL);

  overflow = br_virtual_arena_alloc(&arena, 1u, 1u);
  assert(overflow.status == BR_STATUS_OUT_OF_MEMORY);
  br_virtual_arena_destroy(&arena);

  br_virtual_arena_init(&arena);
  arena.flags = BR_VIRTUAL_ARENA_FLAG_OVERFLOW_PROTECTION;
  arena.default_commit_size = page_size;
  arena.minimum_block_size = reserved;
  assert(br_virtual_arena_init_growing(&arena, reserved) == BR_STATUS_OK);
  assert(arena.total_reserved == reserved);

  full = br_virtual_arena_alloc_uninit(&arena, reserved, 1u);
  assert(full.status == BR_STATUS_OK);
  assert(full.ptr != NULL);

  overflow = br_virtual_arena_alloc(&arena, 1u, 1u);
  assert(overflow.status == BR_STATUS_OK);
  assert(arena.total_reserved > reserved);
  br_virtual_arena_destroy(&arena);
}

#if !defined(_WIN32)
#define TEST_VIRTUAL_ARENA_THREAD_COUNT 4u
#define TEST_VIRTUAL_ARENA_ITERATIONS 128u
#define TEST_VIRTUAL_ARENA_ALLOC_SIZE 16u

typedef struct test_virtual_arena_thread {
  br_virtual_arena *arena;
  br_atomic_bool *start;
} test_virtual_arena_thread;

static void test_virtual_arena_spin_until_bool(const br_atomic_bool *value, bool expected) {
  while (br_atomic_load_explicit(value, BR_ATOMIC_ACQUIRE) != expected) {
    br_cpu_relax();
  }
}

static void *test_virtual_arena_worker(void *ctx) {
  test_virtual_arena_thread *thread = (test_virtual_arena_thread *)ctx;

  test_virtual_arena_spin_until_bool(thread->start, true);
  for (usize i = 0u; i < TEST_VIRTUAL_ARENA_ITERATIONS; ++i) {
    br_alloc_result allocation =
      br_virtual_arena_alloc_uninit(thread->arena, TEST_VIRTUAL_ARENA_ALLOC_SIZE, 1u);

    assert(allocation.status == BR_STATUS_OK);
    assert(allocation.ptr != NULL);
    memset(allocation.ptr, (u8)i, allocation.size);
  }

  return NULL;
}

static void test_virtual_arena_serializes_allocations(void) {
  br_virtual_arena arena;
  br_atomic_bool start;
  test_virtual_arena_thread thread;
  pthread_t threads[TEST_VIRTUAL_ARENA_THREAD_COUNT];
  usize page_size = br_vm_page_size();
  usize expected_used =
    TEST_VIRTUAL_ARENA_THREAD_COUNT * TEST_VIRTUAL_ARENA_ITERATIONS * TEST_VIRTUAL_ARENA_ALLOC_SIZE;
  usize reserve_pages;
  usize reserved;

  if (page_size == 0u) {
    return;
  }

  reserve_pages = (expected_used + page_size - 1u) / page_size + 1u;
  reserved = reserve_pages * page_size;

  br_virtual_arena_init(&arena);
  arena.default_commit_size = page_size;
  assert(br_virtual_arena_init_static(&arena, reserved, page_size) == BR_STATUS_OK);

  br_atomic_init(&start, false);
  thread.arena = &arena;
  thread.start = &start;

  for (usize i = 0u; i < TEST_VIRTUAL_ARENA_THREAD_COUNT; ++i) {
    assert(pthread_create(&threads[i], NULL, test_virtual_arena_worker, &thread) == 0);
  }

  br_atomic_store_explicit(&start, true, BR_ATOMIC_RELEASE);

  for (usize i = 0u; i < TEST_VIRTUAL_ARENA_THREAD_COUNT; ++i) {
    assert(pthread_join(threads[i], NULL) == 0);
  }

  assert(arena.total_used == expected_used);
  assert(arena.temp_count == 0u);

  br_virtual_arena_destroy(&arena);
}
#endif

int main(void) {
  test_vm_reserve_commit_release();
  test_virtual_arena_static();
  test_virtual_arena_growing();
  test_virtual_arena_temp_end();
  test_virtual_arena_temp_ignore();
  test_virtual_arena_temp_invalid();
  test_virtual_arena_lazy_alloc();
  test_virtual_arena_lazy_prefilled_flags();
  test_virtual_arena_lazy_mark_rewind();
  test_virtual_arena_lazy_temp();
  test_virtual_arena_overflow_protection();
#if !defined(_WIN32)
  test_virtual_arena_serializes_allocations();
#endif
  return 0;
}
