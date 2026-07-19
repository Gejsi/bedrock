#ifndef BEDROCK_MEM_VIRTUAL_ARENA_UTIL_H
#define BEDROCK_MEM_VIRTUAL_ARENA_UTIL_H

#include <bedrock/mem/virtual_arena.h>

BR_EXTERN_C_BEGIN

typedef struct br_virtual_arena_make_result {
  void *data;
  size_t len;
  br_status status;
} br_virtual_arena_make_result;

/*
This header provides typed convenience helpers over the raw virtual-arena
allocation API. Because C has no `typeid` overloads, Bedrock exposes raw helpers
plus typed macros that assign directly into caller variables and return
`br_status`.
*/

br_alloc_result br_virtual_arena_new_raw(br_virtual_arena *arena, size_t size, size_t alignment);
br_alloc_result
br_virtual_arena_clone_raw(br_virtual_arena *arena, const void *src, size_t size, size_t alignment);
br_virtual_arena_make_result
br_virtual_arena_make_raw(br_virtual_arena *arena, size_t elem_size, size_t len, size_t alignment);

static inline br_status br__virtual_arena_assign_new(void **out_ptr, br_alloc_result result) {
  if (out_ptr != NULL) {
    *out_ptr = result.ptr;
  }

  return result.status;
}

static inline br_status br__virtual_arena_assign_make(void **out_ptr,
                                                      size_t *out_len,
                                                      br_virtual_arena_make_result result) {
  if (out_ptr != NULL) {
    *out_ptr = result.data;
  }
  if (out_len != NULL) {
    *out_len = result.status == BR_STATUS_OK ? result.len : 0u;
  }

  return result.status;
}

#define br_virtual_arena_new(arena, Type, out_ptr)                                                 \
  br__virtual_arena_assign_new(                                                                    \
    (void **)(void *)&(out_ptr),                                                                   \
    br_virtual_arena_new_raw((arena), sizeof(Type), (size_t)_Alignof(Type)))

#define br_virtual_arena_new_aligned(arena, Type, alignment, out_ptr)                              \
  br__virtual_arena_assign_new((void **)(void *)&(out_ptr),                                        \
                               br_virtual_arena_new_raw((arena), sizeof(Type), (alignment)))

#define br_virtual_arena_new_clone(arena, Type, value, out_ptr)                                    \
  br__virtual_arena_assign_new(                                                                    \
    (void **)(void *)&(out_ptr),                                                                   \
    br_virtual_arena_clone_raw(                                                                    \
      (arena), ((const Type[]){(value)}), sizeof(Type), (size_t)_Alignof(Type)))

#define br_virtual_arena_make_slice(arena, Type, len, out_ptr, out_len)                            \
  br__virtual_arena_assign_make(                                                                   \
    (void **)(void *)&(out_ptr),                                                                   \
    &(out_len),                                                                                    \
    br_virtual_arena_make_raw((arena), sizeof(Type), (len), (size_t)_Alignof(Type)))

#define br_virtual_arena_make_aligned(arena, Type, len, alignment, out_ptr, out_len)               \
  br__virtual_arena_assign_make(                                                                   \
    (void **)(void *)&(out_ptr),                                                                   \
    &(out_len),                                                                                    \
    br_virtual_arena_make_raw((arena), sizeof(Type), (len), (alignment)))

#define br_virtual_arena_make_multi_pointer(arena, Type, len, out_ptr)                             \
  br__virtual_arena_assign_make(                                                                   \
    (void **)(void *)&(out_ptr),                                                                   \
    NULL,                                                                                          \
    br_virtual_arena_make_raw((arena), sizeof(Type), (len), (size_t)_Alignof(Type)))

BR_EXTERN_C_END

#endif
