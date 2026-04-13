#include <bedrock/alloc.h>

#include <stdlib.h>

typedef struct br__heap_header {
    void *base;
    usize size;
} br__heap_header;

static br_alloc_result br__alloc_result(void *ptr, usize size, br_status status) {
    br_alloc_result result;

    result.ptr = ptr;
    result.size = size;
    result.status = status;
    return result;
}

static usize br__normalize_alignment(usize alignment) {
    usize min_alignment = (usize)_Alignof(br__heap_header);

    if (alignment == 0u) {
        alignment = BR_DEFAULT_ALIGNMENT;
    }

    if (alignment < min_alignment) {
        alignment = min_alignment;
    }

    return alignment;
}

static uptr br__align_up_uintptr(uptr value, usize alignment) {
    return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static br_alloc_result br__heap_alloc(usize size, usize alignment, int zeroed) {
    br__heap_header *header;
    uptr aligned_addr;
    u8 *base;
    usize extra;
    usize total;

    alignment = br__normalize_alignment(alignment);
    if (!br_is_power_of_two_size(alignment)) {
        return br__alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
    }

    if (size == 0u) {
        return br__alloc_result(NULL, 0u, BR_STATUS_OK);
    }

    extra = sizeof(br__heap_header) + alignment - 1u;
    if (size > SIZE_MAX - extra) {
        return br__alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }

    total = size + extra;
    base = zeroed ? (u8 *)calloc(1u, total) : (u8 *)malloc(total);
    if (base == NULL) {
        return br__alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }

    aligned_addr = br__align_up_uintptr((uptr)base + sizeof(br__heap_header), alignment);
    header = (br__heap_header *)(void *)(aligned_addr - (uptr)sizeof(br__heap_header));
    header->base = base;
    header->size = size;

    return br__alloc_result((void *)aligned_addr, size, BR_STATUS_OK);
}

static br_alloc_result br__heap_resize(
    void *ptr,
    usize old_size,
    usize new_size,
    usize alignment,
    int zeroed
) {
    br__heap_header *header;
    br_alloc_result result;

    if (ptr == NULL) {
        return br__heap_alloc(new_size, alignment, zeroed);
    }

    if (new_size == 0u) {
        header = (br__heap_header *)ptr - 1;
        free(header->base);
        return br__alloc_result(NULL, 0u, BR_STATUS_OK);
    }

    header = (br__heap_header *)ptr - 1;
    if (old_size == 0u) {
        old_size = header->size;
    }

    result = br__heap_alloc(new_size, alignment, zeroed);
    if (result.status != BR_STATUS_OK) {
        return result;
    }

    memcpy(result.ptr, ptr, br_min_size(old_size, new_size));
    free(header->base);
    return result;
}

static br_alloc_result br__heap_allocator_fn(void *ctx, const br_alloc_request *req) {
    (void)ctx;

    if (req == NULL) {
        return br__alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
    }

    switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
        return br__heap_alloc(req->size, req->alignment, 1);
    case BR_ALLOC_OP_ALLOC_UNINIT:
        return br__heap_alloc(req->size, req->alignment, 0);
    case BR_ALLOC_OP_RESIZE:
        return br__heap_resize(req->ptr, req->old_size, req->size, req->alignment, 1);
    case BR_ALLOC_OP_RESIZE_UNINIT:
        return br__heap_resize(req->ptr, req->old_size, req->size, req->alignment, 0);
    case BR_ALLOC_OP_FREE:
        if (req->ptr != NULL) {
            br__heap_header *header = (br__heap_header *)req->ptr - 1;
            free(header->base);
        }
        return br__alloc_result(NULL, 0u, BR_STATUS_OK);
    case BR_ALLOC_OP_RESET:
        return br__alloc_result(NULL, 0u, BR_STATUS_OK);
    }

    return br__alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

static br_alloc_result br__null_allocator_fn(void *ctx, const br_alloc_request *req) {
    (void)ctx;
    (void)req;
    return br__alloc_result(NULL, 0u, BR_STATUS_OK);
}

static br_alloc_result br__fail_allocator_fn(void *ctx, const br_alloc_request *req) {
    (void)ctx;

    if (req == NULL) {
        return br__alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
    }

    switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
    case BR_ALLOC_OP_ALLOC_UNINIT:
    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
        return br__alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    case BR_ALLOC_OP_FREE:
    case BR_ALLOC_OP_RESET:
        return br__alloc_result(NULL, 0u, BR_STATUS_OK);
    }

    return br__alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

br_alloc_result br_allocator_call(br_allocator allocator, const br_alloc_request *req) {
    if (allocator.fn == NULL) {
        return br__alloc_result(NULL, 0u, BR_STATUS_NOT_SUPPORTED);
    }

    return allocator.fn(allocator.ctx, req);
}

br_alloc_result br_allocator_alloc(br_allocator allocator, usize size, usize alignment) {
    br_alloc_request req;

    req.op = BR_ALLOC_OP_ALLOC;
    req.ptr = NULL;
    req.old_size = 0u;
    req.size = size;
    req.alignment = alignment;
    return br_allocator_call(allocator, &req);
}

br_alloc_result br_allocator_alloc_uninit(br_allocator allocator, usize size, usize alignment) {
    br_alloc_request req;

    req.op = BR_ALLOC_OP_ALLOC_UNINIT;
    req.ptr = NULL;
    req.old_size = 0u;
    req.size = size;
    req.alignment = alignment;
    return br_allocator_call(allocator, &req);
}

br_alloc_result br_allocator_resize(
    br_allocator allocator,
    void *ptr,
    usize old_size,
    usize new_size,
    usize alignment
) {
    br_alloc_request req;

    req.op = BR_ALLOC_OP_RESIZE;
    req.ptr = ptr;
    req.old_size = old_size;
    req.size = new_size;
    req.alignment = alignment;
    return br_allocator_call(allocator, &req);
}

br_alloc_result br_allocator_resize_uninit(
    br_allocator allocator,
    void *ptr,
    usize old_size,
    usize new_size,
    usize alignment
) {
    br_alloc_request req;

    req.op = BR_ALLOC_OP_RESIZE_UNINIT;
    req.ptr = ptr;
    req.old_size = old_size;
    req.size = new_size;
    req.alignment = alignment;
    return br_allocator_call(allocator, &req);
}

br_status br_allocator_free(br_allocator allocator, void *ptr, usize old_size) {
    br_alloc_request req;
    br_alloc_result result;

    req.op = BR_ALLOC_OP_FREE;
    req.ptr = ptr;
    req.old_size = old_size;
    req.size = 0u;
    req.alignment = 0u;
    result = br_allocator_call(allocator, &req);
    return result.status;
}

br_status br_allocator_reset(br_allocator allocator) {
    br_alloc_request req;
    br_alloc_result result;

    req.op = BR_ALLOC_OP_RESET;
    req.ptr = NULL;
    req.old_size = 0u;
    req.size = 0u;
    req.alignment = 0u;
    result = br_allocator_call(allocator, &req);
    return result.status;
}

br_allocator br_allocator_heap(void) {
    br_allocator allocator;

    allocator.fn = br__heap_allocator_fn;
    allocator.ctx = NULL;
    return allocator;
}

br_allocator br_allocator_null(void) {
    br_allocator allocator;

    allocator.fn = br__null_allocator_fn;
    allocator.ctx = NULL;
    return allocator;
}

br_allocator br_allocator_fail(void) {
    br_allocator allocator;

    allocator.fn = br__fail_allocator_fn;
    allocator.ctx = NULL;
    return allocator;
}
