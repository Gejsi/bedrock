#include <bedrock/mem/stack.h>

typedef struct br__stack_header {
  usize prev_offset;
  usize padding;
} br__stack_header;

static br_alloc_result br__stack_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static usize br__stack_normalize_alignment(usize alignment) {
  if (alignment == 0u) {
    alignment = BR_DEFAULT_ALIGNMENT;
  }

  return alignment;
}

static uptr br__stack_align_up(uptr value, usize alignment) {
  return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static usize br__stack_calc_padding_with_header(uptr start, usize alignment) {
  uptr user_ptr = br__stack_align_up(start + sizeof(br__stack_header), alignment);
  return (usize)(user_ptr - start);
}

static br_status br__stack_validate_alignment(usize *alignment) {
  if (alignment == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  *alignment = br__stack_normalize_alignment(*alignment);
  if (!br_is_power_of_two_size(*alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  return BR_STATUS_OK;
}

static br_alloc_result
br__stack_alloc_internal(br_stack *stack, usize size, usize alignment, bool zeroed) {
  uptr curr_addr;
  uptr next_addr;
  usize old_prev_offset;
  usize padding;
  br__stack_header *header;

  if (stack == NULL) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (stack->data == NULL) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (br__stack_validate_alignment(&alignment) != BR_STATUS_OK) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (size == 0u) {
    return br__stack_result(NULL, 0u, BR_STATUS_OK);
  }

  curr_addr = (uptr)stack->data + stack->curr_offset;
  padding = br__stack_calc_padding_with_header(curr_addr, alignment);
  if (stack->curr_offset > stack->capacity || padding > stack->capacity - stack->curr_offset ||
      size > stack->capacity - stack->curr_offset - padding) {
    return br__stack_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  old_prev_offset = stack->prev_offset;
  stack->prev_offset = stack->curr_offset;
  stack->curr_offset += padding;
  next_addr = curr_addr + padding;
  header = (br__stack_header *)(void *)(next_addr - sizeof(br__stack_header));
  header->padding = padding;
  header->prev_offset = old_prev_offset;
  stack->curr_offset += size;
  stack->peak_used = br_max_size(stack->peak_used, stack->curr_offset);

  if (zeroed) {
    memset((void *)next_addr, 0, size);
  }

  return br__stack_result((void *)next_addr, size, BR_STATUS_OK);
}

static br_alloc_result br__stack_fallback_resize(
  br_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment, bool zeroed) {
  br_alloc_result result;

  result = br__stack_alloc_internal(stack, new_size, alignment, false);
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

static br_alloc_result br__stack_resize_internal(
  br_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment, bool zeroed) {
  uptr start;
  uptr end;
  uptr curr_addr;
  usize old_offset;
  usize user_offset;
  usize old_memory_size;
  br__stack_header *header;

  if (stack == NULL) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (stack->data == NULL) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (br__stack_validate_alignment(&alignment) != BR_STATUS_OK) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (ptr == NULL) {
    return br__stack_alloc_internal(stack, new_size, alignment, zeroed);
  }
  if (new_size == 0u) {
    return br__stack_result(NULL, 0u, br_stack_free(stack, ptr));
  }
  if (old_size == 0u) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  start = (uptr)stack->data;
  end = start + stack->capacity;
  curr_addr = (uptr)ptr;
  if (!(start <= curr_addr && curr_addr < end)) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  /*
  Keep Odin's current "already above curr_offset" behavior here: resizing an
  already-freed pointer is treated as a tolerated no-op rather than a hard
  error.
  */
  if (curr_addr >= start + stack->curr_offset) {
    return br__stack_result(NULL, 0u, BR_STATUS_OK);
  }

  if ((curr_addr & (uptr)(alignment - 1u)) != 0u) {
    return br__stack_fallback_resize(stack, ptr, old_size, new_size, alignment, zeroed);
  }
  if (old_size == new_size) {
    return br__stack_result(ptr, new_size, BR_STATUS_OK);
  }

  header = (br__stack_header *)(void *)(curr_addr - sizeof(br__stack_header));
  old_offset = (usize)(curr_addr - (uptr)header->padding - start);

  /*
  Bedrock checks against the stack's current `prev_offset` so resize uses the
  same last-allocation rule as `br_stack_free`. Odin's current source compares
  against `header.prev_offset`, which appears inconsistent with its free path
  and would only permit in-place resize for the first allocation in the stack.
  */
  if (old_offset != stack->prev_offset) {
    return br__stack_fallback_resize(stack, ptr, old_size, new_size, alignment, zeroed);
  }

  user_offset = (usize)(curr_addr - start);
  old_memory_size = stack->curr_offset - user_offset;

  /*
  Odin asserts here. Bedrock returns a status instead so a bad `old_size`
  cannot silently corrupt the stack's running offset in C callers.
  */
  if (old_memory_size != old_size) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (user_offset > stack->capacity || new_size > stack->capacity - user_offset) {
    return br__stack_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  stack->curr_offset = user_offset + new_size;
  stack->peak_used = br_max_size(stack->peak_used, stack->curr_offset);
  if (zeroed && new_size > old_size) {
    memset((u8 *)ptr + old_size, 0, new_size - old_size);
  }

  return br__stack_result(ptr, new_size, BR_STATUS_OK);
}

static br_alloc_result br__stack_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_stack *stack = (br_stack *)ctx;

  if (req == NULL) {
    return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__stack_alloc_internal(stack, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__stack_alloc_internal(stack, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
      return br__stack_resize_internal(
        stack, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__stack_resize_internal(
        stack, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__stack_result(NULL, 0u, br_stack_free(stack, req->ptr));
    case BR_ALLOC_OP_RESET:
      br_stack_free_all(stack);
      return br__stack_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

void br_stack_init(br_stack *stack, void *buffer, usize capacity) {
  if (stack == NULL) {
    return;
  }

  stack->data = (u8 *)buffer;
  stack->capacity = capacity;
  stack->prev_offset = 0u;
  stack->curr_offset = 0u;
  stack->peak_used = 0u;
}

void br_stack_free_all(br_stack *stack) {
  if (stack == NULL) {
    return;
  }

  stack->prev_offset = 0u;
  stack->curr_offset = 0u;
}

br_alloc_result br_stack_alloc(br_stack *stack, usize size, usize alignment) {
  return br__stack_alloc_internal(stack, size, alignment, true);
}

br_alloc_result br_stack_alloc_uninit(br_stack *stack, usize size, usize alignment) {
  return br__stack_alloc_internal(stack, size, alignment, false);
}

br_status br_stack_free(br_stack *stack, void *ptr) {
  uptr start;
  uptr end;
  uptr curr_addr;
  br__stack_header *header;
  usize old_offset;

  if (stack == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (stack->data == NULL) {
    return BR_STATUS_INVALID_STATE;
  }
  if (ptr == NULL) {
    return BR_STATUS_OK;
  }

  start = (uptr)stack->data;
  end = start + stack->capacity;
  curr_addr = (uptr)ptr;
  if (!(start <= curr_addr && curr_addr < end)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (curr_addr >= start + stack->curr_offset) {
    return BR_STATUS_OK;
  }

  header = (br__stack_header *)(void *)(curr_addr - sizeof(br__stack_header));
  old_offset = (usize)(curr_addr - (uptr)header->padding - start);
  if (old_offset != stack->prev_offset) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  stack->prev_offset = header->prev_offset;
  stack->curr_offset = old_offset;
  return BR_STATUS_OK;
}

br_alloc_result
br_stack_resize(br_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__stack_resize_internal(stack, ptr, old_size, new_size, alignment, true);
}

br_alloc_result br_stack_resize_uninit(
  br_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__stack_resize_internal(stack, ptr, old_size, new_size, alignment, false);
}

br_allocator br_stack_allocator(br_stack *stack) {
  br_allocator allocator;

  allocator.fn = br__stack_allocator_fn;
  allocator.ctx = stack;
  return allocator;
}
