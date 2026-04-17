#ifndef BEDROCK_MEM_BUDDY_ALLOCATOR_H
#define BEDROCK_MEM_BUDDY_ALLOCATOR_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_buddy_block br_buddy_block;

typedef struct br_buddy_allocator {
  br_buddy_block *head;
  br_buddy_block *tail;
  usize alignment;
} br_buddy_allocator;

/*
Bedrock keeps Odin's buddy allocator behavior close, with these intentional
C-side adaptations:
- explicit `buffer` + `capacity` instead of Odin's `[]byte` backing slice
- initialization reports statuses instead of Odin's assertion-heavy diagnostics
- the generic allocator adapter targets Bedrock's current alloc/free/resize/reset
  ABI, not Odin's richer query-features/query-info modes
- all allocations use the allocator's fixed initialization alignment, so the
  generic allocator adapter ignores per-request alignments
*/

br_status
br_buddy_allocator_init(br_buddy_allocator *buddy, void *buffer, usize capacity, usize alignment);
void br_buddy_allocator_free_all(br_buddy_allocator *buddy);

br_alloc_result br_buddy_allocator_alloc(br_buddy_allocator *buddy, usize size);
br_alloc_result br_buddy_allocator_alloc_uninit(br_buddy_allocator *buddy, usize size);
br_status br_buddy_allocator_free(br_buddy_allocator *buddy, void *ptr);

br_allocator br_buddy_allocator_allocator(br_buddy_allocator *buddy);

BR_EXTERN_C_END

#endif
