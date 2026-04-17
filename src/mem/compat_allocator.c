#include <bedrock/mem/compat_allocator.h>

typedef struct br__compat_header {
  usize size;
  usize alignment;
} br__compat_header;

BR_STATIC_ASSERT((sizeof(br__compat_header) & (sizeof(br__compat_header) - 1u)) == 0u,
                 "Compat header size must stay a power of two.");

static br_alloc_result br__compat_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static br_status br__compat_validate_alignment(usize *alignment) {
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

static usize br__compat_prefix_size(usize alignment) {
  return br_max_size(alignment, sizeof(br__compat_header));
}

static br__compat_header *br__compat_header_from_user_ptr(void *ptr) {
  return &((br__compat_header *)ptr)[-1];
}

static br_allocator br__compat_parent(const br_compat_allocator *compat) {
  if (compat != NULL && compat->parent.fn != NULL) {
    return compat->parent;
  }
  return br_allocator_heap();
}

static br_alloc_result
br__compat_alloc_internal(br_compat_allocator *compat, usize size, usize alignment, bool zeroed) {
  br_allocator parent;
  br_alloc_result allocation;
  br__compat_header *header;
  usize prefix;
  usize req_size;
  void *data;
  br_status status;

  if (compat == NULL) {
    return br__compat_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  status = br__compat_validate_alignment(&alignment);
  if (status != BR_STATUS_OK) {
    return br__compat_result(NULL, 0u, status);
  }
  if (size == 0u) {
    return br__compat_result(NULL, 0u, BR_STATUS_OK);
  }

  prefix = br__compat_prefix_size(alignment);
  if (size > SIZE_MAX - prefix) {
    return br__compat_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }
  req_size = size + prefix;

  parent = br__compat_parent(compat);
  allocation = zeroed ? br_allocator_alloc(parent, req_size, alignment)
                      : br_allocator_alloc_uninit(parent, req_size, alignment);
  if (allocation.status != BR_STATUS_OK) {
    return allocation;
  }
  if (allocation.ptr == NULL) {
    return br__compat_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  data = (u8 *)allocation.ptr + prefix;
  header = br__compat_header_from_user_ptr(data);
  header->size = size;
  header->alignment = alignment;
  return br__compat_result(data, size, BR_STATUS_OK);
}

static br_status br__compat_free_internal(br_compat_allocator *compat, void *ptr) {
  br__compat_header *header;
  br_allocator parent;
  usize prefix;
  usize orig_size;

  if (compat == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (ptr == NULL) {
    return BR_STATUS_OK;
  }

  header = br__compat_header_from_user_ptr(ptr);
  prefix = br__compat_prefix_size(header->alignment);
  if (header->size > SIZE_MAX - prefix) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  orig_size = header->size + prefix;
  parent = br__compat_parent(compat);
  return br_allocator_free(parent, (u8 *)ptr - prefix, orig_size);
}

static br_alloc_result br__compat_resize_internal(br_compat_allocator *compat,
                                                  void *ptr,
                                                  usize old_size,
                                                  usize new_size,
                                                  usize alignment,
                                                  bool zeroed) {
  br__compat_header *header;
  br_allocator parent;
  br_alloc_result allocation;
  usize orig_alignment;
  usize orig_prefix;
  usize orig_size;
  usize new_alignment;
  usize new_prefix;
  usize req_size;
  usize bytes_to_move;
  void *data;
  br_status status;

  BR_UNUSED(old_size);

  if (compat == NULL) {
    return br__compat_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  status = br__compat_validate_alignment(&alignment);
  if (status != BR_STATUS_OK) {
    return br__compat_result(NULL, 0u, status);
  }
  if (ptr == NULL) {
    return br__compat_alloc_internal(compat, new_size, alignment, zeroed);
  }
  if (new_size == 0u) {
    return br__compat_result(NULL, 0u, br__compat_free_internal(compat, ptr));
  }

  header = br__compat_header_from_user_ptr(ptr);
  orig_alignment = header->alignment;
  orig_prefix = br__compat_prefix_size(orig_alignment);
  if (header->size > SIZE_MAX - orig_prefix) {
    return br__compat_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  orig_size = header->size + orig_prefix;
  new_alignment = br_max_size(orig_alignment, alignment);
  new_prefix = br__compat_prefix_size(new_alignment);
  if (new_size > SIZE_MAX - new_prefix) {
    return br__compat_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }
  req_size = new_size + new_prefix;

  if (header->size == new_size && orig_alignment == new_alignment) {
    return br__compat_result(ptr, new_size, BR_STATUS_OK);
  }

  parent = br__compat_parent(compat);
  allocation =
    zeroed
      ? br_allocator_resize(parent, (u8 *)ptr - orig_prefix, orig_size, req_size, new_alignment)
      : br_allocator_resize_uninit(
          parent, (u8 *)ptr - orig_prefix, orig_size, req_size, new_alignment);
  if (allocation.status != BR_STATUS_OK) {
    return allocation;
  }
  if (allocation.ptr == NULL) {
    return br__compat_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  /*
  Odin forwards the wrapped allocation directly to the parent resize. Bedrock
  additionally shifts the user payload forward when the compat header prefix
  grows so data remains stable even if the parent resize keeps the same base
  pointer in place.
  */
  bytes_to_move = br_min_size(header->size, new_size);
  if (new_prefix != orig_prefix && bytes_to_move != 0u) {
    memmove((u8 *)allocation.ptr + new_prefix, (u8 *)allocation.ptr + orig_prefix, bytes_to_move);
  }

  data = (u8 *)allocation.ptr + new_prefix;
  header = br__compat_header_from_user_ptr(data);
  header->size = new_size;
  header->alignment = new_alignment;

  if (zeroed && new_size > bytes_to_move) {
    memset((u8 *)data + bytes_to_move, 0, new_size - bytes_to_move);
  }

  return br__compat_result(data, new_size, BR_STATUS_OK);
}

static br_alloc_result br__compat_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_compat_allocator *compat = (br_compat_allocator *)ctx;

  if (req == NULL) {
    return br__compat_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
      return br__compat_alloc_internal(compat, req->size, req->alignment, true);
    case BR_ALLOC_OP_ALLOC_UNINIT:
      return br__compat_alloc_internal(compat, req->size, req->alignment, false);
    case BR_ALLOC_OP_RESIZE:
      return br__compat_resize_internal(
        compat, req->ptr, req->old_size, req->size, req->alignment, true);
    case BR_ALLOC_OP_RESIZE_UNINIT:
      return br__compat_resize_internal(
        compat, req->ptr, req->old_size, req->size, req->alignment, false);
    case BR_ALLOC_OP_FREE:
      return br__compat_result(NULL, 0u, br__compat_free_internal(compat, req->ptr));
    case BR_ALLOC_OP_RESET:
      return br__compat_result(NULL, 0u, br_allocator_reset(br__compat_parent(compat)));
  }

  return br__compat_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

void br_compat_allocator_init(br_compat_allocator *compat, br_allocator parent) {
  if (compat == NULL) {
    return;
  }
  if (parent.fn == NULL) {
    parent = br_allocator_heap();
  }
  compat->parent = parent;
}

br_allocator br_compat_allocator_allocator(br_compat_allocator *compat) {
  br_allocator allocator = {0};

  allocator.fn = br__compat_allocator_fn;
  allocator.ctx = compat;
  return allocator;
}
