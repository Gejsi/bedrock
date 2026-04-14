#ifndef BEDROCK_MEM_ALLOC_H
#define BEDROCK_MEM_ALLOC_H

#include <bedrock/base.h>

BR_EXTERN_C_BEGIN

typedef enum br_alloc_op {
  BR_ALLOC_OP_ALLOC = 0,
  BR_ALLOC_OP_ALLOC_UNINIT,
  BR_ALLOC_OP_RESIZE,
  BR_ALLOC_OP_RESIZE_UNINIT,
  BR_ALLOC_OP_FREE,
  BR_ALLOC_OP_RESET
} br_alloc_op;

typedef struct br_alloc_request {
  br_alloc_op op;
  void *ptr;
  usize old_size;
  usize size;
  usize alignment;
} br_alloc_request;

typedef struct br_alloc_result {
  void *ptr;
  usize size;
  br_status status;
} br_alloc_result;

typedef br_alloc_result (*br_allocator_fn)(void *ctx, const br_alloc_request *req);

typedef struct br_allocator {
  br_allocator_fn fn;
  void *ctx;
} br_allocator;

br_alloc_result br_allocator_call(br_allocator allocator, const br_alloc_request *req);

br_alloc_result br_allocator_alloc(br_allocator allocator, usize size, usize alignment);
br_alloc_result br_allocator_alloc_uninit(br_allocator allocator, usize size, usize alignment);
br_alloc_result br_allocator_resize(
  br_allocator allocator, void *ptr, usize old_size, usize new_size, usize alignment);
br_alloc_result br_allocator_resize_uninit(
  br_allocator allocator, void *ptr, usize old_size, usize new_size, usize alignment);
br_status br_allocator_free(br_allocator allocator, void *ptr, usize old_size);
br_status br_allocator_reset(br_allocator allocator);

br_allocator br_allocator_heap(void);
br_allocator br_allocator_null(void);
br_allocator br_allocator_fail(void);

BR_EXTERN_C_END

#endif
