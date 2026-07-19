#ifndef BEDROCK_MEM_BUDDY_ALLOCATOR_H
#define BEDROCK_MEM_BUDDY_ALLOCATOR_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_buddy_block br_buddy_block;

typedef struct br_buddy_allocator {
  br_buddy_block *head;
  br_buddy_block *tail;
  size_t alignment;
} br_buddy_allocator;

/*
Bedrock's buddy allocator, with these design choices:
- takes an explicit `buffer` + `capacity` pair
- initialization reports statuses rather than aborting
- the generic allocator adapter supports alloc/free/resize/reset only (no
  feature/info queries)
- all allocations use the allocator's fixed initialization alignment, so the
  generic allocator adapter ignores per-request alignments
*/

br_status
br_buddy_allocator_init(br_buddy_allocator *buddy, void *buffer, size_t capacity, size_t alignment);
void br_buddy_allocator_free_all(br_buddy_allocator *buddy);

br_alloc_result br_buddy_allocator_alloc(br_buddy_allocator *buddy, size_t size);
br_alloc_result br_buddy_allocator_alloc_uninit(br_buddy_allocator *buddy, size_t size);
br_status br_buddy_allocator_free(br_buddy_allocator *buddy, void *ptr);

br_allocator br_buddy_allocator_allocator(br_buddy_allocator *buddy);

BR_EXTERN_C_END

#endif
