#include <bedrock/bytes/reader.h>

static br_byte_reader_io_result br__byte_reader_io_result(usize count, br_status status) {
  br_byte_reader_io_result result;

  result.count = count;
  result.status = status;
  return result;
}

static br_byte_reader_byte_result br__byte_reader_byte_result(u8 value, br_status status) {
  br_byte_reader_byte_result result;

  result.value = value;
  result.status = status;
  return result;
}

static br_byte_reader_seek_result br__byte_reader_seek_result(i64 offset, br_status status) {
  br_byte_reader_seek_result result;

  result.offset = offset;
  result.status = status;
  return result;
}

void br_byte_reader_init(br_byte_reader *reader, br_bytes_view source) {
  if (reader == NULL) {
    return;
  }

  reader->source = source;
  reader->index = 0;
}

void br_byte_reader_reset(br_byte_reader *reader) {
  if (reader == NULL) {
    return;
  }

  reader->index = 0;
}

br_bytes_view br_byte_reader_view(const br_byte_reader *reader) {
  usize remaining;

  if (reader == NULL) {
    return br_bytes_view_make(NULL, 0u);
  }

  remaining = br_byte_reader_len(reader);
  if (remaining == 0u) {
    return br_bytes_view_make(NULL, 0u);
  }

  return br_bytes_view_make(reader->source.data + (usize)reader->index, remaining);
}

usize br_byte_reader_len(const br_byte_reader *reader) {
  if (reader == NULL || reader->index >= (i64)reader->source.len) {
    return 0u;
  }
  if (reader->index < 0) {
    return reader->source.len;
  }

  return reader->source.len - (usize)reader->index;
}

usize br_byte_reader_size(const br_byte_reader *reader) {
  return reader != NULL ? reader->source.len : 0u;
}

br_byte_reader_io_result br_byte_reader_read(br_byte_reader *reader, void *dst, usize dst_len) {
  usize count;

  if (reader == NULL || (dst == NULL && dst_len > 0u)) {
    return br__byte_reader_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_len == 0u) {
    return br__byte_reader_io_result(0u, BR_STATUS_OK);
  }
  if (reader->index >= (i64)reader->source.len) {
    return br__byte_reader_io_result(0u, BR_STATUS_EOF);
  }

  count = br_min_size(dst_len, reader->source.len - (usize)reader->index);
  memcpy(dst, reader->source.data + (usize)reader->index, count);
  reader->index += (i64)count;
  return br__byte_reader_io_result(count, BR_STATUS_OK);
}

br_byte_reader_io_result
br_byte_reader_read_at(const br_byte_reader *reader, void *dst, usize dst_len, i64 offset) {
  usize count;

  if (reader == NULL || (dst == NULL && dst_len > 0u)) {
    return br__byte_reader_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_len == 0u) {
    return br__byte_reader_io_result(0u, BR_STATUS_OK);
  }
  if (offset < 0) {
    return br__byte_reader_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (offset >= (i64)reader->source.len) {
    return br__byte_reader_io_result(0u, BR_STATUS_EOF);
  }

  count = br_min_size(dst_len, reader->source.len - (usize)offset);
  memcpy(dst, reader->source.data + (usize)offset, count);
  if (count < dst_len) {
    return br__byte_reader_io_result(count, BR_STATUS_EOF);
  }
  return br__byte_reader_io_result(count, BR_STATUS_OK);
}

br_byte_reader_byte_result br_byte_reader_read_byte(br_byte_reader *reader) {
  if (reader == NULL) {
    return br__byte_reader_byte_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (reader->index >= (i64)reader->source.len) {
    return br__byte_reader_byte_result(0u, BR_STATUS_EOF);
  }

  reader->index += 1;
  return br__byte_reader_byte_result(reader->source.data[(usize)reader->index - 1u], BR_STATUS_OK);
}

br_status br_byte_reader_unread_byte(br_byte_reader *reader) {
  if (reader == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (reader->index <= 0) {
    return BR_STATUS_INVALID_STATE;
  }

  reader->index -= 1;
  return BR_STATUS_OK;
}

br_byte_reader_seek_result
br_byte_reader_seek(br_byte_reader *reader, i64 offset, br_seek_from whence) {
  i64 absolute;

  if (reader == NULL) {
    return br__byte_reader_seek_result(0, BR_STATUS_INVALID_ARGUMENT);
  }

  switch (whence) {
    case BR_SEEK_FROM_START:
      absolute = offset;
      break;
    case BR_SEEK_FROM_CURRENT:
      absolute = reader->index + offset;
      break;
    case BR_SEEK_FROM_END:
      absolute = (i64)reader->source.len + offset;
      break;
    default:
      return br__byte_reader_seek_result(0, BR_STATUS_INVALID_ARGUMENT);
  }

  if (absolute < 0) {
    return br__byte_reader_seek_result(0, BR_STATUS_INVALID_ARGUMENT);
  }

  reader->index = absolute;
  return br__byte_reader_seek_result(absolute, BR_STATUS_OK);
}

static br_i64_result br__byte_reader_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  br_byte_reader *reader;
  br_byte_reader_io_result io_result;
  br_byte_reader_seek_result seek_result;
  br_io_mode_set modes;

  reader = (br_byte_reader *)context;
  switch (mode) {
    case BR_IO_MODE_READ:
      io_result = br_byte_reader_read(reader, data, data_len);
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_READ_AT:
      io_result = br_byte_reader_read_at(reader, data, data_len, offset);
      return br_i64_result_make((i64)io_result.count, io_result.status);
    case BR_IO_MODE_SEEK:
      seek_result = br_byte_reader_seek(reader, offset, whence);
      return br_i64_result_make(seek_result.offset, seek_result.status);
    case BR_IO_MODE_SIZE:
      return br_i64_result_make((i64)br_byte_reader_size(reader), BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_READ) | br_io_mode_bit(BR_IO_MODE_READ_AT) |
              br_io_mode_bit(BR_IO_MODE_SEEK) | br_io_mode_bit(BR_IO_MODE_SIZE);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_byte_reader_as_stream(br_byte_reader *reader) {
  return br_stream_make(reader, br__byte_reader_stream_proc);
}
