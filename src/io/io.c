#include <limits.h>

#include <bedrock/io/io.h>

static br_io_result br__io_result_from_i64(br_i64_result result) {
  if (result.value < 0) {
    return br_io_result_make(0u, BR_STATUS_INVALID_STATE);
  }
  if ((u64)result.value > (u64)SIZE_MAX) {
    return br_io_result_make(0u, BR_STATUS_INVALID_STATE);
  }

  return br_io_result_make((usize)result.value, result.status);
}

static br_i64_result br__stream_call(
  br_stream stream, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  if (stream.procedure == NULL) {
    return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }

  return stream.procedure(stream.context, mode, data, data_len, offset, whence);
}

static br_io_result br__read_at_fallback(br_stream stream, void *dst, usize dst_len, i64 offset) {
  br_io_seek_result current;
  br_io_seek_result target;
  br_io_seek_result restore;
  br_io_result result;

  current = br_seek(stream, 0, BR_SEEK_FROM_CURRENT);
  if (current.status != BR_STATUS_OK) {
    return br_io_result_make(0u, current.status);
  }

  target = br_seek(stream, offset, BR_SEEK_FROM_START);
  if (target.status != BR_STATUS_OK) {
    return br_io_result_make(0u, target.status);
  }

  result = br_read(stream, dst, dst_len);
  restore = br_seek(stream, current.offset, BR_SEEK_FROM_START);
  if (restore.status != BR_STATUS_OK && result.status == BR_STATUS_OK) {
    result.status = restore.status;
  }
  return result;
}

static br_io_result
br__write_at_fallback(br_stream stream, const void *src, usize src_len, i64 offset) {
  br_io_seek_result current;
  br_io_seek_result target;
  br_io_seek_result restore;
  br_io_result result;

  current = br_seek(stream, 0, BR_SEEK_FROM_CURRENT);
  if (current.status != BR_STATUS_OK) {
    return br_io_result_make(0u, current.status);
  }

  target = br_seek(stream, offset, BR_SEEK_FROM_START);
  if (target.status != BR_STATUS_OK) {
    return br_io_result_make(0u, target.status);
  }

  result = br_write(stream, src, src_len);
  restore = br_seek(stream, current.offset, BR_SEEK_FROM_START);
  if (restore.status != BR_STATUS_OK && result.status == BR_STATUS_OK) {
    result.status = restore.status;
  }
  return result;
}

br_io_result br_read(br_stream stream, void *dst, usize dst_len) {
  if (dst == NULL && dst_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  return br__io_result_from_i64(
    br__stream_call(stream, BR_IO_MODE_READ, dst, dst_len, 0, BR_SEEK_FROM_START));
}

br_io_result br_write(br_stream stream, const void *src, usize src_len) {
  if (src == NULL && src_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  return br__io_result_from_i64(
    br__stream_call(stream, BR_IO_MODE_WRITE, (void *)src, src_len, 0, BR_SEEK_FROM_START));
}

br_io_result br_read_at(br_stream stream, void *dst, usize dst_len, i64 offset) {
  br_i64_result raw;

  if (dst == NULL && dst_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (offset < 0) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  raw = br__stream_call(stream, BR_IO_MODE_READ_AT, dst, dst_len, offset, BR_SEEK_FROM_START);
  if (raw.status == BR_STATUS_NOT_SUPPORTED) {
    return br__read_at_fallback(stream, dst, dst_len, offset);
  }

  return br__io_result_from_i64(raw);
}

br_io_result br_write_at(br_stream stream, const void *src, usize src_len, i64 offset) {
  br_i64_result raw;

  if (src == NULL && src_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (offset < 0) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  raw =
    br__stream_call(stream, BR_IO_MODE_WRITE_AT, (void *)src, src_len, offset, BR_SEEK_FROM_START);
  if (raw.status == BR_STATUS_NOT_SUPPORTED) {
    return br__write_at_fallback(stream, src, src_len, offset);
  }

  return br__io_result_from_i64(raw);
}

br_io_seek_result br_seek(br_stream stream, i64 offset, br_seek_from whence) {
  br_i64_result raw;

  raw = br__stream_call(stream, BR_IO_MODE_SEEK, NULL, 0u, offset, whence);
  return br_io_seek_result_make(raw.value, raw.status);
}

br_status br_close(br_stream stream) {
  return br__stream_call(stream, BR_IO_MODE_CLOSE, NULL, 0u, 0, BR_SEEK_FROM_START).status;
}

br_status br_flush(br_stream stream) {
  return br__stream_call(stream, BR_IO_MODE_FLUSH, NULL, 0u, 0, BR_SEEK_FROM_START).status;
}

br_status br_destroy(br_stream stream) {
  return br__stream_call(stream, BR_IO_MODE_DESTROY, NULL, 0u, 0, BR_SEEK_FROM_START).status;
}

br_io_size_result br_size(br_stream stream) {
  br_i64_result raw;
  br_io_seek_result current;
  br_io_seek_result end;
  br_io_seek_result restore;

  raw = br__stream_call(stream, BR_IO_MODE_SIZE, NULL, 0u, 0, BR_SEEK_FROM_START);
  if (raw.status != BR_STATUS_NOT_SUPPORTED) {
    return br_io_size_result_make(raw.value, raw.status);
  }

  current = br_seek(stream, 0, BR_SEEK_FROM_CURRENT);
  if (current.status != BR_STATUS_OK) {
    return br_io_size_result_make(0, current.status);
  }

  end = br_seek(stream, 0, BR_SEEK_FROM_END);
  if (end.status != BR_STATUS_OK) {
    return br_io_size_result_make(0, end.status);
  }

  restore = br_seek(stream, current.offset, BR_SEEK_FROM_START);
  if (restore.status != BR_STATUS_OK) {
    return br_io_size_result_make(0, restore.status);
  }

  return br_io_size_result_make(end.offset, BR_STATUS_OK);
}

br_io_query_result br_query(br_stream stream) {
  br_i64_result raw;
  br_io_mode_set modes;

  raw = br__stream_call(stream, BR_IO_MODE_QUERY, NULL, 0u, 0, BR_SEEK_FROM_START);
  if (raw.status != BR_STATUS_OK) {
    return br_io_query_result_make(0u, raw.status);
  }
  if (raw.value < 0) {
    return br_io_query_result_make(0u, BR_STATUS_INVALID_STATE);
  }

  modes = (br_io_mode_set)raw.value;
  modes |= br_io_mode_bit(BR_IO_MODE_QUERY);
  return br_io_query_result_make(modes, BR_STATUS_OK);
}
