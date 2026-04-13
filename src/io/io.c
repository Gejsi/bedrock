#include <bedrock/io/io.h>

br_io_result br_reader_read(br_reader reader, void *dst, usize dst_len) {
  if (reader.read == NULL) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  return reader.read(reader.context, dst, dst_len);
}

br_io_result br_writer_write(br_writer writer, const void *src, usize src_len) {
  if (writer.write == NULL) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  return writer.write(writer.context, src, src_len);
}

br_io_seek_result br_seeker_seek(br_seeker seeker, i64 offset, br_seek_from whence) {
  if (seeker.seek == NULL) {
    return br_io_seek_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }

  return seeker.seek(seeker.context, offset, whence);
}
