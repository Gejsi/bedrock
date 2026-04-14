#ifndef BEDROCK_MEM_ROLLBACK_STACK_H
#define BEDROCK_MEM_ROLLBACK_STACK_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

/*
Reusable head blocks follow Odin's packed-header limit. Oversized singleton
allocations may still exceed this internally because they never chain previous
allocation offsets inside the same block.
*/
#define BR_ROLLBACK_STACK_DEFAULT_BLOCK_SIZE ((usize)(4u * 1024u * 1024u))
#define BR_ROLLBACK_STACK_MAX_HEAD_BLOCK_SIZE ((usize)0x7fffffffu)

typedef struct br_rollback_stack_block br_rollback_stack_block;

typedef struct br_rollback_stack {
  br_rollback_stack_block *head;
  usize block_size;
  br_allocator block_allocator;
  bool head_owned;
} br_rollback_stack;

/*
Bedrock follows Odin's rollback stack allocator shape closely, but a few C-side
adaptations are intentional:
- initialization is split into explicit buffered and dynamic entry points
- invalid usage returns statuses instead of Odin's assertion-heavy diagnostics
- non-last resize fallback copies `min(old_size, new_size)` bytes to avoid
  relying on runtime copy helpers with implicit size semantics
*/

br_status br_rollback_stack_init_buffered(br_rollback_stack *stack, void *buffer, usize capacity);
br_status br_rollback_stack_init_dynamic(br_rollback_stack *stack,
                                         usize block_size,
                                         br_allocator block_allocator);

void br_rollback_stack_destroy(br_rollback_stack *stack);
void br_rollback_stack_reset(br_rollback_stack *stack);

br_alloc_result br_rollback_stack_alloc(br_rollback_stack *stack, usize size, usize alignment);
br_alloc_result
br_rollback_stack_alloc_uninit(br_rollback_stack *stack, usize size, usize alignment);
br_alloc_result br_rollback_stack_resize(
  br_rollback_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment);
br_alloc_result br_rollback_stack_resize_uninit(
  br_rollback_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment);
br_status br_rollback_stack_free(br_rollback_stack *stack, void *ptr);

br_allocator br_rollback_stack_allocator(br_rollback_stack *stack);

BR_EXTERN_C_END

#endif
