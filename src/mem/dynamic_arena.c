#include <bedrock/mem/dynamic_arena.h>

typedef struct br__dynamic_arena_out_band_header {
  void *base;
  usize size;
  usize alignment;
} br__dynamic_arena_out_band_header;

static br__dynamic_arena_out_band_header *br__dynamic_arena_out_band_header_from_ptr(void *ptr) {
  return &((br__dynamic_arena_out_band_header *)ptr)[-1];
}

static br_status br__dynamic_arena_free_all_internal(br_dynamic_arena *arena);

static br_alloc_result br__dynamic_arena_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static br_allocator br__dynamic_arena_block_allocator(const br_dynamic_arena *arena) {
  if (arena != NULL && arena->block_allocator.fn != NULL) {
    return arena->block_allocator;
  }

  return br_allocator_heap();
}

static br_allocator br__dynamic_arena_array_allocator(const br_dynamic_arena *arena) {
  if (arena != NULL && arena->array_allocator.fn != NULL) {
    return arena->array_allocator;
  }

  return br_allocator_heap();
}

static bool br__dynamic_arena_align_size(usize size, usize alignment, usize *result) {
  usize aligned;
  usize mask;

  if (result == NULL) {
    return false;
  }
  if (alignment == 0u || !br_is_power_of_two_size(alignment)) {
    return false;
  }

  mask = alignment - 1u;
  if (size > SIZE_MAX - mask) {
    return false;
  }

  aligned = (size + mask) & ~mask;
  *result = aligned;
  return true;
}

static uptr br__dynamic_arena_align_up_ptr(uptr value, usize alignment) {
  return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static bool br__dynamic_arena_reserve_ptr_array(br_dynamic_arena *arena,
                                                void ***data,
                                                usize *cap,
                                                usize min_cap) {
  br_allocator allocator;
  br_alloc_result resized;
  usize new_cap;

  if (arena == NULL || data == NULL || cap == NULL) {
    return false;
  }
  if (*cap >= min_cap) {
    return true;
  }

  allocator = br__dynamic_arena_array_allocator(arena);
  new_cap = *cap != 0u ? *cap : 8u;
  while (new_cap < min_cap) {
    if (new_cap > SIZE_MAX / 2u) {
      new_cap = min_cap;
      break;
    }
    new_cap *= 2u;
  }

  resized = br_allocator_resize_uninit(
    allocator, *data, *cap * sizeof(**data), new_cap * sizeof(**data), (usize) _Alignof(void *));
  if (resized.status != BR_STATUS_OK) {
    return false;
  }

  *data = (void **)resized.ptr;
  *cap = new_cap;
  return true;
}

static br_status br__dynamic_arena_push_ptr(
  br_dynamic_arena *arena, void ***data, usize *count, usize *cap, void *ptr) {
  if (arena == NULL || data == NULL || count == NULL || cap == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (!br__dynamic_arena_reserve_ptr_array(arena, data, cap, *count + 1u)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  (*data)[*count] = ptr;
  *count += 1u;
  return BR_STATUS_OK;
}

static br_status
br__dynamic_arena_free_recorded(br_allocator allocator, void *ptr, usize size, usize alignment) {
  br_alloc_request req;

  req.op = BR_ALLOC_OP_FREE;
  req.ptr = ptr;
  req.old_size = size;
  req.size = 0u;
  req.alignment = alignment;
  return br_allocator_call(allocator, &req).status;
}

static br_status br__dynamic_arena_free_out_band(br_dynamic_arena *arena, void *ptr) {
  br__dynamic_arena_out_band_header *header;

  if (arena == NULL || ptr == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  header = br__dynamic_arena_out_band_header_from_ptr(ptr);
  return br__dynamic_arena_free_recorded(
    br__dynamic_arena_array_allocator(arena), header->base, header->size, header->alignment);
}

static void *br__dynamic_arena_pop_ptr(void **data, usize *count) {
  if (data == NULL || count == NULL || *count == 0u) {
    return NULL;
  }

  *count -= 1u;
  return data[*count];
}

static br_status br__dynamic_arena_cycle_new_block(br_dynamic_arena *arena, usize alignment) {
  br_alloc_result allocated;
  void *new_block;

  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (arena->block_allocator.fn == NULL && arena->array_allocator.fn == NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  /*
  Reserve the ownership slot before securing another block. If metadata growth
  fails after a block allocation, a failing cleanup free would otherwise leave
  the new block with no retained owner.
  */
  if (arena->current_block != NULL &&
      (arena->used_count == SIZE_MAX ||
       !br__dynamic_arena_reserve_ptr_array(
         arena, &arena->used_blocks, &arena->used_cap, arena->used_count + 1u))) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  new_block = br__dynamic_arena_pop_ptr(arena->unused_blocks, &arena->unused_count);
  if (new_block == NULL) {
    allocated = br_allocator_alloc(br__dynamic_arena_block_allocator(arena),
                                   arena->block_size,
                                   br_max_size(arena->minimum_alignment, alignment));
    if (allocated.status != BR_STATUS_OK) {
      return allocated.status;
    }
    new_block = allocated.ptr;
  }

  /*
  Bedrock moves the old current block into `used_blocks` only after the next
  block is secured, so a failed block allocation cannot orphan the arena's
  current block.
  */
  if (arena->current_block != NULL) {
    arena->used_blocks[arena->used_count] = arena->current_block;
    arena->used_count += 1u;
  }

  arena->bytes_left = arena->block_size;
  arena->current_pos = (u8 *)new_block;
  arena->current_block = new_block;
  return BR_STATUS_OK;
}

static br_alloc_result br__dynamic_arena_alloc_internal(br_dynamic_arena *arena,
                                                        usize size,
                                                        usize alignment,
                                                        bool zeroed) {
  br_alloc_result allocated;
  br_status status;
  usize actual_alignment;
  usize needed;
  usize margin;
  uptr aligned;
  u8 *memory;

  if (arena == NULL) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (arena->block_allocator.fn == NULL || arena->array_allocator.fn == NULL ||
      arena->block_size == 0u || arena->out_band_size == 0u || arena->minimum_alignment == 0u) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (size == 0u) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OK);
  }

  /*
  Every allocation is floored by the arena's `minimum_alignment` and honors a
  larger per-request alignment, so the effective alignment is
  `max(minimum_alignment, alignment)`. This matches Odin's documented contract
  that all allocations respect `minimum_alignment`, and we apply it to out-band
  allocations too (Odin's out-band path passes the raw request instead). A
  non-power-of-two effective alignment is rejected, consistent with the heap
  allocator's normalize-then-reject rule in `alloc.c`.
  */
  actual_alignment = br_max_size(arena->minimum_alignment, alignment);
  if (!br_is_power_of_two_size(actual_alignment)) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  if (size >= arena->out_band_size) {
    br__dynamic_arena_out_band_header *header;
    usize backing_alignment;
    usize prefix;
    usize total_size;
    void *data;

    if (arena->out_band_count == SIZE_MAX ||
        !br__dynamic_arena_reserve_ptr_array(
          arena, &arena->out_band_allocations, &arena->out_band_cap, arena->out_band_count + 1u)) {
      return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }

    backing_alignment =
      br_max_size(actual_alignment, (usize) _Alignof(br__dynamic_arena_out_band_header));
    if (!br__dynamic_arena_align_size(
          sizeof(br__dynamic_arena_out_band_header), actual_alignment, &prefix) ||
        size > SIZE_MAX - prefix) {
      return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
    total_size = prefix + size;

    allocated = zeroed ? br_allocator_alloc(
                           br__dynamic_arena_array_allocator(arena), total_size, backing_alignment)
                       : br_allocator_alloc_uninit(
                           br__dynamic_arena_array_allocator(arena), total_size, backing_alignment);
    if (allocated.status != BR_STATUS_OK) {
      return allocated;
    }

    data = (u8 *)allocated.ptr + prefix;
    header = br__dynamic_arena_out_band_header_from_ptr(data);
    header->base = allocated.ptr;
    header->size = allocated.size;
    header->alignment = backing_alignment;
    arena->out_band_allocations[arena->out_band_count] = data;
    arena->out_band_count += 1u;
    return br__dynamic_arena_result(data, size, BR_STATUS_OK);
  }

  if (!br__dynamic_arena_align_size(size, actual_alignment, &needed)) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }
  if (needed > arena->block_size) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  /*
  Align the bump pointer forward and account for the alignment margin, following
  Odin's per-request model. Unlike Odin, Bedrock re-aligns against the block it
  actually obtained from `cycle` rather than assuming a zero margin: a block
  reused from `unused_blocks` may have been allocated for a smaller alignment,
  so re-aligning guarantees the returned pointer always satisfies
  `actual_alignment`. A reused block too poorly aligned to fit the request
  reports OUT_OF_MEMORY instead of returning an under-aligned pointer.
  */
  aligned = br__dynamic_arena_align_up_ptr((uptr)(void *)arena->current_pos, actual_alignment);
  margin = (usize)(aligned - (uptr)(void *)arena->current_pos);
  if (arena->current_block == NULL || needed > arena->bytes_left ||
      margin > arena->bytes_left - needed) {
    status = br__dynamic_arena_cycle_new_block(arena, alignment);
    if (status != BR_STATUS_OK) {
      return br__dynamic_arena_result(NULL, 0u, status);
    }
    if (arena->current_block == NULL) {
      return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
    aligned = br__dynamic_arena_align_up_ptr((uptr)(void *)arena->current_pos, actual_alignment);
    margin = (usize)(aligned - (uptr)(void *)arena->current_pos);
    if (needed > arena->bytes_left || margin > arena->bytes_left - needed) {
      return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
  }

  memory = (u8 *)(void *)aligned;
  arena->current_pos = memory + needed;
  arena->bytes_left -= margin + needed;
  if (zeroed) {
    memset(memory, 0, size);
  }
  return br__dynamic_arena_result(memory, size, BR_STATUS_OK);
}

static br_alloc_result br__dynamic_arena_resize_internal(br_dynamic_arena *arena,
                                                         void *ptr,
                                                         usize old_size,
                                                         usize new_size,
                                                         usize alignment,
                                                         bool zeroed) {
  br_alloc_result result;
  usize actual_alignment;

  if (arena == NULL) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (ptr == NULL) {
    return br__dynamic_arena_alloc_internal(arena, new_size, alignment, zeroed);
  }
  if (new_size == 0u) {
    /*
    Odin's dynamic arena has no individual free operation. A zero-size resize is
    therefore a successful no-op that returns nil.
    */
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OK);
  }

  actual_alignment = br_max_size(arena->minimum_alignment, alignment);
  if (!br_is_power_of_two_size(actual_alignment)) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (old_size >= new_size && ((uptr)ptr & (uptr)(actual_alignment - 1u)) == 0u) {
    return br__dynamic_arena_result(ptr, new_size, BR_STATUS_OK);
  }

  result = br__dynamic_arena_alloc_internal(arena, new_size, alignment, false);
  if (result.status != BR_STATUS_OK) {
    return result;
  }

  memcpy(result.ptr, ptr, br_min_size(old_size, new_size));
  if (zeroed && new_size > old_size) {
    memset((u8 *)result.ptr + old_size, 0, new_size - old_size);
  }
  result.status = BR_STATUS_OK;
  return result;
}

static br_alloc_result br__dynamic_arena_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_dynamic_arena *arena = (br_dynamic_arena *)ctx;

  if (req == NULL) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__dynamic_arena_alloc_internal(arena, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__dynamic_arena_alloc_internal(arena, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
      return br__dynamic_arena_resize_internal(
        arena, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__dynamic_arena_resize_internal(
        arena, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__dynamic_arena_result(NULL, 0u, BR_STATUS_NOT_SUPPORTED);
    case BR_ALLOC_OP_RESET:
      return br__dynamic_arena_result(NULL, 0u, br__dynamic_arena_free_all_internal(arena));
  }

  return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

br_status br_dynamic_arena_init(br_dynamic_arena *arena,
                                br_allocator block_allocator,
                                br_allocator array_allocator,
                                usize block_size,
                                usize out_band_size,
                                usize minimum_alignment) {
  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (arena->unused_blocks != NULL || arena->used_blocks != NULL ||
      arena->out_band_allocations != NULL || arena->current_block != NULL ||
      arena->unused_count != 0u || arena->used_count != 0u || arena->out_band_count != 0u) {
    return BR_STATUS_INVALID_STATE;
  }

  if (block_allocator.fn == NULL) {
    block_allocator = br_allocator_heap();
  }
  if (array_allocator.fn == NULL) {
    array_allocator = br_allocator_heap();
  }
  if (block_size == 0u) {
    block_size = BR_DYNAMIC_ARENA_DEFAULT_BLOCK_SIZE;
  }
  if (out_band_size == 0u) {
    out_band_size = BR_DYNAMIC_ARENA_DEFAULT_OUT_BAND_SIZE;
  }
  if (minimum_alignment == 0u) {
    minimum_alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (!br_is_power_of_two_size(minimum_alignment) || block_size == 0u || out_band_size == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  arena->block_size = block_size;
  arena->out_band_size = out_band_size;
  arena->minimum_alignment = minimum_alignment;
  arena->block_allocator = block_allocator;
  arena->array_allocator = array_allocator;
  arena->unused_blocks = NULL;
  arena->unused_count = 0u;
  arena->unused_cap = 0u;
  arena->used_blocks = NULL;
  arena->used_count = 0u;
  arena->used_cap = 0u;
  arena->out_band_allocations = NULL;
  arena->out_band_count = 0u;
  arena->out_band_cap = 0u;
  arena->current_block = NULL;
  arena->current_pos = NULL;
  arena->bytes_left = 0u;
  return BR_STATUS_OK;
}

void br_dynamic_arena_destroy(br_dynamic_arena *arena) {
  br_allocator array_allocator;

  if (arena == NULL) {
    return;
  }

  array_allocator = br__dynamic_arena_array_allocator(arena);
  if (br__dynamic_arena_free_all_internal(arena) != BR_STATUS_OK) {
    return;
  }
  if (arena->unused_blocks != NULL) {
    if (br_allocator_free(array_allocator,
                          arena->unused_blocks,
                          arena->unused_cap * sizeof(*arena->unused_blocks)) != BR_STATUS_OK) {
      return;
    }
    arena->unused_blocks = NULL;
    arena->unused_cap = 0u;
  }
  if (arena->used_blocks != NULL) {
    if (br_allocator_free(array_allocator,
                          arena->used_blocks,
                          arena->used_cap * sizeof(*arena->used_blocks)) != BR_STATUS_OK) {
      return;
    }
    arena->used_blocks = NULL;
    arena->used_cap = 0u;
  }
  if (arena->out_band_allocations != NULL) {
    if (br_allocator_free(array_allocator,
                          arena->out_band_allocations,
                          arena->out_band_cap * sizeof(*arena->out_band_allocations)) !=
        BR_STATUS_OK) {
      return;
    }
    arena->out_band_allocations = NULL;
    arena->out_band_cap = 0u;
  }

  memset(arena, 0, sizeof(*arena));
}

void br_dynamic_arena_reset(br_dynamic_arena *arena) {
  void *block;

  if (arena == NULL) {
    return;
  }

  if (arena->current_block != NULL) {
    if (br__dynamic_arena_push_ptr(arena,
                                   &arena->unused_blocks,
                                   &arena->unused_count,
                                   &arena->unused_cap,
                                   arena->current_block) == BR_STATUS_OK) {
      arena->current_block = NULL;
      arena->current_pos = NULL;
    } else {
      arena->current_pos = (u8 *)arena->current_block;
    }
  }

  while (arena->used_count != 0u) {
    block = arena->used_blocks[arena->used_count - 1u];
    if (br__dynamic_arena_push_ptr(
          arena, &arena->unused_blocks, &arena->unused_count, &arena->unused_cap, block) !=
        BR_STATUS_OK) {
      break;
    }
    arena->used_count -= 1u;
  }

  while (arena->out_band_count != 0u) {
    if (br__dynamic_arena_free_out_band(
          arena, arena->out_band_allocations[arena->out_band_count - 1u]) != BR_STATUS_OK) {
      break;
    }
    arena->out_band_count -= 1u;
  }
  arena->bytes_left = 0u;
  if (arena->current_block == NULL) {
    arena->current_pos = NULL;
  }
}

static br_status br__dynamic_arena_free_all_internal(br_dynamic_arena *arena) {
  br_allocator block_allocator;
  br_status first_error = BR_STATUS_OK;
  br_status status;

  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  while (arena->out_band_count != 0u) {
    status = br__dynamic_arena_free_out_band(
      arena, arena->out_band_allocations[arena->out_band_count - 1u]);
    if (status != BR_STATUS_OK) {
      first_error = status;
      break;
    }
    arena->out_band_count -= 1u;
  }

  block_allocator = br__dynamic_arena_block_allocator(arena);
  if (arena->current_block != NULL) {
    status = br_allocator_free(block_allocator, arena->current_block, arena->block_size);
    if (status == BR_STATUS_OK) {
      arena->current_block = NULL;
      arena->current_pos = NULL;
    } else if (first_error == BR_STATUS_OK) {
      first_error = status;
    }
  }
  while (arena->used_count != 0u) {
    status = br_allocator_free(
      block_allocator, arena->used_blocks[arena->used_count - 1u], arena->block_size);
    if (status != BR_STATUS_OK) {
      if (first_error == BR_STATUS_OK) {
        first_error = status;
      }
      break;
    }
    arena->used_count -= 1u;
  }
  while (arena->unused_count != 0u) {
    status = br_allocator_free(
      block_allocator, arena->unused_blocks[arena->unused_count - 1u], arena->block_size);
    if (status != BR_STATUS_OK) {
      if (first_error == BR_STATUS_OK) {
        first_error = status;
      }
      break;
    }
    arena->unused_count -= 1u;
  }

  arena->bytes_left = 0u;
  if (arena->current_block != NULL) {
    arena->current_pos = (u8 *)arena->current_block;
  }
  return first_error;
}

void br_dynamic_arena_free_all(br_dynamic_arena *arena) {
  (void)br__dynamic_arena_free_all_internal(arena);
}

br_alloc_result br_dynamic_arena_alloc(br_dynamic_arena *arena, usize size) {
  return br__dynamic_arena_alloc_internal(arena, size, 0u, true);
}

br_alloc_result br_dynamic_arena_alloc_uninit(br_dynamic_arena *arena, usize size) {
  return br__dynamic_arena_alloc_internal(arena, size, 0u, false);
}

br_alloc_result
br_dynamic_arena_resize(br_dynamic_arena *arena, void *ptr, usize old_size, usize new_size) {
  return br__dynamic_arena_resize_internal(arena, ptr, old_size, new_size, 0u, true);
}

br_alloc_result
br_dynamic_arena_resize_uninit(br_dynamic_arena *arena, void *ptr, usize old_size, usize new_size) {
  return br__dynamic_arena_resize_internal(arena, ptr, old_size, new_size, 0u, false);
}

br_allocator br_dynamic_arena_allocator(br_dynamic_arena *arena) {
  br_allocator allocator;

  allocator.fn = br__dynamic_arena_allocator_fn;
  allocator.ctx = arena;
  return allocator;
}
