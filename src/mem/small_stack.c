#include <bedrock/mem/small_stack.h>

typedef struct br__small_stack_header {
  u8 padding;
} br__small_stack_header;

static br_alloc_result br__small_stack_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static usize br__small_stack_max_alignment(void) {
  return sizeof(usize) * 4u;
}

static usize br__small_stack_normalize_alignment(usize alignment) {
  if (alignment == 0u) {
    alignment = BR_DEFAULT_ALIGNMENT;
  }
  if (alignment < 1u) {
    alignment = 1u;
  }
  if (alignment > br__small_stack_max_alignment()) {
    alignment = br__small_stack_max_alignment();
  }

  return alignment;
}

static uptr br__small_stack_align_up(uptr value, usize alignment) {
  return (value + (uptr)(alignment - 1u)) & ~((uptr)(alignment - 1u));
}

static usize br__small_stack_calc_padding_with_header(uptr start, usize alignment) {
  uptr user_ptr = br__small_stack_align_up(start + sizeof(br__small_stack_header), alignment);
  return (usize)(user_ptr - start);
}

static br_status br__small_stack_validate_alignment(usize *alignment) {
  if (alignment == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  *alignment = br__small_stack_normalize_alignment(*alignment);
  if (!br_is_power_of_two_size(*alignment)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  return BR_STATUS_OK;
}

static br_alloc_result
br__small_stack_alloc_internal(br_small_stack *stack, usize size, usize alignment, bool zeroed) {
  uptr curr_addr;
  uptr next_addr;
  usize padding;
  br__small_stack_header *header;

  if (stack == NULL) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (stack->data == NULL) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (br__small_stack_validate_alignment(&alignment) != BR_STATUS_OK) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (size == 0u) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_OK);
  }

  curr_addr = (uptr)stack->data + stack->offset;
  padding = br__small_stack_calc_padding_with_header(curr_addr, alignment);
  if (padding > UINT8_MAX || stack->offset > stack->capacity ||
      padding > stack->capacity - stack->offset ||
      size > stack->capacity - stack->offset - padding) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  stack->offset += padding;
  next_addr = curr_addr + padding;
  header = (br__small_stack_header *)(void *)(next_addr - sizeof(br__small_stack_header));
  header->padding = (u8)padding;
  stack->offset += size;
  stack->peak_used = br_max_size(stack->peak_used, stack->offset);

  if (zeroed) {
    memset((void *)next_addr, 0, size);
  }

  return br__small_stack_result((void *)next_addr, size, BR_STATUS_OK);
}

static br_alloc_result br__small_stack_resize_fallback(
  br_small_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment, bool zeroed) {
  br_alloc_result result;

  result = br__small_stack_alloc_internal(stack, new_size, alignment, false);
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

static br_alloc_result br__small_stack_resize_internal(
  br_small_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment, bool zeroed) {
  uptr start;
  uptr end;
  uptr curr_addr;

  if (stack == NULL) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (stack->data == NULL) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_STATE);
  }
  if (br__small_stack_validate_alignment(&alignment) != BR_STATUS_OK) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (ptr == NULL) {
    return br__small_stack_alloc_internal(stack, new_size, alignment, zeroed);
  }
  if (new_size == 0u) {
    return br__small_stack_result(NULL, 0u, br_small_stack_free(stack, ptr));
  }

  start = (uptr)stack->data;
  end = start + stack->capacity;
  curr_addr = (uptr)ptr;
  if (!(start <= curr_addr && curr_addr < end)) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  /*
  Keep Odin's current small-stack behavior here: a pointer already above the
  current offset is treated like a tolerated double free in resize paths too.
  */
  if (curr_addr >= start + stack->offset) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_OK);
  }

  if ((curr_addr & (uptr)(alignment - 1u)) != 0u) {
    return br__small_stack_resize_fallback(stack, ptr, old_size, new_size, alignment, zeroed);
  }
  if (old_size == new_size) {
    return br__small_stack_result(ptr, new_size, BR_STATUS_OK);
  }

  return br__small_stack_resize_fallback(stack, ptr, old_size, new_size, alignment, zeroed);
}

static br_alloc_result br__small_stack_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_small_stack *stack = (br_small_stack *)ctx;

  if (req == NULL) {
    return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__small_stack_alloc_internal(stack, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__small_stack_alloc_internal(stack, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
      return br__small_stack_resize_internal(
        stack, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__small_stack_resize_internal(
        stack, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__small_stack_result(NULL, 0u, br_small_stack_free(stack, req->ptr));
    case BR_ALLOC_OP_RESET:
      br_small_stack_free_all(stack);
      return br__small_stack_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__small_stack_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

void br_small_stack_init(br_small_stack *stack, void *buffer, usize capacity) {
  if (stack == NULL) {
    return;
  }

  stack->data = (u8 *)buffer;
  stack->capacity = capacity;
  stack->offset = 0u;
  stack->peak_used = 0u;
}

void br_small_stack_free_all(br_small_stack *stack) {
  if (stack == NULL) {
    return;
  }

  stack->offset = 0u;
}

br_alloc_result br_small_stack_alloc(br_small_stack *stack, usize size, usize alignment) {
  return br__small_stack_alloc_internal(stack, size, alignment, true);
}

br_alloc_result br_small_stack_alloc_uninit(br_small_stack *stack, usize size, usize alignment) {
  return br__small_stack_alloc_internal(stack, size, alignment, false);
}

br_status br_small_stack_free(br_small_stack *stack, void *ptr) {
  uptr start;
  uptr end;
  uptr curr_addr;
  br__small_stack_header *header;

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
  if (curr_addr >= start + stack->offset) {
    return BR_STATUS_OK;
  }

  header = (br__small_stack_header *)(void *)(curr_addr - sizeof(br__small_stack_header));
  stack->offset = (usize)(curr_addr - (uptr)header->padding - start);
  return BR_STATUS_OK;
}

br_alloc_result br_small_stack_resize(
  br_small_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__small_stack_resize_internal(stack, ptr, old_size, new_size, alignment, true);
}

br_alloc_result br_small_stack_resize_uninit(
  br_small_stack *stack, void *ptr, usize old_size, usize new_size, usize alignment) {
  return br__small_stack_resize_internal(stack, ptr, old_size, new_size, alignment, false);
}

br_allocator br_small_stack_allocator(br_small_stack *stack) {
  br_allocator allocator;

  allocator.fn = br__small_stack_allocator_fn;
  allocator.ctx = stack;
  return allocator;
}
