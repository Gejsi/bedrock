#include <limits.h>

#include <bedrock/io/io.h>

enum { BR__IO_COPY_BUFFER_SIZE = 4096 };

/*
Odin relies on stream proc contracts to report sane byte counts. Bedrock
validates them here because arbitrary C callbacks can return impossible values.
*/
static br_io_result br__io_result_from_i64(br_i64_result result, usize requested) {
  if (result.value < 0) {
    return br_io_result_make(0u, BR_STATUS_INVALID_STATE);
  }
  if ((u64)result.value > (u64)SIZE_MAX) {
    return br_io_result_make(0u, BR_STATUS_INVALID_STATE);
  }
  if ((usize)result.value > requested) {
    return br_io_result_make(0u, BR_STATUS_INVALID_STATE);
  }

  return br_io_result_make_error((usize)result.value,
                                 br_io_error_make(result.status, result.native_error));
}

static usize br__utf8_expected_width(u8 byte_value) {
  if (byte_value < 0x80u) {
    return 1u;
  }
  if (byte_value >= 0xc2u && byte_value <= 0xdfu) {
    return 2u;
  }
  if (byte_value >= 0xe0u && byte_value <= 0xefu) {
    return 3u;
  }
  if (byte_value >= 0xf0u && byte_value <= 0xf4u) {
    return 4u;
  }

  return 1u;
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
    return br_io_result_make_error(0u, br_io_error_make(current.status, current.native_error));
  }

  target = br_seek(stream, offset, BR_SEEK_FROM_START);
  if (target.status != BR_STATUS_OK) {
    return br_io_result_make_error(0u, br_io_error_make(target.status, target.native_error));
  }

  result = br_read(stream, dst, dst_len);
  restore = br_seek(stream, current.offset, BR_SEEK_FROM_START);
  if (restore.status != BR_STATUS_OK && result.status == BR_STATUS_OK) {
    result.status = restore.status;
    result.native_error = restore.native_error;
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
    return br_io_result_make_error(0u, br_io_error_make(current.status, current.native_error));
  }

  target = br_seek(stream, offset, BR_SEEK_FROM_START);
  if (target.status != BR_STATUS_OK) {
    return br_io_result_make_error(0u, br_io_error_make(target.status, target.native_error));
  }

  result = br_write(stream, src, src_len);
  restore = br_seek(stream, current.offset, BR_SEEK_FROM_START);
  if (restore.status != BR_STATUS_OK && result.status == BR_STATUS_OK) {
    result.status = restore.status;
    result.native_error = restore.native_error;
  }
  return result;
}

br_io_result br_read(br_stream stream, void *dst, usize dst_len) {
  if (dst == NULL && dst_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  return br__io_result_from_i64(
    br__stream_call(stream, BR_IO_MODE_READ, dst, dst_len, 0, BR_SEEK_FROM_START), dst_len);
}

br_io_result br_read_at_least(br_stream stream, void *dst, usize dst_len, usize min_len) {
  u8 *cursor;
  usize total;
  br_status status;
  br_native_error native_error;

  if (dst == NULL && dst_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_len < min_len) {
    return br_io_result_make(0u, BR_STATUS_SHORT_BUFFER);
  }

  cursor = (u8 *)dst;
  total = 0u;
  status = BR_STATUS_OK;
  native_error = BR_NATIVE_ERROR_NONE;
  while (total < min_len && status == BR_STATUS_OK) {
    br_io_result result;

    result = br_read(stream, cursor + total, dst_len - total);
    total += result.count;
    if (result.status != BR_STATUS_OK) {
      status = result.status;
      native_error = result.native_error;
      break;
    }
    /*
    Odin assumes well-behaved readers here. Bedrock treats a zero-byte success
    as no progress so a buggy C callback cannot spin forever.
    */
    if (result.count == 0u) {
      status = BR_STATUS_NO_PROGRESS;
      break;
    }
  }

  if (total >= min_len) {
    return br_io_result_make(total, BR_STATUS_OK);
  }
  if (total > 0u && status == BR_STATUS_EOF) {
    return br_io_result_make(total, BR_STATUS_UNEXPECTED_EOF);
  }

  return br_io_result_make_error(total, br_io_error_make(status, native_error));
}

br_io_result br_read_full(br_stream stream, void *dst, usize dst_len) {
  return br_read_at_least(stream, dst, dst_len, dst_len);
}

br_io_result br_write(br_stream stream, const void *src, usize src_len) {
  if (src == NULL && src_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  return br__io_result_from_i64(
    br__stream_call(stream, BR_IO_MODE_WRITE, (void *)src, src_len, 0, BR_SEEK_FROM_START),
    src_len);
}

br_io_result br_write_at_least(br_stream stream, const void *src, usize src_len, usize min_len) {
  const u8 *cursor;
  usize total;
  br_status status;
  br_native_error native_error;

  if (src == NULL && src_len > 0u) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (src_len < min_len) {
    return br_io_result_make(0u, BR_STATUS_SHORT_BUFFER);
  }

  cursor = (const u8 *)src;
  total = 0u;
  status = BR_STATUS_OK;
  native_error = BR_NATIVE_ERROR_NONE;
  while (total < min_len && status == BR_STATUS_OK) {
    br_io_result result;

    result = br_write(stream, cursor + total, src_len - total);
    total += result.count;
    if (result.status != BR_STATUS_OK) {
      status = result.status;
      native_error = result.native_error;
      break;
    }
    /*
    Same reasoning as br_read_at_least: a zero-byte successful write would
    otherwise loop forever on a malformed C callback.
    */
    if (result.count == 0u) {
      status = BR_STATUS_NO_PROGRESS;
      break;
    }
  }

  return br_io_result_make_error(total, br_io_error_make(status, native_error));
}

br_io_result br_write_full(br_stream stream, const void *src, usize src_len) {
  return br_write_at_least(stream, src, src_len, src_len);
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

  return br__io_result_from_i64(raw, dst_len);
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

  return br__io_result_from_i64(raw, src_len);
}

br_io_seek_result br_seek(br_stream stream, i64 offset, br_seek_from whence) {
  br_i64_result raw;

  raw = br__stream_call(stream, BR_IO_MODE_SEEK, NULL, 0u, offset, whence);
  return br_io_seek_result_make_error(raw.value, br_io_error_make(raw.status, raw.native_error));
}

br_error br_close(br_stream stream) {
  br_i64_result result;

  result = br__stream_call(stream, BR_IO_MODE_CLOSE, NULL, 0u, 0, BR_SEEK_FROM_START);
  return br_io_error_make(result.status, result.native_error);
}

br_error br_flush(br_stream stream) {
  br_i64_result result;

  result = br__stream_call(stream, BR_IO_MODE_FLUSH, NULL, 0u, 0, BR_SEEK_FROM_START);
  return br_io_error_make(result.status, result.native_error);
}

br_error br_destroy(br_stream stream) {
  br_error first_error;
  br_error error;
  br_i64_result destroy_result;

  first_error = BR_ERROR_OK;
  error = br_flush(stream);
  if (error.status != BR_STATUS_OK && error.status != BR_STATUS_NOT_SUPPORTED) {
    first_error = error;
  }

  error = br_close(stream);
  if (first_error.status == BR_STATUS_OK && error.status != BR_STATUS_OK &&
      error.status != BR_STATUS_NOT_SUPPORTED) {
    first_error = error;
  }

  destroy_result = br__stream_call(stream, BR_IO_MODE_DESTROY, NULL, 0u, 0, BR_SEEK_FROM_START);
  if (first_error.status != BR_STATUS_OK) {
    return first_error;
  }
  return br_io_error_make(destroy_result.status, destroy_result.native_error);
}

br_io_size_result br_size(br_stream stream) {
  br_i64_result raw;
  br_io_seek_result current;
  br_io_seek_result end;
  br_io_seek_result restore;

  raw = br__stream_call(stream, BR_IO_MODE_SIZE, NULL, 0u, 0, BR_SEEK_FROM_START);
  if (raw.status != BR_STATUS_NOT_SUPPORTED) {
    return br_io_size_result_make_error(raw.value, br_io_error_make(raw.status, raw.native_error));
  }

  current = br_seek(stream, 0, BR_SEEK_FROM_CURRENT);
  if (current.status != BR_STATUS_OK) {
    return br_io_size_result_make_error(0, br_io_error_make(current.status, current.native_error));
  }

  end = br_seek(stream, 0, BR_SEEK_FROM_END);
  if (end.status != BR_STATUS_OK) {
    return br_io_size_result_make_error(0, br_io_error_make(end.status, end.native_error));
  }

  restore = br_seek(stream, current.offset, BR_SEEK_FROM_START);
  if (restore.status != BR_STATUS_OK) {
    return br_io_size_result_make_error(0, br_io_error_make(restore.status, restore.native_error));
  }

  return br_io_size_result_make(end.offset, BR_STATUS_OK);
}

br_io_query_result br_query(br_stream stream) {
  br_i64_result raw;
  br_io_mode_set modes;

  raw = br__stream_call(stream, BR_IO_MODE_QUERY, NULL, 0u, 0, BR_SEEK_FROM_START);
  if (raw.status != BR_STATUS_OK) {
    return br_io_query_result_make_error(0u, br_io_error_make(raw.status, raw.native_error));
  }
  if (raw.value < 0) {
    return br_io_query_result_make(0u, BR_STATUS_INVALID_STATE);
  }

  modes = (br_io_mode_set)raw.value;
  modes |= br_io_mode_bit(BR_IO_MODE_QUERY);
  return br_io_query_result_make(modes, BR_STATUS_OK);
}

br_io_byte_result br_read_byte(br_stream stream) {
  u8 byte_value;
  br_io_result result;

  result = br_read(stream, &byte_value, 1u);
  if (result.count == 1u) {
    return br_io_byte_result_make_error(byte_value,
                                        br_io_error_make(result.status, result.native_error));
  }
  if (result.status == BR_STATUS_OK) {
    return br_io_byte_result_make(0u, BR_STATUS_INVALID_STATE);
  }

  return br_io_byte_result_make_error(0u, br_io_error_make(result.status, result.native_error));
}

br_error br_write_byte(br_stream stream, u8 value) {
  br_io_result result;

  result = br_write_full(stream, &value, 1u);
  return br_io_error_make(result.status, result.native_error);
}

br_io_rune_result br_read_rune(br_stream stream) {
  u8 buffer[BR_UTF8_MAX];
  br_io_result first;
  br_utf8_decode_result decoded;
  usize expected;
  usize total;

  first = br_read(stream, buffer, 1u);
  if (first.count == 0u) {
    if (first.status == BR_STATUS_OK) {
      return br_io_rune_result_make(0, 0u, BR_STATUS_NO_PROGRESS);
    }
    return br_io_rune_result_make_error(0, 0u, br_io_error_make(first.status, first.native_error));
  }
  if (buffer[0] < (u8)BR_RUNE_SELF) {
    return br_io_rune_result_make_error(
      (br_rune)buffer[0], 1u, br_io_error_make(first.status, first.native_error));
  }

  expected = br__utf8_expected_width(buffer[0]);
  if (expected == 1u) {
    return br_io_rune_result_make_error(
      BR_RUNE_ERROR, 1u, br_io_error_make(first.status, first.native_error));
  }
  if (first.status != BR_STATUS_OK) {
    return br_io_rune_result_make_error(
      BR_RUNE_ERROR, 1u, br_io_error_make(first.status, first.native_error));
  }

  total = 1u;
  while (total < expected) {
    br_io_result tail;

    tail = br_read(stream, buffer + total, expected - total);
    total += tail.count;
    if (tail.status != BR_STATUS_OK) {
      if (total == expected) {
        decoded = br_utf8_decode(br_bytes_view_make(buffer, total));
        return br_io_rune_result_make_error(
          decoded.value, total, br_io_error_make(tail.status, tail.native_error));
      }
      return br_io_rune_result_make_error(
        BR_RUNE_ERROR, total, br_io_error_make(tail.status, tail.native_error));
    }
    if (tail.count == 0u) {
      return br_io_rune_result_make(BR_RUNE_ERROR, total, BR_STATUS_NO_PROGRESS);
    }
  }

  decoded = br_utf8_decode(br_bytes_view_make(buffer, total));
  return br_io_rune_result_make(decoded.value, total, BR_STATUS_OK);
}

br_io_result br_write_rune(br_stream stream, br_rune value) {
  br_utf8_encode_result encoded;

  encoded = br_utf8_encode(value);
  return br_write_full(stream, encoded.bytes, encoded.len);
}

br_i64_result br_copy(br_stream dst, br_stream src) {
  u8 buffer[BR__IO_COPY_BUFFER_SIZE];

  return br_copy_buffer(dst, src, buffer, BR_ARRAY_COUNT(buffer));
}

br_i64_result br_copy_buffer(br_stream dst, br_stream src, void *buffer, usize buffer_len) {
  u8 *scratch;
  i64 written;

  if (buffer == NULL || buffer_len == 0u) {
    return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }

  scratch = (u8 *)buffer;
  written = 0;
  for (;;) {
    br_io_result read_result;
    usize read_len;
    u64 remaining;

    if ((u64)written == (u64)INT64_MAX) {
      return br_i64_result_make(written, BR_STATUS_OUT_OF_RANGE);
    }
    remaining = (u64)INT64_MAX - (u64)written;
    read_len = buffer_len;
    if ((u64)read_len > remaining) {
      read_len = (usize)remaining;
    }
    read_result = br_read(src, scratch, read_len);
    if (read_result.count > 0u) {
      br_io_result write_result;

      write_result = br_write_full(dst, scratch, read_result.count);
      written += (i64)write_result.count;
      if (write_result.status != BR_STATUS_OK) {
        return br_i64_result_make_error(
          written, br_io_error_make(write_result.status, write_result.native_error));
      }
    }

    if (read_result.status != BR_STATUS_OK) {
      if (read_result.status == BR_STATUS_EOF) {
        return br_i64_result_make(written, BR_STATUS_OK);
      }
      return br_i64_result_make_error(
        written, br_io_error_make(read_result.status, read_result.native_error));
    }
    if (read_result.count == 0u) {
      return br_i64_result_make(written, BR_STATUS_INVALID_STATE);
    }
  }
}
