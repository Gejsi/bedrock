#include <assert.h>
#include <string.h>

#include <bedrock.h>

typedef struct test_bufio_no_progress_reader {
  usize reads;
} test_bufio_no_progress_reader;

typedef struct test_bufio_short_sink {
  usize max_per_write;
  usize written;
} test_bufio_short_sink;

static br_i64_result test_bufio_no_progress_reader_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  test_bufio_no_progress_reader *reader;

  BR_UNUSED(data);
  BR_UNUSED(data_len);
  BR_UNUSED(offset);
  BR_UNUSED(whence);

  reader = (test_bufio_no_progress_reader *)context;
  switch (mode) {
    case BR_IO_MODE_READ:
      reader->reads += 1u;
      return br_i64_result_make(0, BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      return br_stream_query_utility(br_io_mode_bit(BR_IO_MODE_READ));
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

static br_i64_result test_bufio_short_sink_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  test_bufio_short_sink *sink;
  usize count;

  BR_UNUSED(data);
  BR_UNUSED(offset);
  BR_UNUSED(whence);

  sink = (test_bufio_short_sink *)context;
  switch (mode) {
    case BR_IO_MODE_WRITE:
      count = br_min_size(data_len, sink->max_per_write);
      sink->written += count;
      return br_i64_result_make((i64)count, BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      return br_stream_query_utility(br_io_mode_bit(BR_IO_MODE_WRITE));
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

static void test_bufio_reader_basic(void) {
  br_byte_reader source;
  br_bufio_reader reader;
  br_bufio_reader_peek_result peek_result;
  br_bufio_reader_io_result io_result;
  br_bufio_reader_byte_result byte_result;
  u8 backing[4];
  char buffer[4];

  br_byte_reader_init(&source, BR_BYTES_LIT("abcdef"));
  assert(br_bufio_reader_init_with_buffer(
           &reader, br_byte_reader_as_stream(&source), backing, BR_ARRAY_COUNT(backing)) ==
         BR_STATUS_OK);

  peek_result = br_bufio_reader_peek(&reader, 3u);
  assert(peek_result.status == BR_STATUS_OK);
  assert(peek_result.value.len == 3u);
  assert(memcmp(peek_result.value.data, "abc", 3u) == 0);
  assert(br_bufio_reader_buffered(&reader) == 4u);

  io_result = br_bufio_reader_read(&reader, buffer, 2u);
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "ab", 2u) == 0);

  byte_result = br_bufio_reader_read_byte(&reader);
  assert(byte_result.value == (u8)'c');
  assert(byte_result.status == BR_STATUS_OK);
  assert(br_bufio_reader_unread_byte(&reader) == BR_STATUS_OK);

  byte_result = br_bufio_reader_read_byte(&reader);
  assert(byte_result.value == (u8)'c');
  assert(byte_result.status == BR_STATUS_OK);

  io_result = br_bufio_reader_discard(&reader, 1u);
  assert(io_result.count == 1u);
  assert(io_result.status == BR_STATUS_OK);

  io_result = br_bufio_reader_read(&reader, buffer, 2u);
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "ef", 2u) == 0);

  io_result = br_bufio_reader_read(&reader, buffer, 1u);
  assert(io_result.count == 0u);
  assert(io_result.status == BR_STATUS_EOF);
}

static void test_bufio_reader_runes_and_lines(void) {
  static const u8 utf8_word[] = {'a', 0xc3u, 0xa4u, 'b'};
  br_string_reader rune_source;
  br_bufio_reader rune_reader;
  br_byte_reader line_source;
  br_bufio_reader line_reader;
  br_byte_reader string_source;
  br_bufio_reader string_reader;
  br_bufio_reader_rune_result rune_result;
  br_bufio_reader_slice_result slice_result;
  br_bytes_result bytes_result;
  br_string_result string_result;
  u8 backing[4];

  br_string_reader_init(&rune_source, br_string_view_make(utf8_word, BR_ARRAY_COUNT(utf8_word)));
  assert(br_bufio_reader_init_with_buffer(&rune_reader,
                                          br_string_reader_as_stream(&rune_source),
                                          backing,
                                          BR_ARRAY_COUNT(backing)) == BR_STATUS_OK);

  rune_result = br_bufio_reader_read_rune(&rune_reader);
  assert(rune_result.value == (br_rune)'a');
  assert(rune_result.width == 1u);
  assert(rune_result.status == BR_STATUS_OK);

  rune_result = br_bufio_reader_read_rune(&rune_reader);
  assert(rune_result.value == (br_rune)0x00e4);
  assert(rune_result.width == 2u);
  assert(rune_result.status == BR_STATUS_OK);
  assert(br_bufio_reader_unread_rune(&rune_reader) == BR_STATUS_OK);

  rune_result = br_bufio_reader_read_rune(&rune_reader);
  assert(rune_result.value == (br_rune)0x00e4);
  assert(rune_result.width == 2u);
  assert(rune_result.status == BR_STATUS_OK);

  br_byte_reader_init(&line_source, BR_BYTES_LIT("abcde\n"));
  assert(br_bufio_reader_init_with_buffer(&line_reader,
                                          br_byte_reader_as_stream(&line_source),
                                          backing,
                                          BR_ARRAY_COUNT(backing)) == BR_STATUS_OK);

  slice_result = br_bufio_reader_read_slice(&line_reader, (u8)'\n');
  assert(slice_result.status == BR_STATUS_BUFFER_FULL);
  assert(slice_result.value.len == 4u);
  assert(memcmp(slice_result.value.data, "abcd", 4u) == 0);

  slice_result = br_bufio_reader_read_slice(&line_reader, (u8)'\n');
  assert(slice_result.status == BR_STATUS_OK);
  assert(slice_result.value.len == 2u);
  assert(memcmp(slice_result.value.data, "e\n", 2u) == 0);

  br_byte_reader_init(&line_source, BR_BYTES_LIT("alpha\nbeta"));
  assert(br_bufio_reader_init_with_buffer(&line_reader,
                                          br_byte_reader_as_stream(&line_source),
                                          backing,
                                          BR_ARRAY_COUNT(backing)) == BR_STATUS_OK);
  bytes_result = br_bufio_reader_read_bytes(&line_reader, (u8)'\n', br_allocator_heap());
  assert(bytes_result.status == BR_STATUS_OK);
  assert(bytes_result.value.len == 6u);
  assert(memcmp(bytes_result.value.data, "alpha\n", 6u) == 0);
  assert(br_bytes_free(bytes_result.value, br_allocator_heap()) == BR_STATUS_OK);

  br_byte_reader_init(&string_source, BR_BYTES_LIT("unterminated"));
  assert(br_bufio_reader_init_with_buffer(&string_reader,
                                          br_byte_reader_as_stream(&string_source),
                                          backing,
                                          BR_ARRAY_COUNT(backing)) == BR_STATUS_OK);
  string_result = br_bufio_reader_read_string(&string_reader, (u8)'\n', br_allocator_heap());
  assert(string_result.status == BR_STATUS_EOF);
  assert(string_result.value.len == 12u);
  assert(memcmp(string_result.value.data, "unterminated", 12u) == 0);
  assert(br_string_free(string_result.value, br_allocator_heap()) == BR_STATUS_OK);
}

static void test_bufio_reader_no_progress(void) {
  test_bufio_no_progress_reader source;
  br_bufio_reader reader;
  br_bufio_reader_byte_result byte_result;
  u8 backing[4];

  memset(&source, 0, sizeof(source));
  assert(
    br_bufio_reader_init_with_buffer(&reader,
                                     br_stream_make(&source, test_bufio_no_progress_reader_proc),
                                     backing,
                                     BR_ARRAY_COUNT(backing)) == BR_STATUS_OK);
  reader.max_consecutive_empty_reads = 2u;

  byte_result = br_bufio_reader_read_byte(&reader);
  assert(byte_result.status == BR_STATUS_NO_PROGRESS);
  assert(source.reads == 2u);
}

static void test_bufio_reader_write_to(void) {
  br_byte_reader source;
  br_bufio_reader reader;
  br_byte_buffer sink;
  br_bufio_reader_peek_result peek_result;
  br_i64_result write_result;
  br_bytes_view view;
  test_bufio_short_sink short_sink;
  u8 backing[4];

  br_byte_reader_init(&source, BR_BYTES_LIT("abcdef"));
  br_byte_buffer_init(&sink, br_allocator_heap());
  assert(br_bufio_reader_init_with_buffer(
           &reader, br_byte_reader_as_stream(&source), backing, BR_ARRAY_COUNT(backing)) ==
         BR_STATUS_OK);

  peek_result = br_bufio_reader_peek(&reader, 2u);
  assert(peek_result.status == BR_STATUS_OK);
  assert(peek_result.value.len == 2u);

  write_result = br_bufio_reader_write_to(&reader, br_byte_buffer_as_stream(&sink));
  assert(write_result.value == 6);
  assert(write_result.status == BR_STATUS_OK);
  assert(br_bufio_reader_buffered(&reader) == 0u);
  view = br_byte_buffer_view(&sink);
  assert(view.len == 6u);
  assert(memcmp(view.data, "abcdef", 6u) == 0);
  br_byte_buffer_destroy(&sink);

  br_byte_reader_init(&source, BR_BYTES_LIT("abcd"));
  assert(br_bufio_reader_init_with_buffer(
           &reader, br_byte_reader_as_stream(&source), backing, BR_ARRAY_COUNT(backing)) ==
         BR_STATUS_OK);
  peek_result = br_bufio_reader_peek(&reader, 1u);
  assert(peek_result.status == BR_STATUS_OK);
  short_sink.max_per_write = 1u;
  short_sink.written = 0u;
  write_result =
    br_bufio_reader_write_to(&reader, br_stream_make(&short_sink, test_bufio_short_sink_proc));
  assert(write_result.value == 1);
  assert(write_result.status == BR_STATUS_SHORT_WRITE);
  assert(short_sink.written == 1u);
}

static void test_bufio_writer_basic(void) {
  static const u8 expected[] = {'a', 'b', 'c', 0xc3u, 0xa4u, '!'};
  br_byte_buffer sink;
  br_bufio_writer writer;
  br_bufio_writer_io_result io_result;
  br_io_query_result query_result;
  br_bytes_view view;
  u8 backing[4];

  br_byte_buffer_init(&sink, br_allocator_heap());
  assert(br_bufio_writer_init_with_buffer(
           &writer, br_byte_buffer_as_stream(&sink), backing, BR_ARRAY_COUNT(backing)) ==
         BR_STATUS_OK);

  io_result = br_bufio_writer_write(&writer, "ab", 2u);
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);
  assert(br_bufio_writer_buffered(&writer) == 2u);
  assert(br_bufio_writer_available(&writer) == 2u);

  assert(br_bufio_writer_write_byte(&writer, (u8)'c') == BR_STATUS_OK);

  io_result = br_bufio_writer_write_rune(&writer, (br_rune)0x00e4);
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);

  io_result = br_bufio_writer_write_string(&writer, BR_STR_LIT("!"));
  assert(io_result.count == 1u);
  assert(io_result.status == BR_STATUS_OK);

  query_result = br_query(br_bufio_writer_as_stream(&writer));
  assert(query_result.status == BR_STATUS_OK);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_WRITE)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_FLUSH)) != 0u);

  assert(br_bufio_writer_flush(&writer) == BR_STATUS_OK);
  view = br_byte_buffer_view(&sink);
  assert(view.len == BR_ARRAY_COUNT(expected));
  assert(memcmp(view.data, expected, view.len) == 0);

  br_byte_buffer_destroy(&sink);
}

static void test_bufio_writer_short_write(void) {
  test_bufio_short_sink sink;
  br_bufio_writer writer;
  br_bufio_writer_io_result io_result;
  u8 backing[4];

  memset(&sink, 0, sizeof(sink));
  sink.max_per_write = 1u;
  assert(br_bufio_writer_init_with_buffer(&writer,
                                          br_stream_make(&sink, test_bufio_short_sink_proc),
                                          backing,
                                          BR_ARRAY_COUNT(backing)) == BR_STATUS_OK);

  io_result = br_bufio_writer_write(&writer, "abcd", 4u);
  assert(io_result.count == 4u);
  assert(io_result.status == BR_STATUS_OK);

  assert(br_bufio_writer_flush(&writer) == BR_STATUS_SHORT_WRITE);
  assert(br_bufio_writer_buffered(&writer) == 3u);
  assert(br_bufio_writer_write_byte(&writer, (u8)'!') == BR_STATUS_SHORT_WRITE);
  assert(sink.written == 1u);
}

static void test_bufio_writer_read_from(void) {
  br_byte_reader source;
  br_byte_buffer sink;
  br_bufio_writer writer;
  br_i64_result read_result;
  br_bytes_view view;
  test_bufio_no_progress_reader stuck_source;
  u8 backing[4];

  br_byte_reader_init(&source, BR_BYTES_LIT("abcdef"));
  br_byte_buffer_init(&sink, br_allocator_heap());
  assert(br_bufio_writer_init_with_buffer(
           &writer, br_byte_buffer_as_stream(&sink), backing, BR_ARRAY_COUNT(backing)) ==
         BR_STATUS_OK);

  read_result = br_bufio_writer_read_from(&writer, br_byte_reader_as_stream(&source));
  assert(read_result.value == 6);
  assert(read_result.status == BR_STATUS_OK);
  assert(br_bufio_writer_buffered(&writer) == 2u);
  view = br_byte_buffer_view(&sink);
  assert(view.len == 4u);
  assert(memcmp(view.data, "abcd", 4u) == 0);
  assert(br_bufio_writer_flush(&writer) == BR_STATUS_OK);
  view = br_byte_buffer_view(&sink);
  assert(view.len == 6u);
  assert(memcmp(view.data, "abcdef", 6u) == 0);
  br_byte_buffer_destroy(&sink);

  br_byte_buffer_init(&sink, br_allocator_heap());
  assert(br_bufio_writer_init_with_buffer(
           &writer, br_byte_buffer_as_stream(&sink), backing, BR_ARRAY_COUNT(backing)) ==
         BR_STATUS_OK);
  memset(&stuck_source, 0, sizeof(stuck_source));
  writer.max_consecutive_empty_writes = 2u;
  read_result = br_bufio_writer_read_from(
    &writer, br_stream_make(&stuck_source, test_bufio_no_progress_reader_proc));
  assert(read_result.value == 0);
  assert(read_result.status == BR_STATUS_NO_PROGRESS);
  assert(stuck_source.reads == 2u);
  br_byte_buffer_destroy(&sink);
}

static void test_bufio_read_writer_stream(void) {
  br_byte_reader source;
  br_byte_buffer sink;
  br_bufio_reader reader;
  br_bufio_writer writer;
  br_bufio_read_writer read_writer;
  br_stream stream;
  br_io_result io_result;
  br_io_query_result query_result;
  br_bytes_view view;
  char buffer[2];
  u8 reader_backing[4];
  u8 writer_backing[4];

  br_byte_reader_init(&source, BR_BYTES_LIT("hi"));
  br_byte_buffer_init(&sink, br_allocator_heap());
  assert(br_bufio_reader_init_with_buffer(&reader,
                                          br_byte_reader_as_stream(&source),
                                          reader_backing,
                                          BR_ARRAY_COUNT(reader_backing)) == BR_STATUS_OK);
  assert(br_bufio_writer_init_with_buffer(&writer,
                                          br_byte_buffer_as_stream(&sink),
                                          writer_backing,
                                          BR_ARRAY_COUNT(writer_backing)) == BR_STATUS_OK);
  br_bufio_read_writer_init(&read_writer, &reader, &writer);
  stream = br_bufio_read_writer_as_stream(&read_writer);

  query_result = br_query(stream);
  assert(query_result.status == BR_STATUS_OK);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_READ)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_WRITE)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_FLUSH)) != 0u);

  io_result = br_read(stream, buffer, sizeof(buffer));
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(buffer, "hi", 2u) == 0);

  io_result = br_write(stream, "xy", 2u);
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);
  assert(br_flush(stream) == BR_STATUS_OK);

  view = br_byte_buffer_view(&sink);
  assert(view.len == 2u);
  assert(memcmp(view.data, "xy", 2u) == 0);

  br_byte_buffer_destroy(&sink);
}

static void test_bufio_init_validation(void) {
  br_bufio_reader reader;
  br_bufio_writer writer;

  assert(br_bufio_reader_init_with_buffer(&reader, br_stream_make(NULL, NULL), NULL, 0u) ==
         BR_STATUS_INVALID_ARGUMENT);
  assert(br_bufio_writer_init_with_buffer(&writer, br_stream_make(NULL, NULL), NULL, 0u) ==
         BR_STATUS_INVALID_ARGUMENT);
}

int main(void) {
  test_bufio_init_validation();
  test_bufio_reader_basic();
  test_bufio_reader_runes_and_lines();
  test_bufio_reader_no_progress();
  test_bufio_reader_write_to();
  test_bufio_writer_basic();
  test_bufio_writer_read_from();
  test_bufio_writer_short_write();
  test_bufio_read_writer_stream();
  return 0;
}
