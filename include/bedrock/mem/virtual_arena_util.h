#ifndef BEDROCK_MEM_VIRTUAL_ARENA_UTIL_H
#define BEDROCK_MEM_VIRTUAL_ARENA_UTIL_H

#include <bedrock/mem/virtual_arena.h>

BR_EXTERN_C_BEGIN

typedef struct br_virtual_arena_make_result {
  void *data;
  usize len;
  br_status status;
} br_virtual_arena_make_result;

/*
This header is the C analogue of Odin's `virtual/arena_util.odin`: a typed
convenience layer over virtual-arena allocation. Because C has no `typeid`
overloads, Bedrock exposes raw helpers plus typed macros that assign directly
into caller variables and return `br_status`.
*/

br_alloc_result br_virtual_arena_new_raw(br_virtual_arena *arena, usize size, usize alignment);
br_alloc_result
br_virtual_arena_clone_raw(br_virtual_arena *arena, const void *src, usize size, usize alignment);
br_virtual_arena_make_result
br_virtual_arena_make_raw(br_virtual_arena *arena, usize elem_size, usize len, usize alignment);

static inline br_status br__virtual_arena_assign_new(void **out_ptr, br_alloc_result result) {
  if (out_ptr != NULL) {
    *out_ptr = result.ptr;
  }

  return result.status;
}

static inline br_status
br__virtual_arena_assign_make(void **out_ptr, usize *out_len, br_virtual_arena_make_result result) {
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
    br_virtual_arena_new_raw((arena), sizeof(Type), (usize) _Alignof(Type)))

#define br_virtual_arena_new_aligned(arena, Type, alignment, out_ptr)                              \
  br__virtual_arena_assign_new((void **)(void *)&(out_ptr),                                        \
                               br_virtual_arena_new_raw((arena), sizeof(Type), (alignment)))

#define br_virtual_arena_new_clone(arena, Type, value, out_ptr)                                    \
  br__virtual_arena_assign_new(                                                                    \
    (void **)(void *)&(out_ptr),                                                                   \
    br_virtual_arena_clone_raw(                                                                    \
      (arena), ((const Type[]){(value)}), sizeof(Type), (usize) _Alignof(Type)))

#define br_virtual_arena_make_slice(arena, Type, len, out_ptr, out_len)                            \
  br__virtual_arena_assign_make(                                                                   \
    (void **)(void *)&(out_ptr),                                                                   \
    &(out_len),                                                                                    \
    br_virtual_arena_make_raw((arena), sizeof(Type), (len), (usize) _Alignof(Type)))

#define br_virtual_arena_make_aligned(arena, Type, len, alignment, out_ptr, out_len)               \
  br__virtual_arena_assign_make(                                                                   \
    (void **)(void *)&(out_ptr),                                                                   \
    &(out_len),                                                                                    \
    br_virtual_arena_make_raw((arena), sizeof(Type), (len), (alignment)))

#define br_virtual_arena_make_multi_pointer(arena, Type, len, out_ptr)                             \
  br__virtual_arena_assign_make(                                                                   \
    (void **)(void *)&(out_ptr),                                                                   \
    NULL,                                                                                          \
    br_virtual_arena_make_raw((arena), sizeof(Type), (len), (usize) _Alignof(Type)))

BR_EXTERN_C_END

#endif
