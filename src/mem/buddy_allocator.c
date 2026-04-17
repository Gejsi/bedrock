#include <bedrock/mem/buddy_allocator.h>

struct br_buddy_block {
  usize size;
  bool is_free;
};

BR_STATIC_ASSERT((sizeof(struct br_buddy_block) & (sizeof(struct br_buddy_block) - 1u)) == 0u,
                 "Buddy block header size must stay a power of two.");

static br_alloc_result br__buddy_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static br_status br__buddy_validate_request_alignment(usize *alignment) {
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

static usize br__buddy_normalize_init_alignment(usize alignment) {
  if (alignment < sizeof(struct br_buddy_block)) {
    alignment = sizeof(struct br_buddy_block);
  }
  return alignment;
}

static usize br__buddy_block_size_required_for_alignment(usize alignment, usize size) {
  usize actual_size;
  usize rounded;

  if (alignment == 0u || size == 0u) {
    return 0u;
  }
  if (size > SIZE_MAX - alignment) {
    return 0u;
  }

  actual_size = alignment + size;
  if (br_is_power_of_two_size(actual_size)) {
    return actual_size;
  }

  rounded = 1u;
  while (rounded < actual_size) {
    if (rounded > SIZE_MAX / 2u) {
      return 0u;
    }
    rounded <<= 1u;
  }
  return rounded;
}

static usize br__buddy_min_alloc_size(const br_buddy_allocator *buddy) {
  if (buddy == NULL) {
    return 0u;
  }
  return br__buddy_block_size_required_for_alignment(buddy->alignment, 1u);
}

static br_status br__buddy_validate_initialized(const br_buddy_allocator *buddy) {
  if (buddy == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (buddy->head == NULL || buddy->tail == NULL || buddy->alignment == 0u) {
    return BR_STATUS_INVALID_STATE;
  }
  return BR_STATUS_OK;
}

static br_buddy_block *br__buddy_block_next(br_buddy_block *block) {
  return (br_buddy_block *)(void *)((u8 *)(void *)block + block->size);
}

static br_buddy_block *br__buddy_block_split(br_buddy_block *block, usize size) {
  if (block != NULL && size != 0u) {
    while (size < block->size) {
      usize half = block->size >> 1u;

      block->size = half;
      block = br__buddy_block_next(block);
      block->size = half;
      block->is_free = true;
    }
    if (size <= block->size) {
      return block;
    }
  }

  return NULL;
}

static void br__buddy_block_coalescence(br_buddy_block *head, br_buddy_block *tail) {
  for (;;) {
    br_buddy_block *block = head;
    br_buddy_block *buddy = br__buddy_block_next(block);
    bool no_coalescence = true;

    while (block < tail && buddy < tail) {
      if (block->is_free && buddy->is_free && block->size == buddy->size) {
        block->size <<= 1u;
        block = br__buddy_block_next(block);
        if (block < tail) {
          buddy = br__buddy_block_next(block);
          no_coalescence = false;
        }
      } else if (block->size < buddy->size) {
        block = buddy;
        buddy = br__buddy_block_next(buddy);
      } else {
        block = br__buddy_block_next(buddy);
        if (block < tail) {
          buddy = br__buddy_block_next(block);
        }
      }
    }

    if (no_coalescence) {
      return;
    }
  }
}

static br_buddy_block *
br__buddy_block_find_best(br_buddy_block *head, br_buddy_block *tail, usize size) {
  br_buddy_block *best_block = NULL;
  br_buddy_block *block = head;
  br_buddy_block *buddy = br__buddy_block_next(block);

  if (buddy == tail && block->is_free) {
    return br__buddy_block_split(block, size);
  }

  while (block < tail && buddy < tail) {
    if (block->is_free && buddy->is_free && block->size == buddy->size) {
      block->size <<= 1u;
      if (size <= block->size && (best_block == NULL || block->size <= best_block->size)) {
        best_block = block;
      }
      block = br__buddy_block_next(buddy);
      if (block < tail) {
        buddy = br__buddy_block_next(block);
      }
      continue;
    }
    if (block->is_free && size <= block->size &&
        (best_block == NULL || block->size <= best_block->size)) {
      best_block = block;
    }
    if (buddy->is_free && size <= buddy->size &&
        (best_block == NULL || buddy->size < best_block->size)) {
      best_block = buddy;
    }

    if (block->size <= buddy->size) {
      block = br__buddy_block_next(buddy);
      if (block < tail) {
        buddy = br__buddy_block_next(block);
      }
    } else {
      block = buddy;
      buddy = br__buddy_block_next(buddy);
    }
  }

  if (best_block != NULL) {
    return br__buddy_block_split(best_block, size);
  }
  return NULL;
}

static br_status
br__buddy_get_block(const br_buddy_allocator *buddy, void *ptr, br_buddy_block **out_block) {
  uptr head_addr;
  uptr tail_addr;
  uptr ptr_addr;
  uptr block_addr;
  br_buddy_block *block;
  usize min_size;

  if (out_block == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  *out_block = NULL;

  if (ptr == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (br__buddy_validate_initialized(buddy) != BR_STATUS_OK) {
    return BR_STATUS_INVALID_STATE;
  }

  head_addr = (uptr)(void *)buddy->head;
  tail_addr = (uptr)(void *)buddy->tail;
  ptr_addr = (uptr)ptr;
  if (!(head_addr <= ptr_addr && ptr_addr <= tail_addr)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if ((ptr_addr & (uptr)(buddy->alignment - 1u)) != 0u || ptr_addr < head_addr + buddy->alignment) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  block_addr = ptr_addr - buddy->alignment;
  if (!(head_addr <= block_addr && block_addr < tail_addr)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  block = (br_buddy_block *)(void *)block_addr;
  min_size = br__buddy_min_alloc_size(buddy);
  if (!br_is_power_of_two_size(block->size) || block->size < min_size ||
      block->size > tail_addr - block_addr) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if ((uptr)(void *)br__buddy_block_next(block) > tail_addr) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  *out_block = block;
  return BR_STATUS_OK;
}

static br_alloc_result
br__buddy_alloc_internal(br_buddy_allocator *buddy, usize size, bool zeroed) {
  br_buddy_block *found;
  usize actual_size;

  if (buddy == NULL) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (br__buddy_validate_initialized(buddy) != BR_STATUS_OK) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (size == 0u) {
    return br__buddy_result(NULL, 0u, BR_STATUS_OK);
  }

  actual_size = br__buddy_block_size_required_for_alignment(buddy->alignment, size);
  if (actual_size == 0u) {
    return br__buddy_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  found = br__buddy_block_find_best(buddy->head, buddy->tail, actual_size);
  if (found == NULL) {
    br__buddy_block_coalescence(buddy->head, buddy->tail);
    found = br__buddy_block_find_best(buddy->head, buddy->tail, actual_size);
  }
  if (found == NULL) {
    return br__buddy_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  found->is_free = false;
  if (zeroed) {
    memset((u8 *)(void *)found + buddy->alignment, 0, size);
  }
  return br__buddy_result((u8 *)(void *)found + buddy->alignment, size, BR_STATUS_OK);
}

static br_alloc_result br__buddy_resize_internal(br_buddy_allocator *buddy,
                                                 void *ptr,
                                                 usize old_size,
                                                 usize new_size,
                                                 usize alignment,
                                                 bool zeroed) {
  br_buddy_block *block;
  br_alloc_result result;
  br_status status;
  usize payload_size;

  if (buddy == NULL) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (br__buddy_validate_initialized(buddy) != BR_STATUS_OK) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (br__buddy_validate_request_alignment(&alignment) != BR_STATUS_OK) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (ptr == NULL) {
    return br__buddy_alloc_internal(buddy, new_size, zeroed);
  }
  if (new_size == 0u) {
    return br__buddy_result(NULL, 0u, br_buddy_allocator_free(buddy, ptr));
  }
  if (old_size == 0u) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  status = br__buddy_get_block(buddy, ptr, &block);
  if (status != BR_STATUS_OK) {
    return br__buddy_result(NULL, 0u, status);
  }

  /*
  Odin's default resize helper receives a typed `[]byte` old slice. Bedrock's
  raw C resize API gets only `ptr + old_size`, so we bound-check the caller's
  `old_size` against the owning buddy block before copying.
  */
  payload_size = block->size - buddy->alignment;
  if (old_size > payload_size) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (old_size == new_size && ((uptr)ptr & (uptr)(alignment - 1u)) == 0u) {
    return br__buddy_result(ptr, new_size, BR_STATUS_OK);
  }

  result = br__buddy_alloc_internal(buddy, new_size, false);
  if (result.status != BR_STATUS_OK) {
    return result;
  }

  memcpy(result.ptr, ptr, br_min_size(old_size, new_size));
  if (zeroed && new_size > old_size) {
    memset((u8 *)result.ptr + old_size, 0, new_size - old_size);
  }

  (void)br_buddy_allocator_free(buddy, ptr);
  return br__buddy_result(result.ptr, result.size, BR_STATUS_OK);
}

static br_alloc_result br__buddy_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_buddy_allocator *buddy = (br_buddy_allocator *)ctx;

  if (req == NULL) {
    return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__buddy_alloc_internal(buddy, req->size, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__buddy_alloc_internal(buddy, req->size, false);
    case BR_ALLOC_OP_RESIZE:
      return br__buddy_resize_internal(
        buddy, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__buddy_resize_internal(
        buddy, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__buddy_result(NULL, 0u, br_buddy_allocator_free(buddy, req->ptr));
    case BR_ALLOC_OP_RESET:
      br_buddy_allocator_free_all(buddy);
      return br__buddy_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__buddy_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

br_status
br_buddy_allocator_init(br_buddy_allocator *buddy, void *buffer, usize capacity, usize alignment) {
  usize min_capacity;

  if (buddy == NULL || buffer == NULL || capacity == 0u || alignment == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (!br_is_power_of_two_size(capacity) || !br_is_power_of_two_size(alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  alignment = br__buddy_normalize_init_alignment(alignment);
  if (((uptr)buffer & (uptr)(alignment - 1u)) != 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  min_capacity = br__buddy_block_size_required_for_alignment(alignment, 1u);
  if (min_capacity == 0u || capacity < min_capacity * 2u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  buddy->head = (br_buddy_block *)buffer;
  buddy->head->size = capacity;
  buddy->head->is_free = true;
  buddy->tail = br__buddy_block_next(buddy->head);
  buddy->alignment = alignment;
  return BR_STATUS_OK;
}

void br_buddy_allocator_free_all(br_buddy_allocator *buddy) {
  void *buffer;
  usize capacity;
  usize alignment;

  if (buddy == NULL || buddy->head == NULL || buddy->tail == NULL || buddy->alignment == 0u) {
    return;
  }

  buffer = (void *)buddy->head;
  capacity = (usize)((u8 *)(void *)buddy->tail - (u8 *)(void *)buddy->head);
  alignment = buddy->alignment;
  (void)br_buddy_allocator_init(buddy, buffer, capacity, alignment);
}

br_alloc_result br_buddy_allocator_alloc(br_buddy_allocator *buddy, usize size) {
  return br__buddy_alloc_internal(buddy, size, true);
}

br_alloc_result br_buddy_allocator_alloc_uninit(br_buddy_allocator *buddy, usize size) {
  return br__buddy_alloc_internal(buddy, size, false);
}

br_status br_buddy_allocator_free(br_buddy_allocator *buddy, void *ptr) {
  br_buddy_block *block;
  br_status status;

  if (ptr == NULL) {
    return BR_STATUS_OK;
  }
  status = br__buddy_get_block(buddy, ptr, &block);
  if (status != BR_STATUS_OK) {
    return status;
  }

  block->is_free = true;
  br__buddy_block_coalescence(buddy->head, buddy->tail);
  return BR_STATUS_OK;
}

br_allocator br_buddy_allocator_allocator(br_buddy_allocator *buddy) {
  br_allocator allocator = {0};

  allocator.fn = br__buddy_allocator_fn;
  allocator.ctx = buddy;
  return allocator;
}
