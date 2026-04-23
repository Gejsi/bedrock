#include <bedrock/mem/virtual_arena_util.h>

#include <string.h>

static br_virtual_arena_make_result
br__virtual_arena_make_result(void *data, usize len, br_status status) {
  br_virtual_arena_make_result result;

  result.data = data;
  result.len = len;
  result.status = status;
  return result;
}

static bool br__virtual_arena_mul_size(usize a, usize b, usize *result) {
  if (result == NULL) {
    return false;
  }
  if (a != 0u && b > SIZE_MAX / a) {
    return false;
  }

  *result = a * b;
  return true;
}

br_alloc_result br_virtual_arena_new_raw(br_virtual_arena *arena, usize size, usize alignment) {
  return br_virtual_arena_alloc(arena, size, alignment);
}

br_alloc_result
br_virtual_arena_clone_raw(br_virtual_arena *arena, const void *src, usize size, usize alignment) {
  br_alloc_result result;

  result = br_virtual_arena_alloc(arena, size, alignment);
  if (result.status != BR_STATUS_OK || result.ptr == NULL) {
    return result;
  }

  if (src != NULL && size != 0u) {
    memcpy(result.ptr, src, size);
  }
  return result;
}

br_virtual_arena_make_result
br_virtual_arena_make_raw(br_virtual_arena *arena, usize elem_size, usize len, usize alignment) {
  br_alloc_result result;
  usize size;

  if (!br__virtual_arena_mul_size(elem_size, len, &size)) {
    return br__virtual_arena_make_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  result = br_virtual_arena_alloc(arena, size, alignment);
  if (result.status != BR_STATUS_OK) {
    return br__virtual_arena_make_result(NULL, 0u, result.status);
  }
  if (result.ptr == NULL && elem_size != 0u) {
    return br__virtual_arena_make_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  return br__virtual_arena_make_result(result.ptr, len, BR_STATUS_OK);
}
