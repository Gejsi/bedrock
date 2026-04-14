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
  br_status status;

  (void)offset;
  (void)whence;

  read_writer = (br_bufio_read_writer *)context;
  switch (mode) {
    case BR_IO_MODE_FLUSH:
      status = br_bufio_writer_flush(read_writer->writer);
      return br_i64_result_make(0, status);
    case BR_IO_MODE_READ:
      read_result = br_bufio_reader_read(read_writer->reader, data, data_len);
      return br_i64_result_make((i64)read_result.count, read_result.status);
    case BR_IO_MODE_WRITE:
      write_result = br_bufio_writer_write(read_writer->writer, data, data_len);
      return br_i64_result_make((i64)write_result.count, write_result.status);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_FLUSH) | br_io_mode_bit(BR_IO_MODE_READ) |
              br_io_mode_bit(BR_IO_MODE_WRITE);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

br_stream br_bufio_read_writer_as_stream(br_bufio_read_writer *read_writer) {
  return br_stream_make(read_writer, br__bufio_read_writer_stream_proc);
}
