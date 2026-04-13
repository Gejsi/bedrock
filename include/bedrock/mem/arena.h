#ifndef BEDROCK_MEM_ARENA_H
#define BEDROCK_MEM_ARENA_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_arena {
  u8 *buffer;
  usize capacity;
  usize offset;
  usize peak;
} br_arena;

typedef struct br_arena_mark {
  usize offset;
} br_arena_mark;

void br_arena_init(br_arena *arena, void *buffer, usize capacity);
void br_arena_reset(br_arena *arena);

br_arena_mark br_arena_mark_save(const br_arena *arena);
br_status br_arena_rewind(br_arena *arena, br_arena_mark mark);

br_alloc_result br_arena_alloc(br_arena *arena, usize size, usize alignment);
br_alloc_result br_arena_alloc_uninit(br_arena *arena, usize size, usize alignment);
br_allocator br_arena_allocator(br_arena *arena);

BR_EXTERN_C_END

#endif
