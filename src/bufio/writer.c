#include <bedrock/bufio/writer.h>

static void br__bufio_writer_clear_state(br_bufio_writer *writer, br_stream sink) {
  writer->sink = sink;
  writer->n = 0u;
  writer->err = BR_STATUS_OK;
}

br_status br_bufio_writer_init(br_bufio_writer *writer, br_stream sink, br_allocator allocator) {
  return br_bufio_writer_init_with_size(writer, sink, BR_BUFIO_DEFAULT_BUFFER_SIZE, allocator);
}

br_status br_bufio_writer_init_with_size(br_bufio_writer *writer,
                                         br_stream sink,
                                         usize size,
                                         br_allocator allocator) {
  br_alloc_result alloc;

  if (writer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (size < BR_BUFIO_MIN_BUFFER_SIZE) {
    size = BR_BUFIO_MIN_BUFFER_SIZE;
  }

  memset(writer, 0, sizeof(*writer));
  alloc = br_allocator_alloc_uninit(allocator, size, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return alloc.status;
  }

  writer->buf = alloc.ptr;
  writer->cap = size;
  writer->allocator = allocator;
  writer->owns_storage = true;
  br__bufio_writer_clear_state(writer, sink);
  return BR_STATUS_OK;
}

br_status br_bufio_writer_init_with_buffer(br_bufio_writer *writer,
                                           br_stream sink,
                                           void *buffer,
                                           usize buffer_len) {
  if (writer == NULL || buffer == NULL || buffer_len == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  memset(writer, 0, sizeof(*writer));
  writer->buf = buffer;
  writer->cap = buffer_len;
  writer->allocator = br_allocator_null();
  writer->owns_storage = false;
  br__bufio_writer_clear_state(writer, sink);
  return BR_STATUS_OK;
}

void br_bufio_writer_destroy(br_bufio_writer *writer) {
  if (writer == NULL) {
    return;
  }

  if (writer->owns_storage && writer->buf != NULL) {
    (void)br_allocator_free(writer->allocator, writer->buf, writer->cap);
  }

  memset(writer, 0, sizeof(*writer));
}

void br_bufio_writer_reset(br_bufio_writer *writer, br_stream sink) {
  if (writer == NULL) {
    return;
  }

  br__bufio_writer_clear_state(writer, sink);
}

usize br_bufio_writer_size(const br_bufio_writer *writer) {
  return writer != NULL ? writer->cap : 0u;
}

usize br_bufio_writer_available(const br_bufio_writer *writer) {
  if (writer == NULL || writer->cap < writer->n) {
    return 0u;
  }

  return writer->cap - writer->n;
}

usize br_bufio_writer_buffered(const br_bufio_writer *writer) {
  return writer != NULL ? writer->n : 0u;
}

br_status br_bufio_writer_flush(br_bufio_writer *writer) {
  br_io_result result;

  if (writer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (writer->err != BR_STATUS_OK) {
    return writer->err;
  }
  if (writer->n == 0u) {
    return BR_STATUS_OK;
  }

  result = br_write(writer->sink, writer->buf, writer->n);
  if (result.count > writer->n) {
    writer->err = BR_STATUS_INVALID_STATE;
    return BR_STATUS_INVALID_STATE;
  }
  if (result.count < writer->n && result.status == BR_STATUS_OK) {
    result.status = BR_STATUS_SHORT_WRITE;
  }
  if (result.status != BR_STATUS_OK) {
    if (result.count > 0u && result.count < writer->n) {
      memmove(writer->buf, writer->buf + result.count, writer->n - result.count);
    }
    writer->n -= result.count;
    writer->err = result.status;
    return result.status;
  }

  writer->n = 0u;
  return BR_STATUS_OK;
}

br_bufio_writer_io_result
br_bufio_writer_write(br_bufio_writer *writer, const void *src, usize src_len) {
  const u8 *cursor;
  usize written;

  if (writer == NULL || (src == NULL && src_len > 0u)) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (writer->err != BR_STATUS_OK) {
    return br_io_result_make(0u, writer->err);
  }

  cursor = src;
  written = 0u;
  while (src_len > br_bufio_writer_available(writer) && writer->err == BR_STATUS_OK) {
    usize moved;

    if (br_bufio_writer_buffered(writer) == 0u) {
      br_io_result result;

      result = br_write(writer->sink, cursor, src_len);
      if (result.count > src_len) {
        writer->err = BR_STATUS_INVALID_STATE;
        break;
      }
      if (result.count < src_len && result.status == BR_STATUS_OK) {
        writer->err = BR_STATUS_SHORT_WRITE;
      } else if (result.status != BR_STATUS_OK) {
        writer->err = result.status;
      }

      written += result.count;
      cursor += result.count;
      src_len -= result.count;
      if (writer->err != BR_STATUS_OK) {
        break;
      }
    } else {
      moved = br_bufio_writer_available(writer);
      memcpy(writer->buf + writer->n, cursor, moved);
      writer->n += moved;
      written += moved;
      cursor += moved;
      src_len -= moved;
      (void)br_bufio_writer_flush(writer);
    }
  }

  if (writer->err != BR_STATUS_OK) {
    return br_io_result_make(written, writer->err);
  }

  if (src_len > 0u) {
    memcpy(writer->buf + writer->n, cursor, src_len);
    writer->n += src_len;
    written += src_len;
  }
  return br_io_result_make(written, BR_STATUS_OK);
}

br_status br_bufio_writer_write_byte(br_bufio_writer *writer, u8 value) {
  if (writer == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (writer->err != BR_STATUS_OK) {
    return writer->err;
  }
  if (br_bufio_writer_available(writer) == 0u && br_bufio_writer_flush(writer) != BR_STATUS_OK) {
    return writer->err;
  }

  writer->buf[writer->n] = value;
  writer->n += 1u;
  return BR_STATUS_OK;
}

br_bufio_writer_io_result br_bufio_writer_write_rune(br_bufio_writer *writer, br_rune value) {
  br_utf8_encode_result encoded;

  if (writer == NULL) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  if (value < BR_RUNE_SELF) {
    br_status status;

    status = br_bufio_writer_write_byte(writer, (u8)value);
    if (status != BR_STATUS_OK) {
      return br_io_result_make(0u, status);
    }
    return br_io_result_make(1u, BR_STATUS_OK);
  }
  if (writer->err != BR_STATUS_OK) {
    return br_io_result_make(0u, writer->err);
  }

  if (br_bufio_writer_available(writer) < BR_UTF8_MAX) {
    br_status status;

    status = br_bufio_writer_flush(writer);
    if (status != BR_STATUS_OK) {
      return br_io_result_make(0u, status);
    }
    if (br_bufio_writer_available(writer) < BR_UTF8_MAX) {
      encoded = br_utf8_encode(value);
      return br_bufio_writer_write(writer, encoded.bytes, encoded.len);
    }
  }

  encoded = br_utf8_encode(value);
  memcpy(writer->buf + writer->n, encoded.bytes, encoded.len);
  writer->n += encoded.len;
  return br_io_result_make(encoded.len, BR_STATUS_OK);
}

br_bufio_writer_io_result br_bufio_writer_write_string(br_bufio_writer *writer, br_string_view s) {
  return br_bufio_writer_write(writer, s.data, s.len);
}

static br_i64_result br__bufio_writer_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  br_bufio_writer *writer;
  br_io_mode_set modes;
  br_bufio_writer_io_result io_result;
  br_status status;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  writer = (br_bufio_writer *)context;
  switch (mode) {
    case BR_IO_MODE_FLUSH:
      status = br_bufio_writer_flush(writer);
      return br_i64_result_make(0, status);
    case BR_IO_MODE_WRITE:
      io_result = br_bufio_writer_write(writer, data, data_len);
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_DESTROY:
      br_bufio_writer_destroy(writer);
      return br_i64_result_make(0, BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_FLUSH) | br_io_mode_bit(BR_IO_MODE_WRITE) |
              br_io_mode_bit(BR_IO_MODE_DESTROY);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_bufio_writer_as_stream(br_bufio_writer *writer) {
  return br_stream_make(writer, br__bufio_writer_stream_proc);
}
