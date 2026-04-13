#include <bedrock/arena.h>

static br_alloc_result br__arena_result(void *ptr, usize size, br_status status) {
    br_alloc_result result;

    result.ptr = ptr;
    result.size = size;
    result.status = status;
    return result;
}

static uptr br__arena_align_up(uptr value, usize alignment) {
    return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static br_alloc_result br__arena_alloc(br_arena *arena, usize size, usize alignment, int zeroed) {
    uptr base_addr;
    uptr aligned_addr;
    usize new_offset;
    usize start;

    if (arena == NULL) {
        return br__arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
    }

    if (alignment == 0u) {
        alignment = BR_DEFAULT_ALIGNMENT;
    }

    if (!br_is_power_of_two_size(alignment)) {
        return br__arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
    }

    if (size == 0u) {
        return br__arena_result(NULL, 0u, BR_STATUS_OK);
    }

    base_addr = (uptr)arena->buffer;
    aligned_addr = br__arena_align_up(base_addr + arena->offset, alignment);
    start = (usize)(aligned_addr - base_addr);

    if (start > arena->capacity || size > (arena->capacity - start)) {
        return br__arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }

    new_offset = start + size;
    if (zeroed) {
        memset((void *)aligned_addr, 0, size);
    }

    arena->offset = new_offset;
    if (arena->peak < new_offset) {
        arena->peak = new_offset;
    }

    return br__arena_result((void *)aligned_addr, size, BR_STATUS_OK);
}

static br_alloc_result br__arena_allocator_fn(void *ctx, const br_alloc_request *req) {
    br_alloc_result result;
    br_arena *arena = (br_arena *)ctx;

    if (req == NULL) {
        return br__arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
    }

    switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
        return br__arena_alloc(arena, req->size, req->alignment, 1);
    case BR_ALLOC_OP_ALLOC_UNINIT:
        return br__arena_alloc(arena, req->size, req->alignment, 0);
    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
        if (req->ptr == NULL) {
            return br__arena_alloc(arena, req->size, req->alignment, req->op == BR_ALLOC_OP_RESIZE);
        }
        if (req->old_size == 0u) {
            return br__arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
        }

        result = br__arena_alloc(arena, req->size, req->alignment, req->op == BR_ALLOC_OP_RESIZE);
        if (result.status != BR_STATUS_OK) {
            return result;
        }

        memcpy(result.ptr, req->ptr, br_min_size(req->old_size, req->size));
        return result;
    case BR_ALLOC_OP_FREE:
        return br__arena_result(NULL, 0u, BR_STATUS_OK);
    case BR_ALLOC_OP_RESET:
        br_arena_reset(arena);
        return br__arena_result(NULL, 0u, BR_STATUS_OK);
    }

    return br__arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

void br_arena_init(br_arena *arena, void *buffer, usize capacity) {
    if (arena == NULL) {
        return;
    }

    arena->buffer = (u8 *)buffer;
    arena->capacity = capacity;
    arena->offset = 0u;
    arena->peak = 0u;
}

void br_arena_reset(br_arena *arena) {
    if (arena == NULL) {
        return;
    }

    arena->offset = 0u;
}

br_arena_mark br_arena_mark_save(const br_arena *arena) {
    br_arena_mark mark;

    mark.offset = arena != NULL ? arena->offset : 0u;
    return mark;
}

br_status br_arena_rewind(br_arena *arena, br_arena_mark mark) {
    if (arena == NULL) {
        return BR_STATUS_INVALID_ARGUMENT;
    }

    if (mark.offset > arena->offset) {
        return BR_STATUS_INVALID_ARGUMENT;
    }

    arena->offset = mark.offset;
    return BR_STATUS_OK;
}

br_alloc_result br_arena_alloc(br_arena *arena, usize size, usize alignment) {
    return br__arena_alloc(arena, size, alignment, 1);
}

br_alloc_result br_arena_alloc_uninit(br_arena *arena, usize size, usize alignment) {
    return br__arena_alloc(arena, size, alignment, 0);
}

br_allocator br_arena_allocator(br_arena *arena) {
    br_allocator allocator;

    allocator.fn = br__arena_allocator_fn;
    allocator.ctx = arena;
    return allocator;
}
