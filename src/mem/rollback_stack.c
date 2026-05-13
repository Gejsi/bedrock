#include <bedrock/mem/rollback_stack.h>

#define BR__ROLLBACK_HEADER_PREV_OFFSET_MASK UINT64_C(0xffffffff)
#define BR__ROLLBACK_HEADER_IS_FREE_SHIFT 32u
#define BR__ROLLBACK_HEADER_PREV_PTR_SHIFT 33u
#define BR__ROLLBACK_HEADER_PREV_PTR_MASK UINT64_C(0x7fffffff)

typedef u64 br__rollback_header_bits;

struct br_rollback_stack_block {
  struct br_rollback_stack_block *next_block;
  void *last_alloc;
  usize offset;
  usize capacity;
  u8 buffer[];
};

static br_alloc_result br__rollback_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static bool br__rollback_add_overflow(usize a, usize b, usize *out) {
  if (a > SIZE_MAX - b) {
    return true;
  }

  if (out != NULL) {
    *out = a + b;
  }
  return false;
}

static br_allocator br__rollback_block_allocator(br_allocator allocator) {
  if (allocator.fn != NULL) {
    return allocator;
  }

  return br_allocator_heap();
}

static usize br__rollback_normalize_alignment(usize alignment) {
  usize min_alignment = (usize) _Alignof(br__rollback_header_bits);

  if (alignment == 0u) {
    alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (alignment < min_alignment) {
    alignment = min_alignment;
  }

  return alignment;
}

static uptr br__rollback_align_up(uptr value, usize alignment) {
  return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static usize br__rollback_calc_padding_with_header(uptr start, usize alignment) {
  uptr user_ptr;

  user_ptr = br__rollback_align_up(start + sizeof(br__rollback_header_bits), alignment);
  return (usize)(user_ptr - start);
}

static br__rollback_header_bits
br__rollback_pack_header(usize prev_offset, usize prev_ptr, bool is_free) {
  br__rollback_header_bits bits = 0u;

  bits |= (br__rollback_header_bits)(prev_offset & BR__ROLLBACK_HEADER_PREV_OFFSET_MASK);
  bits |= (br__rollback_header_bits)(is_free ? 1u : 0u) << BR__ROLLBACK_HEADER_IS_FREE_SHIFT;
  bits |= (br__rollback_header_bits)(prev_ptr & BR__ROLLBACK_HEADER_PREV_PTR_MASK)
          << BR__ROLLBACK_HEADER_PREV_PTR_SHIFT;
  return bits;
}

static usize br__rollback_header_prev_offset(br__rollback_header_bits bits) {
  return (usize)(bits & BR__ROLLBACK_HEADER_PREV_OFFSET_MASK);
}

static usize br__rollback_header_prev_ptr(br__rollback_header_bits bits) {
  return (usize)((bits >> BR__ROLLBACK_HEADER_PREV_PTR_SHIFT) & BR__ROLLBACK_HEADER_PREV_PTR_MASK);
}

static bool br__rollback_header_is_free(br__rollback_header_bits bits) {
  return ((bits >> BR__ROLLBACK_HEADER_IS_FREE_SHIFT) & 1u) != 0u;
}

static br__rollback_header_bits br__rollback_header_set_free(br__rollback_header_bits bits,
                                                             bool is_free) {
  bits &= ~((br__rollback_header_bits)1u << BR__ROLLBACK_HEADER_IS_FREE_SHIFT);
  bits |= (br__rollback_header_bits)(is_free ? 1u : 0u) << BR__ROLLBACK_HEADER_IS_FREE_SHIFT;
  return bits;
}

static br__rollback_header_bits *br__rollback_header_ptr(void *ptr) {
  return (br__rollback_header_bits *)((u8 *)ptr - sizeof(br__rollback_header_bits));
}

static bool br__rollback_ptr_in_bounds(const br_rollback_stack_block *block, const void *ptr) {
  const u8 *start;
  const u8 *end;
  const u8 *memory = (const u8 *)ptr;

  if (block == NULL || ptr == NULL) {
    return false;
  }

  start = block->buffer;
  end = block->buffer + block->offset;
  return start < memory && memory <= end;
}

static br_status br__rollback_find_ptr(br_rollback_stack *stack,
                                       void *ptr,
                                       br_rollback_stack_block **parent_out,
                                       br_rollback_stack_block **block_out,
                                       br__rollback_header_bits **header_out) {
  br_rollback_stack_block *parent = NULL;
  br_rollback_stack_block *block;

  if (stack == NULL || ptr == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  for (block = stack->head; block != NULL; block = block->next_block) {
    if (br__rollback_ptr_in_bounds(block, ptr)) {
      if (parent_out != NULL) {
        *parent_out = parent;
      }
      if (block_out != NULL) {
        *block_out = block;
      }
      if (header_out != NULL) {
        *header_out = br__rollback_header_ptr(ptr);
      }
      return BR_STATUS_OK;
    }
    parent = block;
  }

  return BR_STATUS_INVALID_ARGUMENT;
}

static bool br__rollback_find_last_alloc(br_rollback_stack *stack,
                                         void *ptr,
                                         br_rollback_stack_block **block_out,
                                         br__rollback_header_bits **header_out) {
  br_rollback_stack_block *block;

  if (stack == NULL || ptr == NULL) {
    return false;
  }

  for (block = stack->head; block != NULL; block = block->next_block) {
    if (block->last_alloc == ptr) {
      if (block_out != NULL) {
        *block_out = block;
      }
      if (header_out != NULL) {
        *header_out = br__rollback_header_ptr(ptr);
      }
      return true;
    }
  }

  return false;
}

static bool br__rollback_block_is_singleton(const br_rollback_stack *stack,
                                            const br_rollback_stack_block *block) {
  return stack != NULL && block != NULL && block->capacity > stack->block_size;
}

static void br__rollback_collapse_freed_tail(br_rollback_stack_block *block) {
  while (block != NULL && block->offset > 0u && block->last_alloc != NULL) {
    br__rollback_header_bits bits = *br__rollback_header_ptr(block->last_alloc);
    usize prev_offset;
    usize prev_ptr;

    if (!br__rollback_header_is_free(bits)) {
      break;
    }

    prev_offset = br__rollback_header_prev_offset(bits);
    prev_ptr = br__rollback_header_prev_ptr(bits);
    block->offset = prev_offset;

    /*
    Bedrock stops here instead of blindly stepping to `prev_ptr - header_size`
    when the chain reaches the start of the block. This avoids underflow on the
    no-previous-allocation case while preserving Odin's rollback behavior.
    */
    if (block->offset == 0u || prev_ptr == 0u) {
      block->last_alloc = NULL;
      break;
    }

    block->last_alloc = block->buffer + prev_ptr;
  }
}

static br_status br__rollback_make_block(usize capacity,
                                         br_allocator allocator,
                                         br_rollback_stack_block **block_out) {
  br_alloc_result allocated;
  usize total_size;
  br_rollback_stack_block *block;

  if (block_out == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (br__rollback_add_overflow(offsetof(br_rollback_stack_block, buffer), capacity, &total_size)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  allocated =
    br_allocator_alloc_uninit(allocator, total_size, (usize) _Alignof(br_rollback_stack_block));
  if (allocated.status != BR_STATUS_OK || allocated.ptr == NULL) {
    return allocated.status;
  }

  block = (br_rollback_stack_block *)allocated.ptr;
  memset(block, 0, offsetof(br_rollback_stack_block, buffer));
  block->capacity = capacity;
  *block_out = block;
  return BR_STATUS_OK;
}

static void br__rollback_free_block(br_rollback_stack_block *block, br_allocator allocator) {
  usize total_size;

  if (block == NULL) {
    return;
  }

  total_size = offsetof(br_rollback_stack_block, buffer) + block->capacity;
  (void)br_allocator_free(allocator, block, total_size);
}

static void br__rollback_reset_head(br_rollback_stack *stack) {
  if (stack == NULL || stack->head == NULL) {
    return;
  }

  stack->head->next_block = NULL;
  stack->head->last_alloc = NULL;
  stack->head->offset = 0u;
}

static void br__rollback_free_all_extra_blocks(br_rollback_stack *stack) {
  br_rollback_stack_block *block;
  br_rollback_stack_block *next;
  br_allocator allocator;

  if (stack == NULL || stack->head == NULL || stack->block_allocator.fn == NULL) {
    return;
  }

  allocator = br__rollback_block_allocator(stack->block_allocator);
  block = stack->head->next_block;
  while (block != NULL) {
    next = block->next_block;
    br__rollback_free_block(block, allocator);
    block = next;
  }

  br__rollback_reset_head(stack);
}

static br_alloc_result
br__rollback_alloc(br_rollback_stack *stack, usize size, usize alignment, bool zeroed) {
  br_rollback_stack_block *block;
  br_rollback_stack_block *parent = NULL;
  br_allocator allocator;

  if (stack == NULL || stack->head == NULL) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }

  alignment = br__rollback_normalize_alignment(alignment);
  if (!br_is_power_of_two_size(alignment)) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (size == 0u) {
    return br__rollback_result(NULL, 0u, BR_STATUS_OK);
  }

  allocator = br__rollback_block_allocator(stack->block_allocator);

  for (block = stack->head;; block = block->next_block) {
    usize minimum_size_required;
    usize new_block_size;
    uptr start_addr;
    usize padding;
    usize needed;
    u8 *ptr;
    br__rollback_header_bits *header_ptr;
    usize prev_ptr;

    if (block == NULL) {
      br_status status;

      if (stack->block_allocator.fn == NULL) {
        return br__rollback_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
      }

      minimum_size_required = sizeof(br__rollback_header_bits);
      if (br__rollback_add_overflow(
            minimum_size_required, alignment - 1u, &minimum_size_required) ||
          br__rollback_add_overflow(minimum_size_required, size, &minimum_size_required)) {
        return br__rollback_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
      }
      new_block_size = stack->block_size;
      if (new_block_size < minimum_size_required) {
        new_block_size = minimum_size_required;
      }

      status = br__rollback_make_block(new_block_size, allocator, &block);
      if (status != BR_STATUS_OK) {
        return br__rollback_result(NULL, 0u, status);
      }

      parent->next_block = block;
    }

    start_addr = (uptr)(block->buffer + block->offset);
    padding = br__rollback_calc_padding_with_header(start_addr, alignment);
    if (br__rollback_add_overflow(padding, size, &needed) ||
        needed > block->capacity - br_min_size(block->offset, block->capacity)) {
      parent = block;
      continue;
    }
    if (block->offset > block->capacity || needed > block->capacity - block->offset) {
      parent = block;
      continue;
    }

    prev_ptr = block->last_alloc != NULL ? (usize)((u8 *)block->last_alloc - block->buffer) : 0u;
    if (block->offset > UINT32_MAX || prev_ptr > BR_ROLLBACK_STACK_MAX_HEAD_BLOCK_SIZE) {
      return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_STATE);
    }

    ptr = block->buffer + block->offset + padding;
    header_ptr = (br__rollback_header_bits *)(void *)(ptr - sizeof(br__rollback_header_bits));
    *header_ptr = br__rollback_pack_header(block->offset, prev_ptr, false);

    block->last_alloc = ptr;
    block->offset += needed;
    if (br__rollback_block_is_singleton(stack, block)) {
      block->offset = block->capacity;
    }

    if (zeroed) {
      memset(ptr, 0, size);
    }

    return br__rollback_result(ptr, size, BR_STATUS_OK);
  }
}

static br_status br__rollback_free(br_rollback_stack *stack, void *ptr) {
  br_rollback_stack_block *parent = NULL;
  br_rollback_stack_block *block = NULL;
  br__rollback_header_bits *header_ptr = NULL;
  br__rollback_header_bits bits;
  br_status status;

  if (stack == NULL || stack->head == NULL) {
    return BR_STATUS_INVALID_STATE;
  }
  if (ptr == NULL) {
    return BR_STATUS_OK;
  }

  status = br__rollback_find_ptr(stack, ptr, &parent, &block, &header_ptr);
  if (status != BR_STATUS_OK) {
    return status;
  }

  bits = *header_ptr;
  if (br__rollback_header_is_free(bits)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  *header_ptr = br__rollback_header_set_free(bits, true);
  if (block->last_alloc == ptr) {
    block->offset = br__rollback_header_prev_offset(bits);
    if (block->offset == 0u || br__rollback_header_prev_ptr(bits) == 0u) {
      block->last_alloc = NULL;
    } else {
      block->last_alloc = block->buffer + br__rollback_header_prev_ptr(bits);
      br__rollback_collapse_freed_tail(block);
    }
  }

  if (parent != NULL && block->offset == 0u) {
    parent->next_block = block->next_block;
    br__rollback_free_block(block, br__rollback_block_allocator(stack->block_allocator));
  }

  return BR_STATUS_OK;
}

static br_alloc_result br__rollback_resize(br_rollback_stack *stack,
                                           void *ptr,
                                           usize old_size,
                                           usize new_size,
                                           usize alignment,
                                           bool zeroed) {
  br_rollback_stack_block *block = NULL;
  br__rollback_header_bits *header_ptr = NULL;
  br_alloc_result result;
  usize new_offset;
  usize copy_size;

  if (stack == NULL || stack->head == NULL) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }

  alignment = br__rollback_normalize_alignment(alignment);
  if (!br_is_power_of_two_size(alignment)) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  if (ptr == NULL) {
    return br__rollback_alloc(stack, new_size, alignment, zeroed);
  }
  if (new_size == 0u) {
    return br__rollback_result(NULL, 0u, br__rollback_free(stack, ptr));
  }
  if (old_size == 0u) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  if (br__rollback_find_last_alloc(stack, ptr, &block, &header_ptr)) {
    if (block->offset < old_size) {
      return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
    }
    if (new_size <= old_size) {
      if (!br__rollback_block_is_singleton(stack, block)) {
        block->offset -= old_size - new_size;
      }
      return br__rollback_result(ptr, new_size, BR_STATUS_OK);
    }

    if (new_size - old_size <= block->capacity - block->offset) {
      if (!br__rollback_block_is_singleton(stack, block)) {
        new_offset = block->offset + (new_size - old_size);
        block->offset = new_offset;
      }
      if (zeroed) {
        memset((u8 *)ptr + old_size, 0, new_size - old_size);
      }
      return br__rollback_result(ptr, new_size, BR_STATUS_OK);
    }
  }

  if (br__rollback_find_ptr(stack, ptr, NULL, NULL, NULL) != BR_STATUS_OK) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  result = br__rollback_alloc(stack, new_size, alignment, false);
  if (result.status != BR_STATUS_OK) {
    return result;
  }

  /*
  Odin delegates this copy to runtime helpers. Bedrock copies the smaller of
  the two sizes explicitly so C callers do not rely on implicit truncation.
  */
  copy_size = br_min_size(old_size, new_size);
  memcpy(result.ptr, ptr, copy_size);
  if (zeroed && new_size > copy_size) {
    memset((u8 *)result.ptr + copy_size, 0, new_size - copy_size);
  }

  result.status = br__rollback_free(stack, ptr);
  if (result.status != BR_STATUS_OK) {
    return br__rollback_result(NULL, 0u, result.status);
  }
  result.status = BR_STATUS_OK;
  return result;
}

static br_alloc_result br__rollback_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_rollback_stack *stack = (br_rollback_stack *)ctx;

  if (req == NULL) {
    return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__rollback_alloc(stack, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__rollback_alloc(stack, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
      return br__rollback_resize(stack, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__rollback_resize(stack, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__rollback_result(NULL, 0u, br__rollback_free(stack, req->ptr));
    case BR_ALLOC_OP_RESET:
      br_rollback_stack_reset(stack);
      return br__rollback_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__rollback_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

br_status br_rollback_stack_init_buffered(br_rollback_stack *stack, void *buffer, usize capacity) {
  br_rollback_stack_block *block;
  usize min_capacity =
    offsetof(br_rollback_stack_block, buffer) + sizeof(br__rollback_header_bits) + sizeof(void *);

  if (stack == NULL || buffer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (capacity < min_capacity) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (capacity - offsetof(br_rollback_stack_block, buffer) >
      BR_ROLLBACK_STACK_MAX_HEAD_BLOCK_SIZE) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  block = (br_rollback_stack_block *)buffer;
  memset(block, 0, offsetof(br_rollback_stack_block, buffer));
  block->capacity = capacity - offsetof(br_rollback_stack_block, buffer);

  memset(stack, 0, sizeof(*stack));
  stack->head = block;
  stack->block_size = block->capacity;
  return BR_STATUS_OK;
}

br_status br_rollback_stack_init_dynamic(br_rollback_stack *stack,
                                         usize block_size,
                                         br_allocator block_allocator) {
  br_rollback_stack_block *block;
  br_status status;
  usize min_capacity = sizeof(br__rollback_header_bits) + sizeof(void *);

  if (stack == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (block_size == 0u) {
    block_size = BR_ROLLBACK_STACK_DEFAULT_BLOCK_SIZE;
  }
  if (block_size < min_capacity || block_size > BR_ROLLBACK_STACK_MAX_HEAD_BLOCK_SIZE) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  block_allocator = br__rollback_block_allocator(block_allocator);
  status = br__rollback_make_block(block_size, block_allocator, &block);
  if (status != BR_STATUS_OK) {
    return status;
  }

  memset(stack, 0, sizeof(*stack));
  stack->head = block;
  stack->block_size = block_size;
  stack->block_allocator = block_allocator;
  stack->head_owned = true;
  return BR_STATUS_OK;
}

void br_rollback_stack_destroy(br_rollback_stack *stack) {
  br_allocator allocator;

  if (stack == NULL || stack->head == NULL) {
    return;
  }

  allocator = br__rollback_block_allocator(stack->block_allocator);
  br__rollback_free_all_extra_blocks(stack);
  if (stack->head_owned) {
    br__rollback_free_block(stack->head, allocator);
  }

  memset(stack, 0, sizeof(*stack));
}

void br_rollback_stack_reset(br_rollback_stack *stack) {
  if (stack == NULL || stack->head == NULL) {
    return;
  }

  br__rollback_free_all_extra_blocks(stack);
}

br_alloc_result br_rollback_stack_alloc(br_rollback_stack *stack, usize size, usize alignment) {
  return br__rollback_alloc(stack, size, alignment, true);
}

br_alloc_result
br_rollback_stack_alloc_uninit(br_rollback_stack *stack, usize size, usize alignment) {
  return br__rollback_alloc(stack, size, alignment, false);
}

br_alloc_result br_rollback_stack_resize(
  br_rollback_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__rollback_resize(stack, ptr, old_size, new_size, alignment, true);
}

br_alloc_result br_rollback_stack_resize_uninit(
  br_rollback_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__rollback_resize(stack, ptr, old_size, new_size, alignment, false);
}

br_status br_rollback_stack_free(br_rollback_stack *stack, void *ptr) {
  return br__rollback_free(stack, ptr);
}

br_allocator br_rollback_stack_allocator(br_rollback_stack *stack) {
  br_allocator allocator;

  allocator.fn = br__rollback_allocator_fn;
  allocator.ctx = stack;
  return allocator;
}
