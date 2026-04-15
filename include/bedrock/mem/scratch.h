#ifndef BEDROCK_MEM_SCRATCH_H
#define BEDROCK_MEM_SCRATCH_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

#define BR_SCRATCH_DEFAULT_BACKING_SIZE ((usize)(4u * 1024u * 1024u))

typedef struct br_scratch_leaked_allocation {
  void *memory;
  usize size;
} br_scratch_leaked_allocation;

typedef struct br_scratch {
  u8 *data;
  usize capacity;
  usize curr_offset;
  void *prev_allocation;
  void *prev_allocation_root;
  br_allocator backup_allocator;
  br_scratch_leaked_allocation *leaked_allocations;
  usize leaked_count;
  usize leaked_cap;
} br_scratch;

/*
Bedrock keeps Odin's current scratch allocator behavior close, with these
intentional C-side adaptations:
- no ambient context allocator/logger; explicit `backup_allocator` defaults to
  `br_allocator_heap()` when unset
- misuse reports statuses instead of Odin's assertion/panic-heavy diagnostics
- Bedrock does not emit Odin's warning log when the backup allocator is used
- copy paths use explicit `min(old_size, new_size)` semantics
*/

br_status br_scratch_init(br_scratch *scratch, usize size, br_allocator backup_allocator);
void br_scratch_destroy(br_scratch *scratch);
void br_scratch_free_all(br_scratch *scratch);

br_alloc_result br_scratch_alloc(br_scratch *scratch, usize size, usize alignment);
br_alloc_result br_scratch_alloc_uninit(br_scratch *scratch, usize size, usize alignment);
br_status br_scratch_free(br_scratch *scratch, void *ptr);

br_alloc_result
br_scratch_resize(br_scratch *scratch, void *ptr, usize old_size, usize new_size, usize alignment);
br_alloc_result br_scratch_resize_uninit(
  br_scratch *scratch, void *ptr, usize old_size, usize new_size, usize alignment);

br_allocator br_scratch_allocator(br_scratch *scratch);

BR_EXTERN_C_END

#endif
