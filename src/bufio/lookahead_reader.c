#include <bedrock/bufio/lookahead_reader.h>

static br_bufio_lookahead_reader_peek_result
br__bufio_lookahead_reader_peek_result(br_bytes_view value, br_status status) {
  br_bufio_lookahead_reader_peek_result result;

  result.value = value;
  result.status = status;
  return result;
}

static void br__bufio_lookahead_reader_clear_state(br_bufio_lookahead_reader *reader,
                                                   br_stream source) {
  reader->source = source;
  reader->n = 0u;
}

br_status br_bufio_lookahead_reader_init(br_bufio_lookahead_reader *reader,
                                         br_stream source,
                                         br_allocator allocator) {
  return br_bufio_lookahead_reader_init_with_size(
    reader, source, BR_BUFIO_DEFAULT_BUFFER_SIZE, allocator);
}

br_status br_bufio_lookahead_reader_init_with_size(br_bufio_lookahead_reader *reader,
                                                   br_stream source,
                                                   usize size,
                                                   br_allocator allocator) {
  br_alloc_result alloc;

  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  memset(reader, 0, sizeof(*reader));
  if (size > 0u) {
    alloc = br_allocator_alloc_uninit(allocator, size, 1u);
    if (alloc.status != BR_STATUS_OK) {
      return alloc.status;
    }

    reader->buf = alloc.ptr;
    reader->owns_storage = true;
  }

  reader->cap = size;
  reader->allocator = allocator;
  br__bufio_lookahead_reader_clear_state(reader, source);
  return BR_STATUS_OK;
}

br_status br_bufio_lookahead_reader_init_with_buffer(br_bufio_lookahead_reader *reader,
                                                     br_stream source,
                                                     void *buffer,
                                                     usize buffer_len) {
  if (reader == NULL || (buffer == NULL && buffer_len > 0u)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  memset(reader, 0, sizeof(*reader));
  reader->buf = buffer;
  reader->cap = buffer_len;
  reader->allocator = br_allocator_null();
  reader->owns_storage = false;
  br__bufio_lookahead_reader_clear_state(reader, source);
  return BR_STATUS_OK;
}

void br_bufio_lookahead_reader_destroy(br_bufio_lookahead_reader *reader) {
  if (reader == NULL) {
    return;
  }

  if (reader->owns_storage && reader->buf != NULL) {
    (void)br_allocator_free(reader->allocator, reader->buf, reader->cap);
  }

  memset(reader, 0, sizeof(*reader));
}

void br_bufio_lookahead_reader_reset(br_bufio_lookahead_reader *reader, br_stream source) {
  if (reader == NULL) {
    return;
  }

  br__bufio_lookahead_reader_clear_state(reader, source);
}

br_bytes_view br_bufio_lookahead_reader_buffer(const br_bufio_lookahead_reader *reader) {
  if (reader == NULL || reader->n == 0u) {
    return br_bytes_view_make(NULL, 0u);
  }

  return br_bytes_view_make(reader->buf, reader->n);
}

br_bufio_lookahead_reader_peek_result
br_bufio_lookahead_reader_peek(br_bufio_lookahead_reader *reader, usize n) {
  br_status status;

  if (reader == NULL) {
    return br__bufio_lookahead_reader_peek_result(br_bytes_view_make(NULL, 0u),
                                                  BR_STATUS_INVALID_ARGUMENT);
  }
  if (n > reader->cap) {
    return br__bufio_lookahead_reader_peek_result(br_bytes_view_make(NULL, 0u),
                                                  BR_STATUS_BUFFER_FULL);
  }

  status = BR_STATUS_OK;
  if (reader->n < n) {
    br_io_result result;

    result = br_read_at_least(
      reader->source, reader->buf + reader->n, reader->cap - reader->n, n - reader->n);
    reader->n += result.count;
    if (result.status == BR_STATUS_UNEXPECTED_EOF) {
      status = BR_STATUS_EOF;
    } else {
      status = result.status;
    }
  }

  if (n > reader->n) {
    n = reader->n;
  }
  return br__bufio_lookahead_reader_peek_result(br_bytes_view_make(reader->buf, n), status);
}

br_bufio_lookahead_reader_peek_result
br_bufio_lookahead_reader_peek_all(br_bufio_lookahead_reader *reader) {
  if (reader == NULL) {
    return br__bufio_lookahead_reader_peek_result(br_bytes_view_make(NULL, 0u),
                                                  BR_STATUS_INVALID_ARGUMENT);
  }

  return br_bufio_lookahead_reader_peek(reader, reader->cap);
}

br_status br_bufio_lookahead_reader_consume(br_bufio_lookahead_reader *reader, usize n) {
  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (n > reader->n) {
    return BR_STATUS_SHORT_BUFFER;
  }
  if (n == 0u) {
    return BR_STATUS_OK;
  }

  memmove(reader->buf, reader->buf + n, reader->n - n);
  reader->n -= n;
  return BR_STATUS_OK;
}

br_status br_bufio_lookahead_reader_consume_all(br_bufio_lookahead_reader *reader) {
  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  reader->n = 0u;
  return BR_STATUS_OK;
}
