#include <bedrock/mem/scratch.h>

static br_alloc_result br__scratch_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static uptr br__scratch_align_up(uptr value, usize alignment) {
  return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static bool br__scratch_is_self_allocator(const br_scratch *scratch, br_allocator allocator) {
  return scratch != NULL && allocator.ctx == (void *)scratch;
}

static br_allocator_fn br__scratch_allocator_fn_ptr(void);

static br_allocator br__scratch_backup_allocator(const br_scratch *scratch) {
  br_allocator allocator = {0};

  if (scratch != NULL && scratch->backup_allocator.fn != NULL) {
    allocator = scratch->backup_allocator;
  } else {
    allocator = br_allocator_heap();
  }

  if (allocator.fn == br__scratch_allocator_fn_ptr() &&
      br__scratch_is_self_allocator(scratch, allocator)) {
    allocator.fn = NULL;
    allocator.ctx = NULL;
  }

  return allocator;
}

static bool br__scratch_reserve_leaks(br_scratch *scratch, usize min_cap) {
  br_allocator allocator;
  br_alloc_result resized;
  usize new_cap;

  if (scratch == NULL) {
    return false;
  }
  if (scratch->leaked_cap >= min_cap) {
    return true;
  }

  allocator = br__scratch_backup_allocator(scratch);
  if (allocator.fn == NULL) {
    return false;
  }

  new_cap = scratch->leaked_cap != 0u ? scratch->leaked_cap : 8u;
  while (new_cap < min_cap) {
    if (new_cap > SIZE_MAX / 2u) {
      new_cap = min_cap;
      break;
    }
    new_cap *= 2u;
  }

  resized = br_allocator_resize_uninit(allocator,
                                       scratch->leaked_allocations,
                                       scratch->leaked_cap * sizeof(*scratch->leaked_allocations),
                                       new_cap * sizeof(*scratch->leaked_allocations),
                                       (usize) _Alignof(br_scratch_leaked_allocation));
  if (resized.status != BR_STATUS_OK) {
    return false;
  }

  scratch->leaked_allocations = (br_scratch_leaked_allocation *)resized.ptr;
  scratch->leaked_cap = new_cap;
  return true;
}

static br_status br__scratch_ensure_initialized(br_scratch *scratch) {
  br_allocator allocator;

  if (scratch == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (scratch->data != NULL) {
    return BR_STATUS_OK;
  }

  allocator = br__scratch_backup_allocator(scratch);
  if (allocator.fn == NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  return br_scratch_init(scratch, BR_SCRATCH_DEFAULT_BACKING_SIZE, allocator);
}

static br_status br__scratch_validate_alignment(usize *alignment) {
  if (alignment == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (*alignment == 0u) {
    *alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (!br_is_power_of_two_size(*alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  return BR_STATUS_OK;
}

static br_alloc_result
br__scratch_alloc_internal(br_scratch *scratch, usize size, usize alignment, bool zeroed) {
  br_allocator backup_allocator;
  br_alloc_result result;
  br_status status;
  usize aligned_size;

  if (scratch == NULL) {
    return br__scratch_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  status = br__scratch_validate_alignment(&alignment);
  if (status != BR_STATUS_OK) {
    return br__scratch_result(NULL, 0u, status);
  }
  if (size == 0u) {
    return br__scratch_result(NULL, 0u, BR_STATUS_OK);
  }

  status = br__scratch_ensure_initialized(scratch);
  if (status != BR_STATUS_OK) {
    return br__scratch_result(NULL, 0u, status);
  }

  aligned_size = size;
  if (alignment > 1u) {
    if (aligned_size > SIZE_MAX - (alignment - 1u)) {
      return br__scratch_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
    aligned_size += alignment - 1u;
  }

  if (scratch->curr_offset <= scratch->capacity &&
      aligned_size <= scratch->capacity - scratch->curr_offset) {
    uptr offset = (uptr)scratch->curr_offset;
    u8 *root = scratch->data + scratch->curr_offset;
    u8 *ptr = root;

    scratch->prev_allocation_root = root;
    if (((uptr)ptr & (uptr)(alignment - 1u)) != 0u) {
      ptr = (u8 *)(void *)br__scratch_align_up((uptr)ptr, alignment);
    }

    scratch->prev_allocation = ptr;
    scratch->curr_offset = (usize)offset + aligned_size;
    if (zeroed) {
      memset(ptr, 0, size);
    }
    return br__scratch_result(ptr, size, BR_STATUS_OK);
  }

  backup_allocator = br__scratch_backup_allocator(scratch);
  if (backup_allocator.fn == NULL) {
    return br__scratch_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }

  result = zeroed ? br_allocator_alloc(backup_allocator, size, alignment)
                  : br_allocator_alloc_uninit(backup_allocator, size, alignment);
  if (result.status != BR_STATUS_OK || result.ptr == NULL) {
    return result;
  }

  if (!br__scratch_reserve_leaks(scratch, scratch->leaked_count + 1u)) {
    (void)br_allocator_free(backup_allocator, result.ptr, result.size);
    return br__scratch_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  scratch->leaked_allocations[scratch->leaked_count].memory = result.ptr;
  scratch->leaked_allocations[scratch->leaked_count].size = result.size;
  scratch->leaked_count += 1u;
  return result;
}

static br_status br__scratch_remove_leak(br_scratch *scratch, usize index) {
  br_allocator allocator;
  br_status status;

  if (scratch == NULL || index >= scratch->leaked_count) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  allocator = br__scratch_backup_allocator(scratch);
  if (allocator.fn == NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  status = br_allocator_free(
    allocator, scratch->leaked_allocations[index].memory, scratch->leaked_allocations[index].size);
  if (status != BR_STATUS_OK) {
    return status;
  }

  if (index + 1u < scratch->leaked_count) {
    memmove(&scratch->leaked_allocations[index],
            &scratch->leaked_allocations[index + 1u],
            (scratch->leaked_count - index - 1u) * sizeof(*scratch->leaked_allocations));
  }
  scratch->leaked_count -= 1u;
  return BR_STATUS_OK;
}

static br_alloc_result br__scratch_resize_internal(
  br_scratch *scratch, void *ptr, usize old_size, usize new_size, usize alignment, bool zeroed) {
  br_alloc_result result;
  br_status status;

  if (scratch == NULL) {
    return br__scratch_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  status = br__scratch_validate_alignment(&alignment);
  if (status != BR_STATUS_OK) {
    return br__scratch_result(NULL, 0u, status);
  }
  if (ptr == NULL) {
    return br__scratch_alloc_internal(scratch, new_size, alignment, zeroed);
  }
  if (new_size == 0u) {
    return br__scratch_result(NULL, 0u, br_scratch_free(scratch, ptr));
  }
  if (old_size == 0u) {
    return br__scratch_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  status = br__scratch_ensure_initialized(scratch);
  if (status != BR_STATUS_OK) {
    return br__scratch_result(NULL, 0u, status);
  }

  if (scratch->prev_allocation == ptr && (((uptr)ptr & (uptr)(alignment - 1u)) == 0u) &&
      (uptr)ptr <= (uptr)(scratch->data + scratch->capacity) &&
      new_size < (usize)((scratch->data + scratch->capacity) - (u8 *)ptr)) {
    scratch->curr_offset = (usize)((u8 *)ptr - scratch->data) + new_size;
    if (zeroed && new_size > old_size) {
      memset((u8 *)ptr + old_size, 0, new_size - old_size);
    }
    return br__scratch_result(ptr, new_size, BR_STATUS_OK);
  }

  result = br__scratch_alloc_internal(scratch, new_size, alignment, false);
  if (result.status != BR_STATUS_OK) {
    return result;
  }

  memcpy(result.ptr, ptr, br_min_size(old_size, new_size));
  if (zeroed && new_size > old_size) {
    memset((u8 *)result.ptr + old_size, 0, new_size - old_size);
  }

  status = br_scratch_free(scratch, ptr);
  if (status != BR_STATUS_OK) {
    return br__scratch_result(NULL, 0u, status);
  }

  result.status = BR_STATUS_OK;
  return result;
}

static br_alloc_result br__scratch_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_scratch *scratch = (br_scratch *)ctx;

  if (req == NULL) {
    return br__scratch_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__scratch_alloc_internal(scratch, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__scratch_alloc_internal(scratch, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
      return br__scratch_resize_internal(
        scratch, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__scratch_resize_internal(
        scratch, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__scratch_result(NULL, 0u, br_scratch_free(scratch, req->ptr));
    case BR_ALLOC_OP_RESET:
      br_scratch_free_all(scratch);
      return br__scratch_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__scratch_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

static br_allocator_fn br__scratch_allocator_fn_ptr(void) {
  return br__scratch_allocator_fn;
}

br_status br_scratch_init(br_scratch *scratch, usize size, br_allocator backup_allocator) {
  br_alloc_result result;

  if (scratch == NULL || size == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (scratch->data != NULL) {
    return BR_STATUS_INVALID_STATE;
  }

  if (backup_allocator.fn == NULL) {
    backup_allocator = br_allocator_heap();
  }
  if (backup_allocator.fn == br__scratch_allocator_fn_ptr() &&
      backup_allocator.ctx == (void *)scratch) {
    return BR_STATUS_INVALID_STATE;
  }

  result = br_allocator_alloc(backup_allocator, size, (usize)(2u * _Alignof(void *)));
  if (result.status != BR_STATUS_OK) {
    return result.status;
  }

  scratch->data = (u8 *)result.ptr;
  scratch->capacity = result.size;
  scratch->curr_offset = 0u;
  scratch->prev_allocation = NULL;
  scratch->prev_allocation_root = NULL;
  scratch->backup_allocator = backup_allocator;
  scratch->leaked_allocations = NULL;
  scratch->leaked_count = 0u;
  scratch->leaked_cap = 0u;
  return BR_STATUS_OK;
}

void br_scratch_destroy(br_scratch *scratch) {
  br_allocator allocator;
  usize i;

  if (scratch == NULL) {
    return;
  }

  allocator = br__scratch_backup_allocator(scratch);
  for (i = 0u; i < scratch->leaked_count; ++i) {
    if (allocator.fn != NULL) {
      (void)br_allocator_free(
        allocator, scratch->leaked_allocations[i].memory, scratch->leaked_allocations[i].size);
    }
  }
  if (scratch->leaked_allocations != NULL && allocator.fn != NULL) {
    (void)br_allocator_free(allocator,
                            scratch->leaked_allocations,
                            scratch->leaked_cap * sizeof(*scratch->leaked_allocations));
  }
  if (scratch->data != NULL && allocator.fn != NULL) {
    (void)br_allocator_free(allocator, scratch->data, scratch->capacity);
  }

  memset(scratch, 0, sizeof(*scratch));
}

void br_scratch_free_all(br_scratch *scratch) {
  br_allocator allocator;
  usize i;

  if (scratch == NULL || scratch->data == NULL) {
    return;
  }

  allocator = br__scratch_backup_allocator(scratch);
  scratch->curr_offset = 0u;
  scratch->prev_allocation = NULL;
  scratch->prev_allocation_root = NULL;

  if (allocator.fn != NULL) {
    for (i = 0u; i < scratch->leaked_count; ++i) {
      (void)br_allocator_free(
        allocator, scratch->leaked_allocations[i].memory, scratch->leaked_allocations[i].size);
    }
  }
  scratch->leaked_count = 0u;
}

br_alloc_result br_scratch_alloc(br_scratch *scratch, usize size, usize alignment) {
  return br__scratch_alloc_internal(scratch, size, alignment, true);
}

br_alloc_result br_scratch_alloc_uninit(br_scratch *scratch, usize size, usize alignment) {
  return br__scratch_alloc_internal(scratch, size, alignment, false);
}

br_status br_scratch_free(br_scratch *scratch, void *ptr) {
  uptr start;
  uptr end;
  uptr old_ptr;
  usize i;

  if (scratch == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (scratch->data == NULL) {
    return BR_STATUS_INVALID_STATE;
  }
  if (ptr == NULL) {
    return BR_STATUS_OK;
  }

  start = (uptr)scratch->data;
  end = start + scratch->capacity;
  old_ptr = (uptr)ptr;

  if (scratch->prev_allocation == ptr) {
    scratch->curr_offset = (usize)((u8 *)scratch->prev_allocation_root - scratch->data);
    scratch->prev_allocation = NULL;
    scratch->prev_allocation_root = NULL;
    return BR_STATUS_OK;
  }
  if (start <= old_ptr && old_ptr < end) {
    return BR_STATUS_OK;
  }

  for (i = 0u; i < scratch->leaked_count; ++i) {
    /*
    Bedrock compares against the requested pointer explicitly here so leaked
    allocations behave as documented even if the current Odin loop shadowing is
    read literally.
    */
    if (scratch->leaked_allocations[i].memory == ptr) {
      return br__scratch_remove_leak(scratch, i);
    }
  }

  return BR_STATUS_INVALID_ARGUMENT;
}

br_alloc_result
br_scratch_resize(br_scratch *scratch, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__scratch_resize_internal(scratch, ptr, old_size, new_size, alignment, true);
}

br_alloc_result br_scratch_resize_uninit(
  br_scratch *scratch, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__scratch_resize_internal(scratch, ptr, old_size, new_size, alignment, false);
}

br_allocator br_scratch_allocator(br_scratch *scratch) {
  br_allocator allocator;

  allocator.fn = br__scratch_allocator_fn;
  allocator.ctx = scratch;
  return allocator;
}
