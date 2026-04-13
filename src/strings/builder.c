#include <bedrock/strings/builder.h>

enum { BR__STRING_BUILDER_MIN_CAPACITY = 64 };

static br_string_builder_io_result br__string_builder_io_result(usize count, br_status status) {
  br_string_builder_io_result result;

  result.count = count;
  result.status = status;
  return result;
}

static br_string_builder_byte_result br__string_builder_byte_result(u8 value, br_status status) {
  br_string_builder_byte_result result;

  result.value = value;
  result.status = status;
  return result;
}

static br_string_builder_rune_result
br__string_builder_rune_result(br_rune value, usize width, br_status status) {
  br_string_builder_rune_result result;

  result.value = value;
  result.width = width;
  result.status = status;
  return result;
}

static bool br__string_builder_add_overflow(usize lhs, usize rhs, usize *out) {
  if (lhs > SIZE_MAX - rhs) {
    return true;
  }

  *out = lhs + rhs;
  return false;
}

static br_status br__string_builder_grow(br_string_builder *builder, usize additional) {
  br_alloc_result resized;
  usize target_len;
  usize new_cap;

  if (builder == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (additional == 0u) {
    return BR_STATUS_OK;
  }
  if (br__string_builder_add_overflow(builder->len, additional, &target_len)) {
    return BR_STATUS_OUT_OF_MEMORY;
  }
  if (target_len <= builder->cap) {
    return BR_STATUS_OK;
  }
  if (!builder->owns_storage) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  new_cap = builder->cap;
  if (new_cap < BR__STRING_BUILDER_MIN_CAPACITY) {
    new_cap = BR__STRING_BUILDER_MIN_CAPACITY;
  }
  while (new_cap < target_len) {
    usize doubled = new_cap * 2u;

    if (doubled <= new_cap) {
      new_cap = target_len;
      break;
    }
    new_cap = doubled;
  }

  if (builder->data == NULL) {
    resized = br_allocator_alloc_uninit(builder->allocator, new_cap, 1u);
  } else {
    resized =
      br_allocator_resize_uninit(builder->allocator, builder->data, builder->cap, new_cap, 1u);
  }
  if (resized.status != BR_STATUS_OK) {
    return resized.status;
  }

  builder->data = resized.ptr;
  builder->cap = new_cap;
  return BR_STATUS_OK;
}

void br_string_builder_init(br_string_builder *builder, br_allocator allocator) {
  if (builder == NULL) {
    return;
  }

  builder->data = NULL;
  builder->len = 0u;
  builder->cap = 0u;
  builder->allocator = allocator;
  builder->owns_storage = 1;
}

br_status br_string_builder_init_with_capacity(br_string_builder *builder,
                                               usize initial_capacity,
                                               br_allocator allocator) {
  br_status status;

  if (builder == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  br_string_builder_init(builder, allocator);
  status = br__string_builder_grow(builder, initial_capacity);
  if (status != BR_STATUS_OK) {
    br_string_builder_destroy(builder);
  }
  return status;
}

void br_string_builder_init_with_backing(br_string_builder *builder,
                                         void *backing,
                                         usize backing_len) {
  if (builder == NULL) {
    return;
  }

  builder->data = (char *)backing;
  builder->len = 0u;
  builder->cap = backing_len;
  builder->allocator = br_allocator_null();
  builder->owns_storage = 0;
}

void br_string_builder_destroy(br_string_builder *builder) {
  if (builder == NULL) {
    return;
  }

  if (builder->owns_storage && builder->data != NULL) {
    (void)br_allocator_free(builder->allocator, builder->data, builder->cap);
  }

  builder->data = NULL;
  builder->len = 0u;
  builder->cap = 0u;
  builder->owns_storage = 0;
}

void br_string_builder_reset(br_string_builder *builder) {
  if (builder == NULL) {
    return;
  }

  builder->len = 0u;
}

bool br_string_builder_is_empty(const br_string_builder *builder) {
  return builder == NULL || builder->len == 0u;
}

usize br_string_builder_len(const br_string_builder *builder) {
  return builder != NULL ? builder->len : 0u;
}

usize br_string_builder_capacity(const br_string_builder *builder) {
  return builder != NULL ? builder->cap : 0u;
}

usize br_string_builder_space(const br_string_builder *builder) {
  if (builder == NULL || builder->cap < builder->len) {
    return 0u;
  }

  return builder->cap - builder->len;
}

br_string_view br_string_builder_view(const br_string_builder *builder) {
  if (builder == NULL || builder->len == 0u) {
    return br_string_view_make(NULL, 0u);
  }

  return br_string_view_make(builder->data, builder->len);
}

br_status br_string_builder_reserve(br_string_builder *builder, usize additional) {
  return br__string_builder_grow(builder, additional);
}

br_status br_string_builder_truncate(br_string_builder *builder, usize n) {
  if (builder == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (n > builder->len) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  builder->len = n;
  return BR_STATUS_OK;
}

br_string_result br_string_builder_clone(const br_string_builder *builder, br_allocator allocator) {
  if (builder == NULL) {
    return br_string_clone(br_string_view_make(NULL, 0u), allocator);
  }

  return br_string_clone(br_string_builder_view(builder), allocator);
}

br_string_builder_io_result br_string_builder_write(br_string_builder *builder,
                                                    br_string_view src) {
  br_status status;

  if (builder == NULL) {
    return br__string_builder_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (src.len == 0u) {
    return br__string_builder_io_result(0u, BR_STATUS_OK);
  }

  status = br__string_builder_grow(builder, src.len);
  if (status != BR_STATUS_OK) {
    return br__string_builder_io_result(0u, status);
  }

  memcpy(builder->data + builder->len, src.data, src.len);
  builder->len += src.len;
  return br__string_builder_io_result(src.len, BR_STATUS_OK);
}

br_status br_string_builder_write_byte(br_string_builder *builder, u8 byte_value) {
  br_status status;

  if (builder == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  status = br__string_builder_grow(builder, 1u);
  if (status != BR_STATUS_OK) {
    return status;
  }

  builder->data[builder->len] = (char)byte_value;
  builder->len += 1u;
  return BR_STATUS_OK;
}

br_string_builder_io_result br_string_builder_write_rune(br_string_builder *builder,
                                                         br_rune value) {
  br_utf8_encode_result encoded;
  br_status status;

  if (builder == NULL) {
    return br__string_builder_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  encoded = br_utf8_encode(value);
  status = br__string_builder_grow(builder, encoded.len);
  if (status != BR_STATUS_OK) {
    return br__string_builder_io_result(0u, status);
  }

  memcpy(builder->data + builder->len, encoded.bytes, encoded.len);
  builder->len += encoded.len;
  return br__string_builder_io_result(encoded.len, BR_STATUS_OK);
}

br_string_builder_byte_result br_string_builder_pop_byte(br_string_builder *builder) {
  if (builder == NULL) {
    return br__string_builder_byte_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (builder->len == 0u) {
    return br__string_builder_byte_result(0u, BR_STATUS_INVALID_STATE);
  }

  builder->len -= 1u;
  return br__string_builder_byte_result((u8)builder->data[builder->len], BR_STATUS_OK);
}

br_string_builder_rune_result br_string_builder_pop_rune(br_string_builder *builder) {
  br_utf8_decode_result decoded;

  if (builder == NULL) {
    return br__string_builder_rune_result(0, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (builder->len == 0u) {
    return br__string_builder_rune_result(0, 0u, BR_STATUS_INVALID_STATE);
  }

  decoded = br_utf8_decode_last(br_bytes_view_make(builder->data, builder->len));
  builder->len -= decoded.width;
  return br__string_builder_rune_result(decoded.value, decoded.width, BR_STATUS_OK);
}

static br_i64_result br__string_builder_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  br_string_builder *builder;
  br_string_builder_io_result io_result;
  br_io_mode_set modes;

  (void)offset;
  (void)whence;

  builder = (br_string_builder *)context;
  switch (mode) {
    case BR_IO_MODE_WRITE:
      io_result = br_string_builder_write(builder, br_string_view_make(data, data_len));
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_SIZE:
      return br_i64_result_make((i64)br_string_builder_len(builder), BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_WRITE) | br_io_mode_bit(BR_IO_MODE_SIZE);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_string_builder_as_stream(br_string_builder *builder) {
  return br_stream_make(builder, br__string_builder_stream_proc);
}
