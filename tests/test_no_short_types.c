/*
Compiled with -DBEDROCK_NO_SHORT_TYPES (see the Makefile). With the short
aliases disabled, this translation unit must build using only standard C types.
If any public header still spelled part of its ABI with an alias like `usize`
or `u32`, this file would fail to compile with an unknown-type error, so the
build succeeding is the guarantee that the public ABI is alias-free.
*/
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <bedrock.h>

/* The vec template is opt-in and not reached through <bedrock.h>; instantiate
   it here so its declarations are covered by the no-short-types build too. */
#define BR_VEC_T uint32_t
#define BR_VEC_NAME br_u32_vec
#include <bedrock/template/vec.h>

static void test_alloc_and_arena(void) {
  uint8_t storage[128];
  br_arena arena;
  br_alloc_result alloc;
  uint32_t *value;

  br_arena_init(&arena, storage, sizeof(storage));

  alloc = br_arena_alloc(&arena, sizeof(uint32_t), (size_t)_Alignof(uint32_t));
  assert(alloc.status == BR_STATUS_OK);
  assert(alloc.ptr != NULL);
  assert(((uintptr_t)alloc.ptr & (_Alignof(uint32_t) - 1u)) == 0u);

  value = (uint32_t *)alloc.ptr;
  *value = 0xdeadbeefu;
  assert(*value == 0xdeadbeefu);
}

static void test_bytes_and_utf8(void) {
  br_bytes_view view;
  br_utf8_decode_result decoded;
  size_t count;

  view = br_bytes_view_from_cstr("heya");
  assert(view.len == 4u);

  decoded = br_utf8_decode(br_bytes_view_from_cstr("A"));
  assert(decoded.value == (br_rune)'A');
  assert(decoded.width == 1u);

  count = br_utf8_rune_count(br_bytes_view_from_cstr("abc"));
  assert(count == 3u);
}

static void test_strings_builder(void) {
  br_string_builder builder;
  br_string_view view;

  br_string_builder_init(&builder, br_allocator_heap());
  assert(br_string_builder_write(&builder, br_string_view_make("hi", 2u)).status == BR_STATUS_OK);
  view = br_string_builder_view(&builder);
  assert(view.len == 2u);
  assert(memcmp(view.data, "hi", 2u) == 0);
  br_string_builder_destroy(&builder);
}

static void test_io_result_types(void) {
  br_i64_result r;
  int64_t offset;

  r = br_i64_result_make((int64_t)42, BR_STATUS_OK);
  assert(r.value == (int64_t)42);
  assert(r.status == BR_STATUS_OK);

  offset = r.value;
  assert(offset == (int64_t)42);
}

static void test_time_durations(void) {
  br_duration one_second;
  double seconds;

  one_second = BR_SECOND;
  seconds = br_duration_seconds(one_second);
  assert(seconds >= 0.5 && seconds <= 1.5);
}

static void test_atomic_typedefs(void) {
  br_atomic_u32 counter;
  br_atomic_usize offset;

  br_atomic_store(&counter, (uint32_t)7);
  assert(br_atomic_load(&counter) == (uint32_t)7);

  br_atomic_store(&offset, (size_t)3);
  assert(br_atomic_load(&offset) == (size_t)3);
}

static void test_vec_template(void) {
  br_u32_vec vec;
  uint32_t popped;

  br_u32_vec_init(&vec, br_allocator_heap());
  assert(br_u32_vec_push(&vec, 11u) == BR_STATUS_OK);
  assert(br_u32_vec_push(&vec, 22u) == BR_STATUS_OK);
  assert(vec.len == 2u);
  assert(br_u32_vec_pop(&vec, &popped) == BR_STATUS_OK);
  assert(popped == 22u);
  br_u32_vec_destroy(&vec);
}

int main(void) {
  test_alloc_and_arena();
  test_bytes_and_utf8();
  test_strings_builder();
  test_io_result_types();
  test_time_durations();
  test_atomic_typedefs();
  test_vec_template();
  return 0;
}
