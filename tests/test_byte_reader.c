#include <assert.h>

#include <bedrock.h>

typedef struct test_byte_reader_writer {
  u8 data[16];
  usize written;
  usize max_per_write;
  usize fail_after;
  br_error error;
} test_byte_reader_writer;

static br_i64_result test_byte_reader_writer_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  test_byte_reader_writer *writer;
  usize count;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  writer = context;
  if (mode != BR_IO_MODE_WRITE) {
    return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }

  count = br_min_size(data_len, writer->max_per_write);
  if (writer->error.status != BR_STATUS_OK) {
    count = br_min_size(count, writer->fail_after - writer->written);
  }
  memcpy(writer->data + writer->written, data, count);
  writer->written += count;
  if (writer->error.status != BR_STATUS_OK && writer->written == writer->fail_after) {
    return br_i64_result_make_error((i64)count, writer->error);
  }
  return br_i64_result_make((i64)count, BR_STATUS_OK);
}

static void test_byte_reader_basic_read(void) {
  br_byte_reader reader;
  br_byte_reader_io_result io_result;
  br_byte_reader_byte_result byte_result;
  u8 scratch[8];

  br_byte_reader_init(&reader, BR_BYTES_LIT("abcdef"));
  assert(br_byte_reader_len(&reader) == 6u);
  assert(br_byte_reader_size(&reader) == 6u);
  assert(br_bytes_equal(br_byte_reader_view(&reader), BR_BYTES_LIT("abcdef")));

  io_result = br_byte_reader_read(&reader, scratch, 2u);
  assert(io_result.status == BR_STATUS_OK);
  assert(io_result.count == 2u);
  assert(io_result.native_error.domain == BR_ERROR_DOMAIN_NONE);
  assert(io_result.native_error.code == 0u);
  assert(br_bytes_equal(br_bytes_view_make(scratch, 2u), BR_BYTES_LIT("ab")));
  assert(br_bytes_equal(br_byte_reader_view(&reader), BR_BYTES_LIT("cdef")));

  byte_result = br_byte_reader_read_byte(&reader);
  assert(byte_result.status == BR_STATUS_OK);
  assert(byte_result.value == (u8)'c');
  assert(br_bytes_equal(br_byte_reader_view(&reader), BR_BYTES_LIT("def")));

  assert(br_byte_reader_unread_byte(&reader) == BR_STATUS_OK);
  assert(br_bytes_equal(br_byte_reader_view(&reader), BR_BYTES_LIT("cdef")));

  br_byte_reader_reset(&reader);
  assert(br_bytes_equal(br_byte_reader_view(&reader), BR_BYTES_LIT("abcdef")));
}

static void test_byte_reader_read_at_and_partial_rules(void) {
  br_byte_reader reader;
  br_byte_reader_io_result io_result;
  u8 scratch[8];

  br_byte_reader_init(&reader, BR_BYTES_LIT("abcd"));

  io_result = br_byte_reader_read_at(&reader, scratch, 2u, 1);
  assert(io_result.status == BR_STATUS_OK);
  assert(io_result.count == 2u);
  assert(br_bytes_equal(br_bytes_view_make(scratch, 2u), BR_BYTES_LIT("bc")));
  assert(br_bytes_equal(br_byte_reader_view(&reader), BR_BYTES_LIT("abcd")));

  io_result = br_byte_reader_read_at(&reader, scratch, 4u, 2);
  assert(io_result.status == BR_STATUS_EOF);
  assert(io_result.count == 2u);
  assert(br_bytes_equal(br_bytes_view_make(scratch, 2u), BR_BYTES_LIT("cd")));
  assert(br_bytes_equal(br_byte_reader_view(&reader), BR_BYTES_LIT("abcd")));

  io_result = br_byte_reader_read(&reader, scratch, 8u);
  assert(io_result.status == BR_STATUS_OK);
  assert(io_result.count == 4u);
  assert(br_bytes_equal(br_bytes_view_make(scratch, 4u), BR_BYTES_LIT("abcd")));

  io_result = br_byte_reader_read(&reader, scratch, 1u);
  assert(io_result.status == BR_STATUS_EOF);
  assert(io_result.count == 0u);
}

static void test_byte_reader_seek_semantics(void) {
  br_byte_reader reader;
  br_byte_reader_seek_result seek_result;
  br_byte_reader_io_result io_result;
  u8 scratch[4];

  br_byte_reader_init(&reader, BR_BYTES_LIT("abcdef"));

  seek_result = br_byte_reader_seek(&reader, 3, BR_SEEK_FROM_START);
  assert(seek_result.status == BR_STATUS_OK);
  assert(seek_result.offset == 3);
  assert(seek_result.native_error.domain == BR_ERROR_DOMAIN_NONE);
  assert(seek_result.native_error.code == 0u);
  assert(br_bytes_equal(br_byte_reader_view(&reader), BR_BYTES_LIT("def")));

  assert(br_byte_reader_unread_byte(&reader) == BR_STATUS_OK);
  assert(br_bytes_equal(br_byte_reader_view(&reader), BR_BYTES_LIT("cdef")));

  seek_result = br_byte_reader_seek(&reader, -1, BR_SEEK_FROM_END);
  assert(seek_result.status == BR_STATUS_OK);
  assert(seek_result.offset == 5);
  assert(br_bytes_equal(br_byte_reader_view(&reader), BR_BYTES_LIT("f")));

  seek_result = br_byte_reader_seek(&reader, 3, BR_SEEK_FROM_END);
  assert(seek_result.status == BR_STATUS_OK);
  assert(seek_result.offset == 9);
  assert(br_byte_reader_len(&reader) == 0u);

  io_result = br_byte_reader_read(&reader, scratch, 1u);
  assert(io_result.status == BR_STATUS_EOF);
  assert(io_result.count == 0u);

  seek_result = br_byte_reader_seek(&reader, -10, BR_SEEK_FROM_CURRENT);
  assert(seek_result.status == BR_STATUS_INVALID_ARGUMENT);

  seek_result = br_byte_reader_seek(&reader, INT64_MAX, BR_SEEK_FROM_START);
  assert(seek_result.status == BR_STATUS_OK);
  assert(seek_result.offset == INT64_MAX);
  seek_result = br_byte_reader_seek(&reader, 1, BR_SEEK_FROM_CURRENT);
  assert(seek_result.status == BR_STATUS_INVALID_ARGUMENT);
  seek_result = br_byte_reader_seek(&reader, 0, BR_SEEK_FROM_CURRENT);
  assert(seek_result.status == BR_STATUS_OK);
  assert(seek_result.offset == INT64_MAX);

  seek_result = br_byte_reader_seek(&reader, INT64_MAX, BR_SEEK_FROM_END);
  assert(seek_result.status == BR_STATUS_INVALID_ARGUMENT);

  seek_result = br_byte_reader_seek(&reader, 1, BR_SEEK_FROM_START);
  assert(seek_result.status == BR_STATUS_OK);
  seek_result = br_byte_reader_seek(&reader, INT64_MIN, BR_SEEK_FROM_CURRENT);
  assert(seek_result.status == BR_STATUS_INVALID_ARGUMENT);
  seek_result = br_byte_reader_seek(&reader, 0, BR_SEEK_FROM_CURRENT);
  assert(seek_result.status == BR_STATUS_OK);
  assert(seek_result.offset == 1);

  seek_result = br_byte_reader_seek(&reader, 0, (br_seek_from)99);
  assert(seek_result.status == BR_STATUS_INVALID_ARGUMENT);
}

static void test_byte_reader_oversized_view(void) {
#if SIZE_MAX > INT64_MAX
  u8 first;
  br_byte_reader reader;
  br_byte_reader_byte_result byte_result;
  br_io_size_result size_result;

  first = 42u;
  br_byte_reader_init(&reader, br_bytes_view_make(&first, (size_t)INT64_MAX + (size_t)1u));

  assert(br_byte_reader_len(&reader) == (size_t)INT64_MAX + (size_t)1u);
  byte_result = br_byte_reader_read_byte(&reader);
  assert(byte_result.status == BR_STATUS_OK);
  assert(byte_result.value == 42u);
  assert(br_byte_reader_len(&reader) == (size_t)INT64_MAX);

  size_result = br_size(br_byte_reader_as_stream(&reader));
  assert(size_result.status == BR_STATUS_OUT_OF_RANGE);
#endif
}

static void test_byte_reader_write_to(void) {
  br_byte_reader reader;
  test_byte_reader_writer writer;
  br_i64_result result;
  br_io_query_result query;
  br_stream stream;

  br_byte_reader_init(&reader, BR_BYTES_LIT("abcdef"));
  memset(&writer, 0, sizeof(writer));
  writer.max_per_write = 2u;
  writer.error = BR_ERROR_OK;
  result = br_byte_reader_write_to(&reader, br_stream_make(&writer, test_byte_reader_writer_proc));
  assert(result.status == BR_STATUS_OK);
  assert(result.value == 6);
  assert(writer.written == 6u);
  assert(memcmp(writer.data, "abcdef", 6u) == 0);
  assert(br_byte_reader_len(&reader) == 0u);

  br_byte_reader_init(&reader, BR_BYTES_LIT("abcdef"));
  memset(&writer, 0, sizeof(writer));
  writer.max_per_write = 4u;
  writer.fail_after = 3u;
  writer.error = br_error_make_native(BR_STATUS_IO_ERROR, BR_ERROR_DOMAIN_POSIX_ERRNO, 77u);
  result = br_byte_reader_write_to(&reader, br_stream_make(&writer, test_byte_reader_writer_proc));
  assert(result.status == BR_STATUS_IO_ERROR);
  assert(result.native_error.code == 77u);
  assert(result.value == 3);
  assert(br_bytes_equal(br_byte_reader_view(&reader), BR_BYTES_LIT("def")));

  stream = br_byte_reader_as_stream(&reader);
  query = br_query(stream);
  assert(query.status == BR_STATUS_OK);
  assert((query.modes & br_io_mode_bit(BR_IO_MODE_WRITE_TO)) != 0u);

  result = stream.procedure(stream.context, BR_IO_MODE_WRITE_TO, NULL, 0u, 0, BR_SEEK_FROM_START);
  assert(result.value == 0);
  assert(result.status == BR_STATUS_INVALID_ARGUMENT);
}

int main(void) {
  test_byte_reader_basic_read();
  test_byte_reader_read_at_and_partial_rules();
  test_byte_reader_seek_semantics();
  test_byte_reader_oversized_view();
  test_byte_reader_write_to();
  return 0;
}
