#ifndef BEDROCK_MEM_VIRTUAL_ARENA_H
#define BEDROCK_MEM_VIRTUAL_ARENA_H

#include <bedrock/mem/alloc.h>
#include <bedrock/mem/virtual.h>

BR_EXTERN_C_BEGIN

typedef struct br_virtual_arena_block br_virtual_arena_block;

typedef enum br_virtual_arena_kind {
  BR_VIRTUAL_ARENA_KIND_NONE = 0,
  BR_VIRTUAL_ARENA_KIND_GROWING = 1,
  BR_VIRTUAL_ARENA_KIND_STATIC = 2
} br_virtual_arena_kind;

typedef struct br_virtual_arena {
  br_virtual_arena_kind kind;
  br_virtual_arena_block *curr_block;
  usize total_used;
  usize total_reserved;
  usize default_commit_size;
  usize minimum_block_size;
  usize temp_count;
} br_virtual_arena;

typedef struct br_virtual_arena_mark {
  br_virtual_arena_block *block;
  usize used;
} br_virtual_arena_mark;

typedef struct br_virtual_arena_temp {
  br_virtual_arena *arena;
  br_virtual_arena_block *block;
  usize used;
} br_virtual_arena_temp;

typedef struct br_virtual_arena_temp_result {
  br_virtual_arena_temp value;
  br_status status;
} br_virtual_arena_temp_result;

#define BR_VIRTUAL_ARENA_DEFAULT_STATIC_COMMIT_SIZE ((usize)1048576u)
#define BR_VIRTUAL_ARENA_DEFAULT_GROWING_COMMIT_SIZE ((usize)8388608u)
#define BR_VIRTUAL_ARENA_DEFAULT_GROWING_MINIMUM_BLOCK_SIZE                                        \
  BR_VIRTUAL_ARENA_DEFAULT_STATIC_COMMIT_SIZE

#if UINTPTR_MAX > UINT32_MAX
#define BR_VIRTUAL_ARENA_DEFAULT_STATIC_RESERVE_SIZE ((usize)1073741824u)
#else
#define BR_VIRTUAL_ARENA_DEFAULT_STATIC_RESERVE_SIZE ((usize)134217728u)
#endif

/*
Bedrock v1 only exposes Odin's virtual growing/static variants here. Buffer
arenas already exist separately as `br_arena`, so this layer stays focused on
virtual-memory-backed arenas. Unlike Odin's current implementation, Bedrock
does not add a built-in mutex here yet because `thread`/`sync` has not landed.
*/

void br_virtual_arena_init(br_virtual_arena *arena);

/*
Callers may prefill `default_commit_size` and `minimum_block_size` on a zeroed
arena before initialization, matching Odin's growth-policy style.
*/
br_status br_virtual_arena_init_growing(br_virtual_arena *arena, usize reserved);
br_status br_virtual_arena_init_static(br_virtual_arena *arena, usize reserved, usize commit_size);

void br_virtual_arena_reset(br_virtual_arena *arena);
void br_virtual_arena_destroy(br_virtual_arena *arena);

br_virtual_arena_mark br_virtual_arena_mark_save(const br_virtual_arena *arena);
br_status br_virtual_arena_rewind(br_virtual_arena *arena, br_virtual_arena_mark mark);

/*
These helpers mirror Odin's Arena_Temp workflow, but Bedrock reports misuse
through statuses instead of asserting because the C API should not abort by
default on ownership mistakes.
*/
br_virtual_arena_temp_result br_virtual_arena_temp_begin(br_virtual_arena *arena);
br_status br_virtual_arena_temp_end(br_virtual_arena_temp temp);
br_status br_virtual_arena_temp_ignore(br_virtual_arena_temp temp);
br_status br_virtual_arena_check_temp(const br_virtual_arena *arena);

br_alloc_result br_virtual_arena_alloc(br_virtual_arena *arena, usize size, usize alignment);
br_alloc_result br_virtual_arena_alloc_uninit(br_virtual_arena *arena, usize size, usize alignment);
br_allocator br_virtual_arena_allocator(br_virtual_arena *arena);

BR_EXTERN_C_END

#endif
