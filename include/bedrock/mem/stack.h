#ifndef BEDROCK_MEM_STACK_H
#define BEDROCK_MEM_STACK_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_stack {
  uint8_t *data;
  size_t capacity;
  size_t prev_offset;
  size_t curr_offset;
  size_t peak_used;
} br_stack;

/*
Bedrock's stack allocator, with these design choices:
- takes an explicit `buffer` + `capacity` pair
- misuse reports statuses rather than aborting
- the allocator adapter supports alloc/free/resize/reset only (no feature/info
  queries)
*/

void br_stack_init(br_stack *stack, void *buffer, size_t capacity);
void br_stack_free_all(br_stack *stack);

br_alloc_result br_stack_alloc(br_stack *stack, size_t size, size_t alignment);
br_alloc_result br_stack_alloc_uninit(br_stack *stack, size_t size, size_t alignment);
br_status br_stack_free(br_stack *stack, void *ptr);

br_alloc_result
br_stack_resize(br_stack *stack, void *ptr, size_t old_size, size_t new_size, size_t alignment);
br_alloc_result br_stack_resize_uninit(
  br_stack *stack, void *ptr, size_t old_size, size_t new_size, size_t alignment);

br_allocator br_stack_allocator(br_stack *stack);

BR_EXTERN_C_END

#endif
