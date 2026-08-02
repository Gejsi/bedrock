#include <bedrock/bufio/read_writer.h>

void br_bufio_read_writer_init(br_bufio_read_writer *read_writer,
                               br_bufio_reader *reader,
                               br_bufio_writer *writer) {
  if (read_writer == NULL) {
    return;
  }

  read_writer->reader = reader;
  read_writer->writer = writer;
}

static br_i64_result br__bufio_read_writer_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  br_bufio_read_writer *read_writer;
  br_io_mode_set modes;
  br_bufio_reader_io_result read_result;
  br_bufio_writer_io_result write_result;
  br_error error;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  read_writer = (br_bufio_read_writer *)context;
  switch (mode) {
    case BR_IO_MODE_FLUSH:
      error = br_bufio_writer_flush(read_writer->writer);
      return br_i64_result_make_error(0, error);
    case BR_IO_MODE_READ:
      read_result = br_bufio_reader_read(read_writer->reader, data, data_len);
      return br_i64_result_make_error(
        (i64)read_result.count, br_io_error_make(read_result.status, read_result.native_error));
    case BR_IO_MODE_WRITE:
      write_result = br_bufio_writer_write(read_writer->writer, data, data_len);
      return br_i64_result_make_error(
        (i64)write_result.count, br_io_error_make(write_result.status, write_result.native_error));
    case BR_IO_MODE_WRITE_TO:
    case BR_IO_MODE_READ_FROM: {
      br_io_transfer_request request;

      if (data == NULL || data_len != sizeof(request)) {
        return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
      }
      memcpy(&request, data, sizeof(request));
      if (mode == BR_IO_MODE_WRITE_TO) {
        return br_bufio_reader_write_to(read_writer->reader, request.peer);
      }
      return br_bufio_writer_read_from(read_writer->writer, request.peer);
    }
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_FLUSH) | br_io_mode_bit(BR_IO_MODE_READ) |
              br_io_mode_bit(BR_IO_MODE_WRITE) | br_io_mode_bit(BR_IO_MODE_WRITE_TO) |
              br_io_mode_bit(BR_IO_MODE_READ_FROM);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_bufio_read_writer_as_stream(br_bufio_read_writer *read_writer) {
  return br_stream_make(read_writer, br__bufio_read_writer_stream_proc);
}
