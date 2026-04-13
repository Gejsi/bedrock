#include <assert.h>
#include <string.h>

#include <bedrock.h>

typedef struct test_memory_stream {
  u8 data[32];
  usize len;
  i64 pos;
} test_memory_stream;

static br_i64_result test_memory_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  test_memory_stream *stream;
  usize count;
  i64 absolute;
  br_io_mode_set modes;

  stream = (test_memory_stream *)context;
  switch (mode) {
    case BR_IO_MODE_READ:
      if (stream->pos >= (i64)stream->len) {
        return br_i64_result_make(0, BR_STATUS_EOF);
      }

      count = br_min_size(data_len, stream->len - (usize)stream->pos);
      memcpy(data, stream->data + (usize)stream->pos, count);
      stream->pos += (i64)count;
      return br_i64_result_make((i64)count, BR_STATUS_OK);
    case BR_IO_MODE_WRITE:
      if (stream->pos < 0 || (usize)stream->pos > BR_ARRAY_COUNT(stream->data)) {
        return br_i64_result_make(0, BR_STATUS_INVALID_STATE);
      }
      if ((usize)stream->pos + data_len > BR_ARRAY_COUNT(stream->data)) {
        return br_i64_result_make(0, BR_STATUS_OUT_OF_MEMORY);
      }

      memcpy(stream->data + (usize)stream->pos, data, data_len);
      stream->pos += (i64)data_len;
      if ((usize)stream->pos > stream->len) {
        stream->len = (usize)stream->pos;
      }
      return br_i64_result_make((i64)data_len, BR_STATUS_OK);
    case BR_IO_MODE_SEEK:
      switch (whence) {
        case BR_SEEK_FROM_START:
          absolute = offset;
          break;
        case BR_SEEK_FROM_CURRENT:
          absolute = stream->pos + offset;
          break;
        case BR_SEEK_FROM_END:
          absolute = (i64)stream->len + offset;
          break;
        default:
          return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
      }

      if (absolute < 0) {
        return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
      }

      stream->pos = absolute;
      return br_i64_result_make(stream->pos, BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_READ) | br_io_mode_bit(BR_IO_MODE_WRITE) |
              br_io_mode_bit(BR_IO_MODE_SEEK);
      return br_stream_query_utility(modes);
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

static void test_io_invalid_stream(void) {
  br_stream stream;
  br_io_result io_result;
  br_io_seek_result seek_result;
  br_io_size_result size_result;
  br_io_query_result query_result;
  char buffer[4];

  stream = br_stream_make(NULL, NULL);

  io_result = br_read(stream, buffer, sizeof(buffer));
  assert(io_result.count == 0u);
  assert(io_result.status == BR_STATUS_NOT_SUPPORTED);

  io_result = br_write(stream, buffer, sizeof(buffer));
  assert(io_result.count == 0u);
  assert(io_result.status == BR_STATUS_NOT_SUPPORTED);

  seek_result = br_seek(stream, 0, BR_SEEK_FROM_START);
  assert(seek_result.offset == 0);
  assert(seek_result.status == BR_STATUS_NOT_SUPPORTED);

  size_result = br_size(stream);
  assert(size_result.size == 0);
  assert(size_result.status == BR_STATUS_NOT_SUPPORTED);

  query_result = br_query(stream);
  assert(query_result.modes == 0u);
  assert(query_result.status == BR_STATUS_NOT_SUPPORTED);
}

static void test_io_stream_fallbacks(void) {
  test_memory_stream memory = {{'a', 'b', 'c', 'd', 'e', 'f'}, 6u, 0};
  br_stream stream;
  br_io_result io_result;
  br_io_seek_result seek_result;
  br_io_size_result size_result;
  br_io_query_result query_result;
  char buffer[4];

  stream = br_stream_make(&memory, test_memory_stream_proc);

  query_result = br_query(stream);
  assert(query_result.status == BR_STATUS_OK);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_READ)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_WRITE)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_SEEK)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_READ_AT)) == 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_QUERY)) != 0u);

  io_result = br_read_at(stream, buffer, 3u, 2);
  assert(io_result.count == 3u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "cde", 3u) == 0);

  seek_result = br_seek(stream, 0, BR_SEEK_FROM_CURRENT);
  assert(seek_result.offset == 0);
  assert(seek_result.status == BR_STATUS_OK);

  io_result = br_write_at(stream, "ZZ", 2u, 1);
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);

  seek_result = br_seek(stream, 0, BR_SEEK_FROM_CURRENT);
  assert(seek_result.offset == 0);
  assert(seek_result.status == BR_STATUS_OK);

  size_result = br_size(stream);
  assert(size_result.size == 6);
  assert(size_result.status == BR_STATUS_OK);

  io_result = br_read(stream, buffer, 3u);
  assert(io_result.count == 3u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "aZZ", 3u) == 0);
}

static void test_io_byte_reader_stream(void) {
  br_byte_reader concrete;
  br_stream stream;
  br_io_result io_result;
  br_io_seek_result seek_result;
  br_io_size_result size_result;
  br_io_query_result query_result;
  char buffer[4];

  br_byte_reader_init(&concrete, BR_BYTES_LIT("abcdef"));
  stream = br_byte_reader_as_stream(&concrete);

  query_result = br_query(stream);
  assert(query_result.status == BR_STATUS_OK);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_READ)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_READ_AT)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_SEEK)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_SIZE)) != 0u);

  io_result = br_read(stream, buffer, 3u);
  assert(io_result.count == 3u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "abc", 3u) == 0);

  io_result = br_read_at(stream, buffer, 2u, 4);
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "ef", 2u) == 0);

  seek_result = br_seek(stream, 1, BR_SEEK_FROM_CURRENT);
  assert(seek_result.offset == 4);
  assert(seek_result.status == BR_STATUS_OK);

  size_result = br_size(stream);
  assert(size_result.size == 6);
  assert(size_result.status == BR_STATUS_OK);

  io_result = br_read(stream, buffer, 2u);
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "ef", 2u) == 0);
}

static void test_io_byte_buffer_stream(void) {
  br_byte_buffer concrete;
  br_stream stream;
  br_io_result io_result;
  br_io_size_result size_result;
  br_io_query_result query_result;
  char buffer[8];

  br_byte_buffer_init(&concrete, br_allocator_heap());
  stream = br_byte_buffer_as_stream(&concrete);

  query_result = br_query(stream);
  assert(query_result.status == BR_STATUS_OK);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_READ)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_WRITE)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_SIZE)) != 0u);

  io_result = br_write(stream, "hello", 5u);
  assert(io_result.count == 5u);
  assert(io_result.status == BR_STATUS_OK);

  io_result = br_write(stream, " world", 6u);
  assert(io_result.count == 6u);
  assert(io_result.status == BR_STATUS_OK);

  size_result = br_size(stream);
  assert(size_result.size == 11);
  assert(size_result.status == BR_STATUS_OK);

  io_result = br_read(stream, buffer, 6u);
  assert(io_result.count == 6u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "hello ", 6u) == 0);

  size_result = br_size(stream);
  assert(size_result.size == 5);
  assert(size_result.status == BR_STATUS_OK);

  io_result = br_read(stream, buffer, 5u);
  assert(io_result.count == 5u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "world", 5u) == 0);

  br_byte_buffer_destroy(&concrete);
}

static void test_io_string_streams(void) {
  static const u8 utf8_word[] = {'a', 0xc3u, 0xa4u, 'b'};
  br_string_reader concrete_reader;
  br_string_builder concrete_builder;
  br_stream reader_stream;
  br_stream builder_stream;
  br_io_result io_result;
  br_io_seek_result seek_result;
  br_io_size_result size_result;
  br_string_view built;
  char buffer[4];

  br_string_reader_init(&concrete_reader,
                        br_string_view_make(utf8_word, BR_ARRAY_COUNT(utf8_word)));
  br_string_builder_init(&concrete_builder, br_allocator_heap());

  reader_stream = br_string_reader_as_stream(&concrete_reader);
  builder_stream = br_string_builder_as_stream(&concrete_builder);

  io_result = br_read(reader_stream, buffer, 3u);
  assert(io_result.count == 3u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, utf8_word, 3u) == 0);

  seek_result = br_seek(reader_stream, 0, BR_SEEK_FROM_START);
  assert(seek_result.offset == 0);
  assert(seek_result.status == BR_STATUS_OK);

  size_result = br_size(reader_stream);
  assert(size_result.size == (i64)BR_ARRAY_COUNT(utf8_word));
  assert(size_result.status == BR_STATUS_OK);

  io_result = br_write(builder_stream, utf8_word, BR_ARRAY_COUNT(utf8_word));
  assert(io_result.count == BR_ARRAY_COUNT(utf8_word));
  assert(io_result.status == BR_STATUS_OK);

  size_result = br_size(builder_stream);
  assert(size_result.size == (i64)BR_ARRAY_COUNT(utf8_word));
  assert(size_result.status == BR_STATUS_OK);

  built = br_string_builder_view(&concrete_builder);
  assert(built.len == BR_ARRAY_COUNT(utf8_word));
  assert(memcmp(built.data, utf8_word, built.len) == 0);

  br_string_builder_destroy(&concrete_builder);
}

int main(void) {
  test_io_invalid_stream();
  test_io_stream_fallbacks();
  test_io_byte_reader_stream();
  test_io_byte_buffer_stream();
  test_io_string_streams();
  return 0;
}
