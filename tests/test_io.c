#include <assert.h>
#include <string.h>

#include <bedrock.h>

static void test_io_invalid_traits(void) {
  br_io_result io_result;
  br_io_seek_result seek_result;
  char buffer[4];

  io_result = br_reader_read(br_reader_make(NULL, NULL), buffer, sizeof(buffer));
  assert(io_result.count == 0u);
  assert(io_result.status == BR_STATUS_INVALID_ARGUMENT);

  io_result = br_writer_write(br_writer_make(NULL, NULL), buffer, sizeof(buffer));
  assert(io_result.count == 0u);
  assert(io_result.status == BR_STATUS_INVALID_ARGUMENT);

  seek_result = br_seeker_seek(br_seeker_make(NULL, NULL), 0, BR_SEEK_FROM_START);
  assert(seek_result.offset == 0);
  assert(seek_result.status == BR_STATUS_INVALID_ARGUMENT);
}

static void test_io_byte_reader_traits(void) {
  br_byte_reader concrete;
  br_reader reader;
  br_seeker seeker;
  br_io_result io_result;
  br_io_seek_result seek_result;
  char buffer[4];

  br_byte_reader_init(&concrete, BR_BYTES_LIT("abcdef"));
  reader = br_byte_reader_as_reader(&concrete);
  seeker = br_byte_reader_as_seeker(&concrete);

  assert(br_reader_is_valid(reader));
  assert(br_seeker_is_valid(seeker));

  io_result = br_reader_read(reader, buffer, 3u);
  assert(io_result.count == 3u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "abc", 3u) == 0);

  seek_result = br_seeker_seek(seeker, 1, BR_SEEK_FROM_CURRENT);
  assert(seek_result.offset == 4);
  assert(seek_result.status == BR_STATUS_OK);

  io_result = br_reader_read(reader, buffer, 2u);
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "ef", 2u) == 0);

  io_result = br_reader_read(reader, buffer, 1u);
  assert(io_result.count == 0u);
  assert(io_result.status == BR_STATUS_EOF);
}

static void test_io_byte_buffer_traits(void) {
  br_byte_buffer concrete;
  br_reader reader;
  br_writer writer;
  br_io_result io_result;
  char buffer[8];

  br_byte_buffer_init(&concrete, br_allocator_heap());
  reader = br_byte_buffer_as_reader(&concrete);
  writer = br_byte_buffer_as_writer(&concrete);

  assert(br_reader_is_valid(reader));
  assert(br_writer_is_valid(writer));

  io_result = br_writer_write(writer, "hello", 5u);
  assert(io_result.count == 5u);
  assert(io_result.status == BR_STATUS_OK);

  io_result = br_writer_write(writer, " world", 6u);
  assert(io_result.count == 6u);
  assert(io_result.status == BR_STATUS_OK);

  io_result = br_reader_read(reader, buffer, 6u);
  assert(io_result.count == 6u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "hello ", 6u) == 0);

  io_result = br_reader_read(reader, buffer, 5u);
  assert(io_result.count == 5u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "world", 5u) == 0);

  io_result = br_reader_read(reader, buffer, 1u);
  assert(io_result.count == 0u);
  assert(io_result.status == BR_STATUS_EOF);

  br_byte_buffer_destroy(&concrete);
}

static void test_io_string_traits(void) {
  static const u8 utf8_word[] = {'a', 0xc3u, 0xa4u, 'b'};
  br_string_reader concrete_reader;
  br_string_builder concrete_builder;
  br_reader reader;
  br_seeker seeker;
  br_writer writer;
  br_io_result io_result;
  br_io_seek_result seek_result;
  br_string_view built;
  char buffer[4];

  br_string_reader_init(&concrete_reader,
                        br_string_view_make(utf8_word, BR_ARRAY_COUNT(utf8_word)));
  br_string_builder_init(&concrete_builder, br_allocator_heap());
  reader = br_string_reader_as_reader(&concrete_reader);
  seeker = br_string_reader_as_seeker(&concrete_reader);
  writer = br_string_builder_as_writer(&concrete_builder);

  io_result = br_reader_read(reader, buffer, 3u);
  assert(io_result.count == 3u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, utf8_word, 3u) == 0);

  seek_result = br_seeker_seek(seeker, 0, BR_SEEK_FROM_START);
  assert(seek_result.offset == 0);
  assert(seek_result.status == BR_STATUS_OK);

  io_result = br_writer_write(writer, utf8_word, BR_ARRAY_COUNT(utf8_word));
  assert(io_result.count == BR_ARRAY_COUNT(utf8_word));
  assert(io_result.status == BR_STATUS_OK);

  built = br_string_builder_view(&concrete_builder);
  assert(built.len == BR_ARRAY_COUNT(utf8_word));
  assert(memcmp(built.data, utf8_word, built.len) == 0);

  br_string_builder_destroy(&concrete_builder);
}

int main(void) {
  test_io_invalid_traits();
  test_io_byte_reader_traits();
  test_io_byte_buffer_traits();
  test_io_string_traits();
  return 0;
}
