#ifndef BEDROCK_MEM_TRACKING_ALLOCATOR_H
#define BEDROCK_MEM_TRACKING_ALLOCATOR_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_tracking_allocator_entry {
  void *memory;
  usize size;
  usize alignment;
  br_alloc_op op;
  br_status status;
} br_tracking_allocator_entry;

typedef struct br_tracking_allocator_bad_free {
  void *memory;
  usize size;
} br_tracking_allocator_bad_free;

typedef struct br_tracking_allocator_stats {
  usize total_memory_allocated;
  usize total_allocation_count;
  usize total_memory_freed;
  usize total_free_count;
  usize peak_memory_allocated;
  usize current_memory_allocated;
  usize live_allocation_count;
} br_tracking_allocator_stats;

typedef void (*br_tracking_allocator_bad_free_fn)(void *ctx,
                                                  const br_tracking_allocator_bad_free *bad_free);

typedef struct br_tracking_allocator {
  br_allocator backing;
  br_allocator internals;
  br_tracking_allocator_entry *entries;
  usize entry_count;
  usize entry_cap;
  br_tracking_allocator_bad_free *bad_frees;
  usize bad_free_count;
  usize bad_free_cap;
  br_tracking_allocator_stats stats;
  br_tracking_allocator_bad_free_fn bad_free_fn;
  void *bad_free_ctx;
  bool clear_on_reset;
} br_tracking_allocator;

/*
Bedrock's tracking allocator is intentionally smaller than Odin's current one:
- no built-in mutex yet
- linear live-allocation tracking instead of a map
- `clear_on_reset` is configured explicitly because Bedrock's allocator ABI
  does not yet expose Odin-style feature queries
- bad frees are recorded by default instead of trapping
*/

void br_tracking_allocator_init(br_tracking_allocator *tracking,
                                br_allocator backing,
                                br_allocator internals);
void br_tracking_allocator_destroy(br_tracking_allocator *tracking);

/* Clears live allocation and bad-free records but keeps cumulative totals. */
void br_tracking_allocator_clear(br_tracking_allocator *tracking);

/* Resets both live tracking state and cumulative totals. */
void br_tracking_allocator_reset(br_tracking_allocator *tracking);

br_allocator br_tracking_allocator_allocator(br_tracking_allocator *tracking);

BR_EXTERN_C_END

#endif
