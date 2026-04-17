#ifndef BEDROCK_MEM_DYNAMIC_ARENA_H
#define BEDROCK_MEM_DYNAMIC_ARENA_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

#define BR_DYNAMIC_ARENA_DEFAULT_BLOCK_SIZE ((usize)65536u)
#define BR_DYNAMIC_ARENA_DEFAULT_OUT_BAND_SIZE ((usize)6554u)

typedef struct br_dynamic_arena {
  usize block_size;
  usize out_band_size;
  usize alignment;
  void **unused_blocks;
  usize unused_count;
  usize unused_cap;
  void **used_blocks;
  usize used_count;
  usize used_cap;
  void **out_band_allocations;
  usize out_band_count;
  usize out_band_cap;
  void *current_block;
  u8 *current_pos;
  usize bytes_left;
  br_allocator block_allocator;
  br_allocator array_allocator;
} br_dynamic_arena;

/*
Bedrock keeps Odin's dynamic arena behavior close, with these intentional
C-side adaptations:
- explicit `block_allocator` and `array_allocator` default to heap allocators
  when passed unset, instead of using Odin's ambient context allocator
- direct allocation calls use the arena's configured alignment rather than a
  per-request alignment parameter, matching Odin's fixed-alignment design
- the generic allocator adapter targets Bedrock's alloc/free/resize/reset ABI;
  `FREE` reports not-supported and `RESET` maps to `free_all`
- block cycling preserves the current block if acquiring the next block fails,
  instead of mutating state before the failing allocation completes
*/

br_status br_dynamic_arena_init(br_dynamic_arena *arena,
                                br_allocator block_allocator,
                                br_allocator array_allocator,
                                usize block_size,
                                usize out_band_size,
                                usize alignment);
void br_dynamic_arena_destroy(br_dynamic_arena *arena);

void br_dynamic_arena_reset(br_dynamic_arena *arena);
void br_dynamic_arena_free_all(br_dynamic_arena *arena);

br_alloc_result br_dynamic_arena_alloc(br_dynamic_arena *arena, usize size);
br_alloc_result br_dynamic_arena_alloc_uninit(br_dynamic_arena *arena, usize size);
br_alloc_result
br_dynamic_arena_resize(br_dynamic_arena *arena, void *ptr, usize old_size, usize new_size);
br_alloc_result
br_dynamic_arena_resize_uninit(br_dynamic_arena *arena, void *ptr, usize old_size, usize new_size);

br_allocator br_dynamic_arena_allocator(br_dynamic_arena *arena);

BR_EXTERN_C_END

#endif
