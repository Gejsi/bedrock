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
  br_source_location location;
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
br_alloc_result br_allocator_call_at(br_allocator allocator,
                                     const br_alloc_request *req,
                                     br_source_location location);

br_alloc_result br_allocator_alloc(br_allocator allocator, usize size, usize alignment);
br_alloc_result br_allocator_alloc_uninit(br_allocator allocator, usize size, usize alignment);
br_alloc_result br_allocator_resize(
  br_allocator allocator, void *ptr, usize old_size, usize new_size, usize alignment);
br_alloc_result br_allocator_resize_uninit(
  br_allocator allocator, void *ptr, usize old_size, usize new_size, usize alignment);
br_status br_allocator_free(br_allocator allocator, void *ptr, usize old_size);
br_status br_allocator_reset(br_allocator allocator);

br_alloc_result br_allocator_alloc_at(br_allocator allocator,
                                      usize size,
                                      usize alignment,
                                      br_source_location location);
br_alloc_result br_allocator_alloc_uninit_at(br_allocator allocator,
                                             usize size,
                                             usize alignment,
                                             br_source_location location);
br_alloc_result br_allocator_resize_at(br_allocator allocator,
                                       void *ptr,
                                       usize old_size,
                                       usize new_size,
                                       usize alignment,
                                       br_source_location location);
br_alloc_result br_allocator_resize_uninit_at(br_allocator allocator,
                                              void *ptr,
                                              usize old_size,
                                              usize new_size,
                                              usize alignment,
                                              br_source_location location);
br_status br_allocator_free_at(br_allocator allocator,
                               void *ptr,
                               usize old_size,
                               br_source_location location);
br_status br_allocator_reset_at(br_allocator allocator, br_source_location location);

br_allocator br_allocator_heap(void);
br_allocator br_allocator_null(void);
br_allocator br_allocator_fail(void);

#ifndef BEDROCK_NO_ALLOCATOR_LOCATION_MACROS
#define br_allocator_call(allocator, req)                                                          \
  br_allocator_call_at((allocator), (req), BR_SOURCE_LOCATION)
#define br_allocator_alloc(allocator, size, alignment)                                             \
  br_allocator_alloc_at((allocator), (size), (alignment), BR_SOURCE_LOCATION)
#define br_allocator_alloc_uninit(allocator, size, alignment)                                      \
  br_allocator_alloc_uninit_at((allocator), (size), (alignment), BR_SOURCE_LOCATION)
#define br_allocator_resize(allocator, ptr, old_size, new_size, alignment)                         \
  br_allocator_resize_at(                                                                          \
    (allocator), (ptr), (old_size), (new_size), (alignment), BR_SOURCE_LOCATION)
#define br_allocator_resize_uninit(allocator, ptr, old_size, new_size, alignment)                  \
  br_allocator_resize_uninit_at(                                                                   \
    (allocator), (ptr), (old_size), (new_size), (alignment), BR_SOURCE_LOCATION)
#define br_allocator_free(allocator, ptr, old_size)                                                \
  br_allocator_free_at((allocator), (ptr), (old_size), BR_SOURCE_LOCATION)
#define br_allocator_reset(allocator) br_allocator_reset_at((allocator), BR_SOURCE_LOCATION)
#endif

BR_EXTERN_C_END

#endif
