#include <bedrock/bufio/reader.h>

static br_bufio_reader_peek_result br__bufio_reader_peek_result(br_bytes_view value,
                                                                br_status status) {
  br_bufio_reader_peek_result result;

  result.value = value;
  result.status = status;
  return result;
}

static void br__bufio_reader_clear_state(br_bufio_reader *reader, br_stream source) {
  reader->source = source;
  reader->r = 0u;
  reader->w = 0u;
  reader->err = BR_STATUS_OK;
  reader->last_byte = -1;
  reader->last_rune_size = -1;
}

static void br__bufio_reader_transfer_to_front(br_bufio_reader *reader) {
  usize unread;

  if (reader->r == 0u) {
    return;
  }

  unread = reader->w - reader->r;
  if (unread > 0u) {
    memmove(reader->buf, reader->buf + reader->r, unread);
  }
  reader->r = 0u;
  reader->w = unread;
}

static br_status br__bufio_reader_fill(br_bufio_reader *reader) {
  usize attempts;

  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (reader->r > 0u) {
    br__bufio_reader_transfer_to_front(reader);
  }
  if (reader->w >= reader->cap) {
    reader->err = BR_STATUS_BUFFER_FULL;
    return BR_STATUS_OK;
  }

  attempts = reader->max_consecutive_empty_reads;
  if (attempts == 0u) {
    attempts = BR_BUFIO_DEFAULT_MAX_CONSECUTIVE_EMPTY_READS;
  }

  while (attempts > 0u) {
    br_io_result read_result;

    read_result = br_read(reader->source, reader->buf + reader->w, reader->cap - reader->w);
    if (read_result.count > 0u) {
      reader->w += read_result.count;
      if (read_result.status != BR_STATUS_OK) {
        reader->err = read_result.status;
      }
      return BR_STATUS_OK;
    }
    if (read_result.status != BR_STATUS_OK) {
      reader->err = read_result.status;
      return BR_STATUS_OK;
    }

    attempts -= 1u;
  }

  reader->err = BR_STATUS_NO_PROGRESS;
  return BR_STATUS_OK;
}

static br_status br__bufio_reader_consume_err(br_bufio_reader *reader) {
  br_status err;

  err = reader->err;
  reader->err = BR_STATUS_OK;
  return err;
}

static br_bytes br__bufio_take_byte_buffer(br_byte_buffer *buffer) {
  br_bytes bytes;

  bytes = br_bytes_make(buffer->data, buffer->len);
  buffer->data = NULL;
  buffer->len = 0u;
  buffer->cap = 0u;
  buffer->off = 0u;
  buffer->allocator = br_allocator_null();
  buffer->can_unread_byte = false;
  return bytes;
}

static br_i64_result br__bufio_reader_write_buffer(br_bufio_reader *reader, br_stream sink) {
  usize available;
  br_io_result result;

  available = br_bufio_reader_buffered(reader);
  result = br_write(sink, reader->buf + reader->r, available);
  reader->r += result.count;
  /*
  Odin's io.write already reports short writes as errors. Bedrock's generic
  br_write is looser, so bufio normalizes a short successful chunk here.
  */
  if (result.count < available && result.status == BR_STATUS_OK) {
    result.status = BR_STATUS_SHORT_WRITE;
  }
  return br_i64_result_make((i64)result.count, result.status);
}

br_status br_bufio_reader_init(br_bufio_reader *reader, br_stream source, br_allocator allocator) {
  return br_bufio_reader_init_with_size(reader, source, BR_BUFIO_DEFAULT_BUFFER_SIZE, allocator);
}

br_status br_bufio_reader_init_with_size(br_bufio_reader *reader,
                                         br_stream source,
                                         usize size,
                                         br_allocator allocator) {
  br_alloc_result alloc;

  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (size < BR_BUFIO_MIN_BUFFER_SIZE) {
    size = BR_BUFIO_MIN_BUFFER_SIZE;
  }

  memset(reader, 0, sizeof(*reader));
  alloc = br_allocator_alloc_uninit(allocator, size, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return alloc.status;
  }

  reader->buf = alloc.ptr;
  reader->cap = size;
  reader->allocator = allocator;
  reader->owns_storage = true;
  reader->max_consecutive_empty_reads = BR_BUFIO_DEFAULT_MAX_CONSECUTIVE_EMPTY_READS;
  br__bufio_reader_clear_state(reader, source);
  return BR_STATUS_OK;
}

br_status br_bufio_reader_init_with_buffer(br_bufio_reader *reader,
                                           br_stream source,
                                           void *buffer,
                                           usize buffer_len) {
  if (reader == NULL || buffer == NULL || buffer_len == 0u) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  memset(reader, 0, sizeof(*reader));
  reader->buf = buffer;
  reader->cap = buffer_len;
  reader->allocator = br_allocator_null();
  reader->owns_storage = false;
  reader->max_consecutive_empty_reads = BR_BUFIO_DEFAULT_MAX_CONSECUTIVE_EMPTY_READS;
  br__bufio_reader_clear_state(reader, source);
  return BR_STATUS_OK;
}

void br_bufio_reader_destroy(br_bufio_reader *reader) {
  if (reader == NULL) {
    return;
  }

  if (reader->owns_storage && reader->buf != NULL) {
    (void)br_allocator_free(reader->allocator, reader->buf, reader->cap);
  }

  memset(reader, 0, sizeof(*reader));
}

void br_bufio_reader_reset(br_bufio_reader *reader, br_stream source) {
  if (reader == NULL) {
    return;
  }

  br__bufio_reader_clear_state(reader, source);
}

usize br_bufio_reader_size(const br_bufio_reader *reader) {
  return reader != NULL ? reader->cap : 0u;
}

usize br_bufio_reader_buffered(const br_bufio_reader *reader) {
  if (reader == NULL || reader->w < reader->r) {
    return 0u;
  }

  return reader->w - reader->r;
}

br_bufio_reader_peek_result br_bufio_reader_peek(br_bufio_reader *reader, usize n) {
  usize available;
  br_status status;

  if (reader == NULL) {
    return br__bufio_reader_peek_result(br_bytes_view_make(NULL, 0u), BR_STATUS_INVALID_ARGUMENT);
  }

  reader->last_byte = -1;
  reader->last_rune_size = -1;

  while (br_bufio_reader_buffered(reader) < n && br_bufio_reader_buffered(reader) < reader->cap &&
         reader->err == BR_STATUS_OK) {
    br_status fill_status;

    fill_status = br__bufio_reader_fill(reader);
    if (fill_status != BR_STATUS_OK) {
      return br__bufio_reader_peek_result(br_bytes_view_make(NULL, 0u), fill_status);
    }
  }

  available = br_bufio_reader_buffered(reader);
  if (n > reader->cap) {
    return br__bufio_reader_peek_result(br_bytes_view_make(reader->buf + reader->r, available),
                                        BR_STATUS_BUFFER_FULL);
  }

  status = BR_STATUS_OK;
  if (available < n) {
    n = available;
    status = br__bufio_reader_consume_err(reader);
    if (status == BR_STATUS_OK) {
      status = BR_STATUS_BUFFER_FULL;
    }
  }

  return br__bufio_reader_peek_result(br_bytes_view_make(reader->buf + reader->r, n), status);
}

br_bufio_reader_io_result br_bufio_reader_discard(br_bufio_reader *reader, usize n) {
  usize remaining;
  usize discarded;

  if (reader == NULL) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (n == 0u) {
    return br_io_result_make(0u, BR_STATUS_OK);
  }

  reader->last_byte = -1;
  reader->last_rune_size = -1;
  remaining = n;
  discarded = 0u;
  for (;;) {
    usize skip;

    skip = br_bufio_reader_buffered(reader);
    if (skip == 0u) {
      if (reader->err != BR_STATUS_OK) {
        return br_io_result_make(discarded, br__bufio_reader_consume_err(reader));
      }
      if (br__bufio_reader_fill(reader) != BR_STATUS_OK) {
        return br_io_result_make(discarded, BR_STATUS_INVALID_STATE);
      }
      skip = br_bufio_reader_buffered(reader);
      if (skip == 0u && reader->err != BR_STATUS_OK) {
        return br_io_result_make(discarded, br__bufio_reader_consume_err(reader));
      }
    }

    skip = br_min_size(skip, remaining);
    reader->r += skip;
    discarded += skip;
    remaining -= skip;
    if (remaining == 0u) {
      return br_io_result_make(discarded, BR_STATUS_OK);
    }
  }
}

br_bufio_reader_io_result br_bufio_reader_read(br_bufio_reader *reader, void *dst, usize dst_len) {
  br_io_result read_result;
  usize copied;

  if (reader == NULL || (dst == NULL && dst_len > 0u)) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_len == 0u) {
    if (br_bufio_reader_buffered(reader) > 0u) {
      return br_io_result_make(0u, BR_STATUS_OK);
    }
    return br_io_result_make(0u, br__bufio_reader_consume_err(reader));
  }

  if (reader->r == reader->w) {
    if (reader->err != BR_STATUS_OK) {
      return br_io_result_make(0u, br__bufio_reader_consume_err(reader));
    }

    if (dst_len >= reader->cap) {
      read_result = br_read(reader->source, dst, dst_len);
      if (read_result.count > 0u) {
        reader->last_byte = ((const u8 *)dst)[read_result.count - 1u];
        reader->last_rune_size = -1;
      }
      return read_result;
    }

    reader->r = 0u;
    reader->w = 0u;
    read_result = br_read(reader->source, reader->buf, reader->cap);
    if (read_result.count == 0u) {
      return br_io_result_make(0u, read_result.status);
    }
    reader->w = read_result.count;
    if (read_result.status != BR_STATUS_OK) {
      reader->err = read_result.status;
    }
  }

  copied = br_min_size(dst_len, reader->w - reader->r);
  memcpy(dst, reader->buf + reader->r, copied);
  reader->r += copied;
  reader->last_byte = reader->buf[reader->r - 1u];
  reader->last_rune_size = -1;
  return br_io_result_make(copied, BR_STATUS_OK);
}

br_bufio_reader_byte_result br_bufio_reader_read_byte(br_bufio_reader *reader) {
  u8 byte_value;

  if (reader == NULL) {
    return br_io_byte_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  reader->last_rune_size = -1;
  while (reader->r == reader->w) {
    if (reader->err != BR_STATUS_OK) {
      return br_io_byte_result_make(0u, br__bufio_reader_consume_err(reader));
    }
    if (br__bufio_reader_fill(reader) != BR_STATUS_OK) {
      return br_io_byte_result_make(0u, BR_STATUS_INVALID_STATE);
    }
  }

  byte_value = reader->buf[reader->r];
  reader->r += 1u;
  reader->last_byte = byte_value;
  return br_io_byte_result_make(byte_value, BR_STATUS_OK);
}

br_status br_bufio_reader_unread_byte(br_bufio_reader *reader) {
  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (reader->last_byte < 0 || (reader->r == 0u && reader->w > 0u) || reader->cap == 0u) {
    return BR_STATUS_INVALID_STATE;
  }

  if (reader->r > 0u) {
    reader->r -= 1u;
  } else {
    reader->w = 1u;
  }
  reader->buf[reader->r] = (u8)reader->last_byte;
  reader->last_byte = -1;
  reader->last_rune_size = -1;
  return BR_STATUS_OK;
}

br_bufio_reader_rune_result br_bufio_reader_read_rune(br_bufio_reader *reader) {
  br_rune value;
  usize size;

  if (reader == NULL) {
    return br_io_rune_result_make(0, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  while (reader->r + BR_UTF8_MAX > reader->w &&
         !br_utf8_full_rune(br_bytes_view_make(reader->buf + reader->r, reader->w - reader->r)) &&
         reader->err == BR_STATUS_OK && br_bufio_reader_buffered(reader) < reader->cap) {
    if (br__bufio_reader_fill(reader) != BR_STATUS_OK) {
      return br_io_rune_result_make(0, 0u, BR_STATUS_INVALID_STATE);
    }
  }

  reader->last_rune_size = -1;
  if (reader->r == reader->w) {
    return br_io_rune_result_make(0, 0u, br__bufio_reader_consume_err(reader));
  }

  value = (br_rune)reader->buf[reader->r];
  size = 1u;
  if (value >= BR_RUNE_SELF) {
    br_utf8_decode_result decoded;

    decoded = br_utf8_decode(br_bytes_view_make(reader->buf + reader->r, reader->w - reader->r));
    value = decoded.value;
    size = decoded.width;
  }

  reader->r += size;
  reader->last_byte = reader->buf[reader->r - 1u];
  reader->last_rune_size = (isize)size;
  return br_io_rune_result_make(value, size, BR_STATUS_OK);
}

br_status br_bufio_reader_unread_rune(br_bufio_reader *reader) {
  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (reader->last_rune_size < 0 || reader->r < (usize)reader->last_rune_size) {
    return BR_STATUS_INVALID_STATE;
  }

  reader->r -= (usize)reader->last_rune_size;
  reader->last_byte = -1;
  reader->last_rune_size = -1;
  return BR_STATUS_OK;
}

br_bufio_reader_slice_result br_bufio_reader_read_slice(br_bufio_reader *reader, u8 delim) {
  usize search_from;

  if (reader == NULL) {
    return br__bufio_reader_peek_result(br_bytes_view_make(NULL, 0u), BR_STATUS_INVALID_ARGUMENT);
  }

  search_from = 0u;
  for (;;) {
    isize index;
    br_bytes_view line;
    br_status status;

    index = br_bytes_index_byte(br_bytes_view_make(reader->buf + reader->r + search_from,
                                                   reader->w - reader->r - search_from),
                                delim);
    if (index >= 0) {
      usize len;

      len = search_from + (usize)index + 1u;
      line = br_bytes_view_make(reader->buf + reader->r, len);
      reader->r += len;
      if (len > 0u) {
        reader->last_byte = line.data[len - 1u];
        reader->last_rune_size = -1;
      }
      return br__bufio_reader_peek_result(line, BR_STATUS_OK);
    }

    if (reader->err != BR_STATUS_OK) {
      line = br_bytes_view_make(reader->buf + reader->r, reader->w - reader->r);
      reader->r = reader->w;
      status = br__bufio_reader_consume_err(reader);
      if (line.len > 0u) {
        reader->last_byte = line.data[line.len - 1u];
        reader->last_rune_size = -1;
      }
      return br__bufio_reader_peek_result(line, status);
    }

    if (br_bufio_reader_buffered(reader) >= reader->cap) {
      line = br_bytes_view_make(reader->buf, reader->w);
      reader->r = reader->w;
      if (line.len > 0u) {
        reader->last_byte = line.data[line.len - 1u];
        reader->last_rune_size = -1;
      }
      return br__bufio_reader_peek_result(line, BR_STATUS_BUFFER_FULL);
    }

    search_from = br_bufio_reader_buffered(reader);
    if (br__bufio_reader_fill(reader) != BR_STATUS_OK) {
      return br__bufio_reader_peek_result(br_bytes_view_make(NULL, 0u), BR_STATUS_INVALID_STATE);
    }
  }
}

br_bytes_result
br_bufio_reader_read_bytes(br_bufio_reader *reader, u8 delim, br_allocator allocator) {
  br_byte_buffer buffer;
  br_status init_status;
  br_status final_status;

  if (reader == NULL) {
    return (br_bytes_result){br_bytes_make(NULL, 0u), BR_STATUS_INVALID_ARGUMENT};
  }

  br_byte_buffer_init(&buffer, allocator);
  final_status = BR_STATUS_OK;
  for (;;) {
    br_bufio_reader_slice_result slice_result;
    br_byte_buffer_io_result write_result;

    slice_result = br_bufio_reader_read_slice(reader, delim);
    if (slice_result.value.len > 0u) {
      write_result = br_byte_buffer_write(&buffer, slice_result.value);
      if (write_result.status != BR_STATUS_OK) {
        br_byte_buffer_destroy(&buffer);
        return (br_bytes_result){br_bytes_make(NULL, 0u), write_result.status};
      }
    }

    if (slice_result.status == BR_STATUS_BUFFER_FULL) {
      continue;
    }

    final_status = slice_result.status;
    break;
  }

  init_status = final_status;
  return (br_bytes_result){br__bufio_take_byte_buffer(&buffer), init_status};
}

br_string_result
br_bufio_reader_read_string(br_bufio_reader *reader, u8 delim, br_allocator allocator) {
  br_bytes_result bytes_result;

  bytes_result = br_bufio_reader_read_bytes(reader, delim, allocator);
  return (br_string_result){br_string_make(bytes_result.value.data, bytes_result.value.len),
                            bytes_result.status};
}

br_i64_result br_bufio_reader_write_to(br_bufio_reader *reader, br_stream sink) {
  br_i64_result written_result;
  i64 total;

  if (reader == NULL) {
    return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }

  written_result = br__bufio_reader_write_buffer(reader, sink);
  total = written_result.value;
  if (written_result.status != BR_STATUS_OK) {
    return br_i64_result_make(total, written_result.status);
  }

  if (br_bufio_reader_buffered(reader) < reader->cap) {
    br_status fill_status;

    fill_status = br__bufio_reader_fill(reader);
    if (fill_status != BR_STATUS_OK) {
      return br_i64_result_make(total, fill_status);
    }
  }

  while (reader->r < reader->w) {
    br_status fill_status;

    written_result = br__bufio_reader_write_buffer(reader, sink);
    total += written_result.value;
    if (written_result.status != BR_STATUS_OK) {
      return br_i64_result_make(total, written_result.status);
    }

    fill_status = br__bufio_reader_fill(reader);
    if (fill_status != BR_STATUS_OK) {
      return br_i64_result_make(total, fill_status);
    }
  }

  if (reader->err == BR_STATUS_EOF) {
    reader->err = BR_STATUS_OK;
  }

  return br_i64_result_make(total, br__bufio_reader_consume_err(reader));
}

static br_i64_result br__bufio_reader_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  br_bufio_reader *reader;
  br_io_mode_set modes;
  br_bufio_reader_io_result io_result;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  reader = (br_bufio_reader *)context;
  switch (mode) {
    case BR_IO_MODE_READ:
      io_result = br_bufio_reader_read(reader, data, data_len);
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_DESTROY:
      br_bufio_reader_destroy(reader);
      return br_i64_result_make(0, BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_READ) | br_io_mode_bit(BR_IO_MODE_DESTROY);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_bufio_reader_as_stream(br_bufio_reader *reader) {
  return br_stream_make(reader, br__bufio_reader_stream_proc);
}
