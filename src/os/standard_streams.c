#include <bedrock/os/standard_streams.h>

#include "standard_streams_internal.h"

static br_i64_result br__standard_stream_call(br__standard_stream_kind kind,
                                              br_io_mode mode,
                                              void *data,
                                              size_t data_len,
                                              int64_t offset,
                                              br_seek_from whence) {
  br_io_mode_set modes;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  switch (mode) {
    case BR_IO_MODE_READ:
      if (kind != BR__STANDARD_STREAM_STDIN) {
        return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
      }
      if (data_len == 0u) {
        return br_i64_result_make(0, BR_STATUS_OK);
      }
      return br__standard_stream_platform_read(kind, data, data_len);
    case BR_IO_MODE_WRITE:
      if (kind == BR__STANDARD_STREAM_STDIN) {
        return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
      }
      if (data_len == 0u) {
        return br_i64_result_make(0, BR_STATUS_OK);
      }
      return br__standard_stream_platform_write(kind, data, data_len);
    case BR_IO_MODE_DESTROY:
      return br_i64_result_make(0, BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_DESTROY);
      if (kind == BR__STANDARD_STREAM_STDIN) {
        modes |= br_io_mode_bit(BR_IO_MODE_READ);
      } else {
        modes |= br_io_mode_bit(BR_IO_MODE_WRITE);
      }
      return br_stream_query_utility(modes);
    case BR_IO_MODE_CLOSE:
    case BR_IO_MODE_FLUSH:
    case BR_IO_MODE_READ_AT:
    case BR_IO_MODE_WRITE_AT:
    case BR_IO_MODE_SEEK:
    case BR_IO_MODE_SIZE:
    case BR_IO_MODE_WRITE_TO:
    case BR_IO_MODE_READ_FROM:
    case BR_IO_MODE_COUNT:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }

  return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
}

static br_i64_result br__stdin_proc(void *context,
                                    br_io_mode mode,
                                    void *data,
                                    size_t data_len,
                                    int64_t offset,
                                    br_seek_from whence) {
  BR_UNUSED(context);
  return br__standard_stream_call(BR__STANDARD_STREAM_STDIN, mode, data, data_len, offset, whence);
}

static br_i64_result br__stdout_proc(void *context,
                                     br_io_mode mode,
                                     void *data,
                                     size_t data_len,
                                     int64_t offset,
                                     br_seek_from whence) {
  BR_UNUSED(context);
  return br__standard_stream_call(BR__STANDARD_STREAM_STDOUT, mode, data, data_len, offset, whence);
}

static br_i64_result br__stderr_proc(void *context,
                                     br_io_mode mode,
                                     void *data,
                                     size_t data_len,
                                     int64_t offset,
                                     br_seek_from whence) {
  BR_UNUSED(context);
  return br__standard_stream_call(BR__STANDARD_STREAM_STDERR, mode, data, data_len, offset, whence);
}

br_reader br_stdin(void) {
  return br_reader_make(NULL, br__stdin_proc);
}

br_writer br_stdout(void) {
  return br_writer_make(NULL, br__stdout_proc);
}

br_writer br_stderr(void) {
  return br_writer_make(NULL, br__stderr_proc);
}
