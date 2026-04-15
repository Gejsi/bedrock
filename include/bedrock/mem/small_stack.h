#ifndef BEDROCK_MEM_SMALL_STACK_H
#define BEDROCK_MEM_SMALL_STACK_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_small_stack {
  u8 *data;
  usize capacity;
  usize offset;
  usize peak_used;
} br_small_stack;

/*
Bedrock keeps Odin's small stack allocator behavior close, with these
intentional C-side adaptations:
- explicit `buffer` + `capacity` instead of Odin's `[]byte` backing slice
- misuse reports statuses instead of Odin's panic-heavy diagnostics
- the allocator adapter targets Bedrock's current alloc/free/resize/reset ABI,
  not Odin's richer query-features/query-info modes
- alignment is still expected to satisfy Bedrock's power-of-two allocator
  contract after Odin-style clamping to the small-stack maximum
*/

void br_small_stack_init(br_small_stack *stack, void *buffer, usize capacity);
void br_small_stack_free_all(br_small_stack *stack);

br_alloc_result br_small_stack_alloc(br_small_stack *stack, usize size, usize alignment);
br_alloc_result br_small_stack_alloc_uninit(br_small_stack *stack, usize size, usize alignment);
br_status br_small_stack_free(br_small_stack *stack, void *ptr);

br_alloc_result br_small_stack_resize(
  br_small_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment);
br_alloc_result br_small_stack_resize_uninit(
  br_small_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment);

br_allocator br_small_stack_allocator(br_small_stack *stack);

BR_EXTERN_C_END

#endif
