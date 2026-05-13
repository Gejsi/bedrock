#include <bedrock/mem/dynamic_arena.h>

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

static void *br__dynamic_arena_pop_ptr(void **data, usize *count) {
  if (data == NULL || count == NULL || *count == 0u) {
    return NULL;
  }

  *count -= 1u;
  return data[*count];
}

static br_status br__dynamic_arena_cycle_new_block(br_dynamic_arena *arena) {
  br_alloc_result allocated;
  void *new_block;
  br_status status;

  if (arena == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (arena->block_allocator.fn == NULL && arena->array_allocator.fn == NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  new_block = br__dynamic_arena_pop_ptr(arena->unused_blocks, &arena->unused_count);
  if (new_block == NULL) {
    allocated = br_allocator_alloc(
      br__dynamic_arena_block_allocator(arena), arena->block_size, arena->alignment);
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
    status = br__dynamic_arena_push_ptr(
      arena, &arena->used_blocks, &arena->used_count, &arena->used_cap, arena->current_block);
    if (status != BR_STATUS_OK) {
      if (arena->unused_count < arena->unused_cap && arena->unused_blocks != NULL) {
        arena->unused_blocks[arena->unused_count] = new_block;
        arena->unused_count += 1u;
      } else if (allocated.ptr == new_block) {
        (void)br_allocator_free(
          br__dynamic_arena_block_allocator(arena), new_block, arena->block_size);
      }
      return status;
    }
  }

  arena->bytes_left = arena->block_size;
  arena->current_pos = (u8 *)new_block;
  arena->current_block = new_block;
  return BR_STATUS_OK;
}

static br_alloc_result
br__dynamic_arena_alloc_internal(br_dynamic_arena *arena, usize size, bool zeroed) {
  br_alloc_result allocated;
  br_status status;
  usize needed;
  void *memory;

  if (arena == NULL) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (arena->block_allocator.fn == NULL || arena->array_allocator.fn == NULL ||
      arena->block_size == 0u || arena->out_band_size == 0u || arena->alignment == 0u) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (size == 0u) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OK);
  }

  if (size >= arena->out_band_size) {
    allocated =
      zeroed ? br_allocator_alloc(br__dynamic_arena_array_allocator(arena), size, arena->alignment)
             : br_allocator_alloc_uninit(
                 br__dynamic_arena_array_allocator(arena), size, arena->alignment);
    if (allocated.status != BR_STATUS_OK) {
      return allocated;
    }
    status = br__dynamic_arena_push_ptr(arena,
                                        &arena->out_band_allocations,
                                        &arena->out_band_count,
                                        &arena->out_band_cap,
                                        allocated.ptr);
    if (status != BR_STATUS_OK) {
      (void)br_allocator_free(
        br__dynamic_arena_array_allocator(arena), allocated.ptr, allocated.size);
      return br__dynamic_arena_result(NULL, 0u, status);
    }
    return allocated;
  }

  if (!br__dynamic_arena_align_size(size, arena->alignment, &needed)) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }
  if (needed > arena->block_size) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (arena->bytes_left < needed) {
    status = br__dynamic_arena_cycle_new_block(arena);
    if (status != BR_STATUS_OK) {
      return br__dynamic_arena_result(NULL, 0u, status);
    }
    if (arena->current_block == NULL) {
      return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
  }

  memory = arena->current_pos;
  arena->current_pos += needed;
  arena->bytes_left -= needed;
  if (zeroed) {
    memset(memory, 0, size);
  }
  return br__dynamic_arena_result(memory, size, BR_STATUS_OK);
}

static br_alloc_result br__dynamic_arena_resize_internal(
  br_dynamic_arena *arena, void *ptr, usize old_size, usize new_size, bool zeroed) {
  br_alloc_result result;

  if (arena == NULL) {
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (ptr == NULL) {
    return br__dynamic_arena_alloc_internal(arena, new_size, zeroed);
  }
  if (new_size == 0u) {
    /*
    Odin's dynamic arena has no individual free operation. A zero-size resize is
    therefore a successful no-op that returns nil.
    */
    return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OK);
  }
  if (old_size >= new_size) {
    if (zeroed && new_size > old_size) {
      memset((u8 *)ptr + old_size, 0, new_size - old_size);
    }
    return br__dynamic_arena_result(ptr, new_size, BR_STATUS_OK);
  }

  result = br__dynamic_arena_alloc_internal(arena, new_size, false);
  if (result.status != BR_STATUS_OK) {
    return result;
  }

  memcpy(result.ptr, ptr, old_size);
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
      return br__dynamic_arena_alloc_internal(arena, req->size, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__dynamic_arena_alloc_internal(arena, req->size, false);
    case BR_ALLOC_OP_RESIZE:
      return br__dynamic_arena_resize_internal(arena, req->ptr, req->old_size, req->size, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__dynamic_arena_resize_internal(arena, req->ptr, req->old_size, req->size, false);
    case BR_ALLOC_OP_FREE:
      return br__dynamic_arena_result(NULL, 0u, BR_STATUS_NOT_SUPPORTED);
    case BR_ALLOC_OP_RESET:
      br_dynamic_arena_free_all(arena);
      return br__dynamic_arena_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__dynamic_arena_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

br_status br_dynamic_arena_init(br_dynamic_arena *arena,
                                br_allocator block_allocator,
                                br_allocator array_allocator,
                                usize block_size,
                                usize out_band_size,
                                usize alignment) {
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
  if (alignment == 0u) {
    alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (!br_is_power_of_two_size(alignment) || block_size == 0u || out_band_size == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  arena->block_size = block_size;
  arena->out_band_size = out_band_size;
  arena->alignment = alignment;
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
  br_dynamic_arena_free_all(arena);
  if (arena->unused_blocks != NULL) {
    (void)br_allocator_free(
      array_allocator, arena->unused_blocks, arena->unused_cap * sizeof(*arena->unused_blocks));
  }
  if (arena->used_blocks != NULL) {
    (void)br_allocator_free(
      array_allocator, arena->used_blocks, arena->used_cap * sizeof(*arena->used_blocks));
  }
  if (arena->out_band_allocations != NULL) {
    (void)br_allocator_free(array_allocator,
                            arena->out_band_allocations,
                            arena->out_band_cap * sizeof(*arena->out_band_allocations));
  }

  memset(arena, 0, sizeof(*arena));
}

void br_dynamic_arena_reset(br_dynamic_arena *arena) {
  usize i;

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
    }
  }

  for (i = 0u; i < arena->used_count; ++i) {
    if (br__dynamic_arena_push_ptr(arena,
                                   &arena->unused_blocks,
                                   &arena->unused_count,
                                   &arena->unused_cap,
                                   arena->used_blocks[i]) != BR_STATUS_OK) {
      break;
    }
  }
  arena->used_count = 0u;

  for (i = 0u; i < arena->out_band_count; ++i) {
    (void)br_allocator_free(
      br__dynamic_arena_array_allocator(arena), arena->out_band_allocations[i], 0u);
  }
  arena->out_band_count = 0u;
  arena->bytes_left = 0u;
  arena->current_pos = NULL;
}

void br_dynamic_arena_free_all(br_dynamic_arena *arena) {
  usize i;

  if (arena == NULL) {
    return;
  }

  br_dynamic_arena_reset(arena);
  if (arena->current_block != NULL) {
    (void)br_allocator_free(
      br__dynamic_arena_block_allocator(arena), arena->current_block, arena->block_size);
    arena->current_block = NULL;
    arena->current_pos = NULL;
  }
  for (i = 0u; i < arena->used_count; ++i) {
    (void)br_allocator_free(
      br__dynamic_arena_block_allocator(arena), arena->used_blocks[i], arena->block_size);
  }
  arena->used_count = 0u;
  for (i = 0u; i < arena->unused_count; ++i) {
    (void)br_allocator_free(
      br__dynamic_arena_block_allocator(arena), arena->unused_blocks[i], arena->block_size);
  }
  arena->unused_count = 0u;
}

br_alloc_result br_dynamic_arena_alloc(br_dynamic_arena *arena, usize size) {
  return br__dynamic_arena_alloc_internal(arena, size, true);
}

br_alloc_result br_dynamic_arena_alloc_uninit(br_dynamic_arena *arena, usize size) {
  return br__dynamic_arena_alloc_internal(arena, size, false);
}

br_alloc_result
br_dynamic_arena_resize(br_dynamic_arena *arena, void *ptr, usize old_size, usize new_size) {
  return br__dynamic_arena_resize_internal(arena, ptr, old_size, new_size, true);
}

br_alloc_result
br_dynamic_arena_resize_uninit(br_dynamic_arena *arena, void *ptr, usize old_size, usize new_size) {
  return br__dynamic_arena_resize_internal(arena, ptr, old_size, new_size, false);
}

br_allocator br_dynamic_arena_allocator(br_dynamic_arena *arena) {
  br_allocator allocator;

  allocator.fn = br__dynamic_arena_allocator_fn;
  allocator.ctx = arena;
  return allocator;
}
