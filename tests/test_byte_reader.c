#include <assert.h>

#include <bedrock.h>

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

  seek_result = br_byte_reader_seek(&reader, 0, (br_seek_from)99);
  assert(seek_result.status == BR_STATUS_INVALID_ARGUMENT);
}

int main(void) {
  test_byte_reader_basic_read();
  test_byte_reader_read_at_and_partial_rules();
  test_byte_reader_seek_semantics();
  return 0;
}
