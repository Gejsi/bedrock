#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <bedrock.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

static void br__write_text_file(const char *path, const char *text) {
  FILE *file = fopen(path, "wb");

  assert(file != NULL);
  assert(fwrite(text, 1u, strlen(text), file) == strlen(text));
  assert(fclose(file) == 0);
}

static void br__read_text_file(const char *path, char *buffer, usize cap) {
  FILE *file = fopen(path, "rb");
  usize read_len;

  assert(file != NULL);
  read_len = fread(buffer, 1u, cap - 1u, file);
  buffer[read_len] = '\0';
  assert(fclose(file) == 0);
}

static void br__make_temp_path(char *buffer, usize cap) {
#if defined(_WIN32)
  assert(tmpnam_s(buffer, cap) == 0);
#else
  int fd;
  static unsigned counter = 0u;
  int written;

  for (;;) {
    written = snprintf(buffer, cap, "/tmp/bedrock-vm-%ld-%u.tmp", (long)getpid(), counter);
    assert(written > 0);
    assert((usize)written < cap);
    counter += 1u;

    fd = open(buffer, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd >= 0) {
      assert(close(fd) == 0);
      return;
    }
  }
#endif
}

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

static void test_vm_map_file_readonly(void) {
  char path[64];
  const char *text = "bedrock-map";
  br_vm_mapped_file_result mapping;

  br__make_temp_path(path, sizeof(path));
  br__write_text_file(path, text);

  mapping = br_vm_map_file(path, BR_VM_MAP_FILE_READ);
  assert(mapping.error == BR_VM_MAP_FILE_ERROR_NONE);
  assert(mapping.value.data != NULL);
  assert(mapping.value.size == strlen(text));
  assert(memcmp(mapping.value.data, text, mapping.value.size) == 0);

  br_vm_unmap_file(mapping.value);
  assert(remove(path) == 0);
}

static void test_vm_map_file_write(void) {
  char path[64];
  char file_text[64];
  br_vm_mapped_file_result mapping;

  br__make_temp_path(path, sizeof(path));
  br__write_text_file(path, "abcde");

  mapping = br_vm_map_file(path, BR_VM_MAP_FILE_READ | BR_VM_MAP_FILE_WRITE);
  assert(mapping.error == BR_VM_MAP_FILE_ERROR_NONE);
  assert(mapping.value.data != NULL);
  assert(mapping.value.size == 5u);

  memcpy(mapping.value.data, "ABCDE", mapping.value.size);
  br_vm_unmap_file(mapping.value);

  br__read_text_file(path, file_text, sizeof(file_text));
  assert(strcmp(file_text, "ABCDE") == 0);
  assert(remove(path) == 0);
}

static void test_vm_map_file_invalid(void) {
  br_vm_mapped_file_result mapping;

  mapping = br_vm_map_file(NULL, BR_VM_MAP_FILE_READ);
  assert(mapping.error == BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT);

  mapping = br_vm_map_file("this-file-should-not-exist-bedrock.tmp", BR_VM_MAP_FILE_READ);
  assert(mapping.error == BR_VM_MAP_FILE_ERROR_OPEN_FAILURE);

  br_vm_unmap_file(mapping.value);
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

  br_virtual_arena_init(&arena);
  assert(br_virtual_arena_temp_begin(&arena).status == BR_STATUS_INVALID_STATE);

  assert(br_virtual_arena_init_static(&arena, page_size * 2u, page_size) == BR_STATUS_OK);
  temp = br_virtual_arena_temp_begin(&arena);
  assert(temp.status == BR_STATUS_OK);
  br_virtual_arena_destroy(&arena);
  assert(br_virtual_arena_temp_end(temp.value) == BR_STATUS_INVALID_STATE);
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

int main(void) {
  test_vm_reserve_commit_release();
  test_vm_map_file_readonly();
  test_vm_map_file_write();
  test_vm_map_file_invalid();
  test_virtual_arena_static();
  test_virtual_arena_growing();
  test_virtual_arena_temp_end();
  test_virtual_arena_temp_ignore();
  test_virtual_arena_temp_invalid();
  test_virtual_arena_overflow_protection();
  return 0;
}
