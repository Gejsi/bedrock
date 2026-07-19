#ifndef BEDROCK_MEM_SMALL_STACK_H
#define BEDROCK_MEM_SMALL_STACK_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_small_stack {
  uint8_t *data;
  size_t capacity;
  size_t offset;
  size_t peak_used;
} br_small_stack;

/*
Bedrock's small stack allocator, with these design choices:
- takes an explicit `buffer` + `capacity` pair
- misuse reports statuses rather than aborting
- the allocator adapter supports alloc/free/resize/reset only (no feature/info
  queries)
- alignment is still expected to satisfy Bedrock's power-of-two allocator
  contract after clamping to the small-stack maximum
*/

void br_small_stack_init(br_small_stack *stack, void *buffer, size_t capacity);
void br_small_stack_free_all(br_small_stack *stack);

br_alloc_result br_small_stack_alloc(br_small_stack *stack, size_t size, size_t alignment);
br_alloc_result br_small_stack_alloc_uninit(br_small_stack *stack, size_t size, size_t alignment);
br_status br_small_stack_free(br_small_stack *stack, void *ptr);

br_alloc_result br_small_stack_resize(
  br_small_stack *stack, void *ptr, size_t old_size, size_t new_size, size_t alignment);
br_alloc_result br_small_stack_resize_uninit(
  br_small_stack *stack, void *ptr, size_t old_size, size_t new_size, size_t alignment);

br_allocator br_small_stack_allocator(br_small_stack *stack);

BR_EXTERN_C_END

#endif
