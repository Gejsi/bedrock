#ifndef BEDROCK_ARENA_H
#define BEDROCK_ARENA_H

#include <bedrock/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_arena {
    unsigned char *buffer;
    size_t capacity;
    size_t offset;
    size_t peak;
} br_arena;

typedef struct br_arena_mark {
    size_t offset;
} br_arena_mark;

void br_arena_init(br_arena *arena, void *buffer, size_t capacity);
void br_arena_reset(br_arena *arena);

br_arena_mark br_arena_mark_save(const br_arena *arena);
br_status br_arena_rewind(br_arena *arena, br_arena_mark mark);

br_alloc_result br_arena_alloc(br_arena *arena, size_t size, size_t alignment);
br_alloc_result br_arena_alloc_uninit(br_arena *arena, size_t size, size_t alignment);
br_allocator br_arena_allocator(br_arena *arena);

BR_EXTERN_C_END

#endif
