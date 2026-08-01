#include <assert.h>
#include <string.h>

#include <bedrock.h>

typedef struct test_bufio_no_progress_reader {
  usize reads;
} test_bufio_no_progress_reader;

typedef struct test_bufio_short_sink {
  u8 data[64];
  usize max_per_write;
  usize written;
  usize calls;
} test_bufio_short_sink;

typedef struct test_bufio_progress_error_sink {
  u8 data[64];
  usize max_per_write;
  usize fail_after;
  usize written;
  usize calls;
  br_error error;
} test_bufio_progress_error_sink;

typedef struct test_bufio_data_error_reader {
  usize reads;
} test_bufio_data_error_reader;

typedef struct test_bufio_native_error_stream {
  br_io_mode fail_mode;
} test_bufio_native_error_stream;

typedef struct test_bufio_strict_allocation {
  void *ptr;
  usize size;
} test_bufio_strict_allocation;

typedef struct test_bufio_strict_allocator {
  test_bufio_strict_allocation allocations[8];
  usize allocation_count;
  usize size_errors;
} test_bufio_strict_allocator;

static br_alloc_result test_bufio_alloc_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static usize test_bufio_strict_find(const test_bufio_strict_allocator *strict, const void *ptr) {
  usize i;

  for (i = 0u; i < strict->allocation_count; i += 1u) {
    if (strict->allocations[i].ptr == ptr) {
      return i;
    }
  }
  return SIZE_MAX;
}

static br_alloc_result test_bufio_strict_allocator_proc(void *context,
                                                        const br_alloc_request *request) {
  test_bufio_strict_allocator *strict = (test_bufio_strict_allocator *)context;
  br_alloc_result result;
  usize index;

  switch (request->op) {
    case BR_ALLOC_OP_ALLOC:
    case BR_ALLOC_OP_ALLOC_UNINIT:
      result = br_allocator_call(br_allocator_heap(), request);
      if (result.status != BR_STATUS_OK || result.ptr == NULL) {
        return result;
      }
      if (strict->allocation_count == BR_ARRAY_COUNT(strict->allocations)) {
        (void)br_allocator_free(br_allocator_heap(), result.ptr, result.size);
        return test_bufio_alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
      }
      strict->allocations[strict->allocation_count].ptr = result.ptr;
      strict->allocations[strict->allocation_count].size = result.size;
      strict->allocation_count += 1u;
      return result;

    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
    case BR_ALLOC_OP_FREE:
      if (request->ptr == NULL) {
        return br_allocator_call(br_allocator_heap(), request);
      }
      index = test_bufio_strict_find(strict, request->ptr);
      if (index == SIZE_MAX || strict->allocations[index].size != request->old_size) {
        strict->size_errors += 1u;
        return test_bufio_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
      }

      result = br_allocator_call(br_allocator_heap(), request);
      if (result.status != BR_STATUS_OK) {
        return result;
      }
      if (request->op == BR_ALLOC_OP_FREE || request->size == 0u) {
        strict->allocation_count -= 1u;
        strict->allocations[index] = strict->allocations[strict->allocation_count];
      } else {
        strict->allocations[index].ptr = result.ptr;
        strict->allocations[index].size = result.size;
      }
      return result;

    case BR_ALLOC_OP_RESET:
      return test_bufio_alloc_result(NULL, 0u, BR_STATUS_NOT_SUPPORTED);
  }

  return test_bufio_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

static br_allocator test_bufio_strict_allocator_make(test_bufio_strict_allocator *strict) {
  br_allocator allocator;

  allocator.fn = test_bufio_strict_allocator_proc;
  allocator.ctx = strict;
  return allocator;
}

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

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  sink = (test_bufio_short_sink *)context;
  switch (mode) {
    case BR_IO_MODE_WRITE:
      count = br_min_size(data_len, sink->max_per_write);
      assert(sink->written + count <= BR_ARRAY_COUNT(sink->data));
      memcpy(sink->data + sink->written, data, count);
      sink->written += count;
      sink->calls += 1u;
      return br_i64_result_make((i64)count, BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      return br_stream_query_utility(br_io_mode_bit(BR_IO_MODE_WRITE));
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

static br_i64_result test_bufio_progress_error_sink_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  test_bufio_progress_error_sink *sink;
  usize count;
  usize remaining;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  sink = (test_bufio_progress_error_sink *)context;
  switch (mode) {
    case BR_IO_MODE_WRITE:
      remaining = sink->fail_after - sink->written;
      count = br_min_size(data_len, sink->max_per_write);
      count = br_min_size(count, remaining);
      assert(sink->written + count <= BR_ARRAY_COUNT(sink->data));
      memcpy(sink->data + sink->written, data, count);
      sink->written += count;
      sink->calls += 1u;
      if (sink->written == sink->fail_after) {
        return br_i64_result_make_error((i64)count, sink->error);
      }
      return br_i64_result_make((i64)count, BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      return br_stream_query_utility(br_io_mode_bit(BR_IO_MODE_WRITE));
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

static br_i64_result test_bufio_data_error_reader_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  test_bufio_data_error_reader *reader;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  reader = (test_bufio_data_error_reader *)context;
  switch (mode) {
    case BR_IO_MODE_READ:
      reader->reads += 1u;
      if (reader->reads == 1u) {
        assert(data_len >= 3u);
        memcpy(data, "abc", 3u);
        return br_i64_result_make(3, BR_STATUS_INVALID_ENCODING);
      }
      return br_i64_result_make(0, BR_STATUS_EOF);
    case BR_IO_MODE_QUERY:
      return br_stream_query_utility(br_io_mode_bit(BR_IO_MODE_READ));
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
}

static br_i64_result test_bufio_native_error_stream_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  test_bufio_native_error_stream *stream;

  BR_UNUSED(data);
  BR_UNUSED(data_len);
  BR_UNUSED(offset);
  BR_UNUSED(whence);

  stream = (test_bufio_native_error_stream *)context;
  if (mode == stream->fail_mode) {
    return br_i64_result_make_error(
      0, br_error_make_native(BR_STATUS_IO_ERROR, BR_ERROR_DOMAIN_WIN32, 5u));
  }
  if (mode == BR_IO_MODE_QUERY) {
    return br_stream_query_utility(br_io_mode_bit(stream->fail_mode));
  }
  return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
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
  br_bufio_reader_bytes_result bytes_result;
  br_bufio_reader_string_result string_result;
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

  bytes_result = br_bufio_reader_read_bytes(&line_reader, (u8)'\n', br_allocator_heap());
  assert(bytes_result.status == BR_STATUS_EOF);
  assert(bytes_result.value.len == 4u);
  assert(memcmp(bytes_result.value.data, "beta", 4u) == 0);
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

static void test_bufio_owned_result_sizes(void) {
  test_bufio_strict_allocator strict;
  br_allocator allocator;
  br_byte_reader source;
  br_bufio_reader reader;
  br_bufio_reader_bytes_result bytes_result;
  br_bufio_reader_string_result string_result;
  u8 backing[4];

  memset(&strict, 0, sizeof(strict));
  allocator = test_bufio_strict_allocator_make(&strict);

  br_byte_reader_init(&source, BR_BYTES_LIT("x\n"));
  assert(br_bufio_reader_init_with_buffer(
           &reader, br_byte_reader_as_stream(&source), backing, BR_ARRAY_COUNT(backing)) ==
         BR_STATUS_OK);
  bytes_result = br_bufio_reader_read_bytes(&reader, (u8)'\n', allocator);
  assert(bytes_result.status == BR_STATUS_OK);
  assert(br_bytes_equal(br_bytes_view_from_bytes(bytes_result.value), BR_BYTES_LIT("x\n")));
  assert(strict.allocation_count == 1u);
  assert(strict.allocations[0].ptr == bytes_result.value.data);
  assert(strict.allocations[0].size == bytes_result.value.len);
  assert(br_bytes_free(bytes_result.value, allocator) == BR_STATUS_OK);
  assert(strict.size_errors == 0u);
  assert(strict.allocation_count == 0u);

  br_byte_reader_init(&source, BR_BYTES_LIT("ok\n"));
  assert(br_bufio_reader_init_with_buffer(
           &reader, br_byte_reader_as_stream(&source), backing, BR_ARRAY_COUNT(backing)) ==
         BR_STATUS_OK);
  string_result = br_bufio_reader_read_string(&reader, (u8)'\n', allocator);
  assert(string_result.status == BR_STATUS_OK);
  assert(br_string_equal(br_string_view_from_string(string_result.value), BR_STR_LIT("ok\n")));
  assert(strict.allocation_count == 1u);
  assert(strict.allocations[0].ptr == string_result.value.data);
  assert(strict.allocations[0].size == string_result.value.len);
  assert(br_string_free(string_result.value, allocator) == BR_STATUS_OK);
  assert(strict.size_errors == 0u);
  assert(strict.allocation_count == 0u);
}

static void test_bufio_reader_write_to(void) {
  br_byte_reader source;
  br_bufio_reader reader;
  br_byte_buffer sink;
  br_bufio_reader_peek_result peek_result;
  br_i64_result write_result;
  br_bytes_view view;
  test_bufio_data_error_reader data_error_source;
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
  memset(&short_sink, 0, sizeof(short_sink));
  short_sink.max_per_write = 1u;
  write_result =
    br_bufio_reader_write_to(&reader, br_stream_make(&short_sink, test_bufio_short_sink_proc));
  assert(write_result.value == 4);
  assert(write_result.status == BR_STATUS_OK);
  assert(short_sink.written == 4u);
  assert(short_sink.calls == 4u);
  assert(memcmp(short_sink.data, "abcd", 4u) == 0);

  memset(&data_error_source, 0, sizeof(data_error_source));
  br_byte_buffer_init(&sink, br_allocator_heap());
  assert(br_bufio_reader_init_with_buffer(
           &reader,
           br_stream_make(&data_error_source, test_bufio_data_error_reader_proc),
           backing,
           BR_ARRAY_COUNT(backing)) == BR_STATUS_OK);
  write_result = br_bufio_reader_write_to(&reader, br_byte_buffer_as_stream(&sink));
  assert(write_result.value == 3);
  assert(write_result.status == BR_STATUS_INVALID_ENCODING);
  assert(data_error_source.reads == 1u);
  view = br_byte_buffer_view(&sink);
  assert(br_bytes_equal(view, BR_BYTES_LIT("abc")));
  br_byte_buffer_destroy(&sink);
}

static void test_bufio_writer_basic(void) {
  static const u8 expected[] = {'a', 'b', 'c', 0xc3u, 0xa4u, 0xefu, 0xbfu, 0xbdu, '!'};
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

  assert(br_bufio_writer_write_byte(&writer, (u8)'c').status == BR_STATUS_OK);

  io_result = br_bufio_writer_write_rune(&writer, (br_rune)0x00e4);
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);

  io_result = br_bufio_writer_write_rune(&writer, BR_RUNE_EOF);
  assert(io_result.count == 3u);
  assert(io_result.status == BR_STATUS_OK);

  io_result = br_bufio_writer_write_string(&writer, BR_STR_LIT("!"));
  assert(io_result.count == 1u);
  assert(io_result.status == BR_STATUS_OK);

  query_result = br_query(br_bufio_writer_as_stream(&writer));
  assert(query_result.status == BR_STATUS_OK);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_WRITE)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_FLUSH)) != 0u);

  assert(br_bufio_writer_flush(&writer).status == BR_STATUS_OK);
  view = br_byte_buffer_view(&sink);
  assert(view.len == BR_ARRAY_COUNT(expected));
  assert(memcmp(view.data, expected, view.len) == 0);

  br_byte_buffer_destroy(&sink);
}

static void test_bufio_writer_partial_writes(void) {
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

  assert(br_bufio_writer_flush(&writer).status == BR_STATUS_OK);
  assert(br_bufio_writer_buffered(&writer) == 0u);
  assert(sink.written == 4u);
  assert(sink.calls == 4u);
  assert(memcmp(sink.data, "abcd", 4u) == 0);

  io_result = br_bufio_writer_write(&writer, "efghij", 6u);
  assert(io_result.count == 6u);
  assert(io_result.status == BR_STATUS_OK);
  assert(br_bufio_writer_buffered(&writer) == 0u);
  assert(sink.written == 10u);
  assert(sink.calls == 10u);
  assert(memcmp(sink.data, "abcdefghij", 10u) == 0);

  assert(br_bufio_writer_write_byte(&writer, (u8)'!').status == BR_STATUS_OK);
  assert(br_bufio_writer_destroy(&writer).status == BR_STATUS_OK);
  assert(sink.written == 11u);
  assert(sink.calls == 11u);
  assert(memcmp(sink.data, "abcdefghij!", 11u) == 0);

  memset(&sink, 0, sizeof(sink));
  assert(br_bufio_writer_init_with_buffer(&writer,
                                          br_stream_make(&sink, test_bufio_short_sink_proc),
                                          backing,
                                          BR_ARRAY_COUNT(backing)) == BR_STATUS_OK);
  io_result = br_bufio_writer_write(&writer, "abcd", 4u);
  assert(io_result.count == 4u);
  assert(io_result.status == BR_STATUS_OK);
  assert(br_bufio_writer_flush(&writer).status == BR_STATUS_NO_PROGRESS);
  assert(br_bufio_writer_buffered(&writer) == 4u);
  assert(sink.calls == 1u);
  assert(br_bufio_writer_write_byte(&writer, (u8)'!').status == BR_STATUS_NO_PROGRESS);
  assert(br_bufio_writer_destroy(&writer).status == BR_STATUS_NO_PROGRESS);
  assert(sink.calls == 1u);
}

static void test_bufio_write_errors_after_progress(void) {
  test_bufio_progress_error_sink sink;
  br_byte_reader source;
  br_bufio_reader reader;
  br_bufio_reader_peek_result peek_result;
  br_i64_result transfer_result;
  br_bufio_writer writer;
  br_bufio_writer_io_result write_result;
  br_error error;
  u8 reader_backing[4];
  u8 writer_backing[4];

  memset(&sink, 0, sizeof(sink));
  sink.max_per_write = 1u;
  sink.fail_after = 2u;
  sink.error = br_error_make_native(BR_STATUS_IO_ERROR, BR_ERROR_DOMAIN_POSIX_ERRNO, 55u);

  br_byte_reader_init(&source, BR_BYTES_LIT("abcd"));
  assert(br_bufio_reader_init_with_buffer(&reader,
                                          br_byte_reader_as_stream(&source),
                                          reader_backing,
                                          BR_ARRAY_COUNT(reader_backing)) == BR_STATUS_OK);
  peek_result = br_bufio_reader_peek(&reader, 1u);
  assert(peek_result.status == BR_STATUS_OK);
  transfer_result =
    br_bufio_reader_write_to(&reader, br_stream_make(&sink, test_bufio_progress_error_sink_proc));
  assert(transfer_result.value == 2);
  assert(transfer_result.status == BR_STATUS_IO_ERROR);
  assert(transfer_result.native_error.domain == BR_ERROR_DOMAIN_POSIX_ERRNO);
  assert(transfer_result.native_error.code == 55u);
  assert(sink.calls == 2u);
  assert(memcmp(sink.data, "ab", 2u) == 0);
  assert(br_bufio_reader_buffered(&reader) == 2u);
  peek_result = br_bufio_reader_peek(&reader, 2u);
  assert(peek_result.status == BR_STATUS_OK);
  assert(br_bytes_equal(peek_result.value, BR_BYTES_LIT("cd")));

  memset(&sink, 0, sizeof(sink));
  sink.max_per_write = 1u;
  sink.fail_after = 2u;
  sink.error = br_error_make_native(BR_STATUS_IO_ERROR, BR_ERROR_DOMAIN_POSIX_ERRNO, 55u);
  assert(
    br_bufio_writer_init_with_buffer(&writer,
                                     br_stream_make(&sink, test_bufio_progress_error_sink_proc),
                                     writer_backing,
                                     BR_ARRAY_COUNT(writer_backing)) == BR_STATUS_OK);
  write_result = br_bufio_writer_write(&writer, "abcd", 4u);
  assert(write_result.count == 4u);
  assert(write_result.status == BR_STATUS_OK);
  error = br_bufio_writer_flush(&writer);
  assert(error.status == BR_STATUS_IO_ERROR);
  assert(error.native.domain == BR_ERROR_DOMAIN_POSIX_ERRNO);
  assert(error.native.code == 55u);
  assert(sink.calls == 2u);
  assert(memcmp(sink.data, "ab", 2u) == 0);
  assert(br_bufio_writer_buffered(&writer) == 2u);
  assert(memcmp(writer.buf, "cd", 2u) == 0);
  br_bufio_writer_discard(&writer);

  memset(&sink, 0, sizeof(sink));
  sink.max_per_write = 1u;
  sink.fail_after = 2u;
  sink.error = br_error_make_native(BR_STATUS_IO_ERROR, BR_ERROR_DOMAIN_POSIX_ERRNO, 55u);
  assert(
    br_bufio_writer_init_with_buffer(&writer,
                                     br_stream_make(&sink, test_bufio_progress_error_sink_proc),
                                     writer_backing,
                                     BR_ARRAY_COUNT(writer_backing)) == BR_STATUS_OK);
  write_result = br_bufio_writer_write(&writer, "abcdef", 6u);
  assert(write_result.count == 2u);
  assert(write_result.status == BR_STATUS_IO_ERROR);
  assert(write_result.native_error.domain == BR_ERROR_DOMAIN_POSIX_ERRNO);
  assert(write_result.native_error.code == 55u);
  assert(sink.calls == 2u);
  assert(memcmp(sink.data, "ab", 2u) == 0);
  assert(br_bufio_writer_buffered(&writer) == 0u);
  br_bufio_writer_discard(&writer);
}

static void test_bufio_writer_destroy_flushes(void) {
  br_byte_buffer sink;
  br_bufio_writer writer;
  br_bufio_writer_io_result io_result;
  br_bytes_view view;
  test_bufio_short_sink short_sink;
  test_bufio_strict_allocator strict;
  br_allocator allocator;

  br_byte_buffer_init(&sink, br_allocator_heap());
  assert(br_bufio_writer_init_with_size(
           &writer, br_byte_buffer_as_stream(&sink), 16u, br_allocator_heap()) == BR_STATUS_OK);

  io_result = br_bufio_writer_write(&writer, "pending", 7u);
  assert(io_result.count == 7u);
  assert(io_result.status == BR_STATUS_OK);
  assert(br_byte_buffer_len(&sink) == 0u);

  assert(br_bufio_writer_destroy(&writer).status == BR_STATUS_OK);
  view = br_byte_buffer_view(&sink);
  assert(br_bytes_equal(view, BR_BYTES_LIT("pending")));
  assert(writer.buf == NULL);
  assert(writer.cap == 0u);
  br_byte_buffer_destroy(&sink);

  memset(&short_sink, 0, sizeof(short_sink));
  short_sink.max_per_write = 1u;
  memset(&strict, 0, sizeof(strict));
  allocator = test_bufio_strict_allocator_make(&strict);
  assert(br_bufio_writer_init_with_size(
           &writer, br_stream_make(&short_sink, test_bufio_short_sink_proc), 16u, allocator) ==
         BR_STATUS_OK);
  io_result = br_bufio_writer_write(&writer, "pending", 7u);
  assert(io_result.count == 7u);
  assert(io_result.status == BR_STATUS_OK);

  assert(br_bufio_writer_destroy(&writer).status == BR_STATUS_OK);
  assert(short_sink.written == 7u);
  assert(short_sink.calls == 7u);
  assert(memcmp(short_sink.data, "pending", 7u) == 0);
  assert(strict.size_errors == 0u);
  assert(strict.allocation_count == 0u);
  assert(writer.buf == NULL);
  assert(writer.cap == 0u);
  assert(br_bufio_writer_destroy(NULL).status == BR_STATUS_OK);
}

static void test_bufio_writer_discard(void) {
  test_bufio_short_sink sink;
  test_bufio_strict_allocator strict;
  br_allocator allocator;
  br_bufio_writer writer;
  br_bufio_writer_io_result io_result;

  memset(&sink, 0, sizeof(sink));
  sink.max_per_write = 1u;
  memset(&strict, 0, sizeof(strict));
  allocator = test_bufio_strict_allocator_make(&strict);
  assert(br_bufio_writer_init_with_size(
           &writer, br_stream_make(&sink, test_bufio_short_sink_proc), 16u, allocator) ==
         BR_STATUS_OK);
  io_result = br_bufio_writer_write(&writer, "pending", 7u);
  assert(io_result.count == 7u);
  assert(io_result.status == BR_STATUS_OK);

  br_bufio_writer_discard(&writer);
  assert(sink.written == 0u);
  assert(strict.size_errors == 0u);
  assert(strict.allocation_count == 0u);
  assert(writer.buf == NULL);
  assert(writer.cap == 0u);
  br_bufio_writer_discard(NULL);
}

static void test_bufio_writer_stream_destroy_flushes(void) {
  br_byte_buffer sink;
  br_bufio_writer writer;
  br_bufio_writer_io_result io_result;
  br_bytes_view view;

  br_byte_buffer_init(&sink, br_allocator_heap());
  assert(br_bufio_writer_init_with_size(
           &writer, br_byte_buffer_as_stream(&sink), 16u, br_allocator_heap()) == BR_STATUS_OK);

  io_result = br_bufio_writer_write(&writer, "pending", 7u);
  assert(io_result.count == 7u);
  assert(io_result.status == BR_STATUS_OK);
  assert(br_byte_buffer_len(&sink) == 0u);

  assert(br_destroy(br_bufio_writer_as_stream(&writer)).status == BR_STATUS_OK);
  view = br_byte_buffer_view(&sink);
  assert(view.len == 7u);
  assert(memcmp(view.data, "pending", view.len) == 0);
  assert(writer.buf == NULL);
  assert(writer.cap == 0u);

  br_byte_buffer_destroy(&sink);

  {
    test_bufio_short_sink short_sink;
    u8 backing[4];

    memset(&short_sink, 0, sizeof(short_sink));
    short_sink.max_per_write = 1u;
    assert(br_bufio_writer_init_with_buffer(&writer,
                                            br_stream_make(&short_sink, test_bufio_short_sink_proc),
                                            backing,
                                            BR_ARRAY_COUNT(backing)) == BR_STATUS_OK);
    io_result = br_bufio_writer_write(&writer, "fail", 4u);
    assert(io_result.count == 4u);
    assert(io_result.status == BR_STATUS_OK);
    assert(br_destroy(br_bufio_writer_as_stream(&writer)).status == BR_STATUS_OK);
    assert(short_sink.written == 4u);
    assert(short_sink.calls == 4u);
    assert(memcmp(short_sink.data, "fail", 4u) == 0);
    assert(writer.buf == NULL);
    assert(writer.cap == 0u);
  }
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
  assert(br_bufio_writer_flush(&writer).status == BR_STATUS_OK);
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
  assert(br_flush(stream).status == BR_STATUS_OK);

  view = br_byte_buffer_view(&sink);
  assert(view.len == 2u);
  assert(memcmp(view.data, "xy", 2u) == 0);

  br_byte_buffer_destroy(&sink);
}

static void test_bufio_native_error_propagation(void) {
  test_bufio_native_error_stream source;
  br_bufio_reader reader;
  br_bufio_reader_byte_result byte_result;
  br_bufio_reader_bytes_result bytes_result;
  br_bufio_writer writer;
  br_bufio_writer_io_result write_result;
  br_error error;
  u8 backing[4];

  source.fail_mode = BR_IO_MODE_READ;
  assert(
    br_bufio_reader_init_with_buffer(&reader,
                                     br_stream_make(&source, test_bufio_native_error_stream_proc),
                                     backing,
                                     BR_ARRAY_COUNT(backing)) == BR_STATUS_OK);
  byte_result = br_bufio_reader_read_byte(&reader);
  assert(byte_result.status == BR_STATUS_IO_ERROR);
  assert(byte_result.native_error.domain == BR_ERROR_DOMAIN_WIN32);
  assert(byte_result.native_error.code == 5u);

  br_bufio_reader_reset(&reader, br_stream_make(&source, test_bufio_native_error_stream_proc));
  bytes_result = br_bufio_reader_read_bytes(&reader, (u8)'\n', br_allocator_heap());
  assert(bytes_result.status == BR_STATUS_IO_ERROR);
  assert(bytes_result.native_error.domain == BR_ERROR_DOMAIN_WIN32);
  assert(bytes_result.native_error.code == 5u);
  assert(br_bytes_free(bytes_result.value, br_allocator_heap()) == BR_STATUS_OK);

  source.fail_mode = BR_IO_MODE_WRITE;
  assert(
    br_bufio_writer_init_with_buffer(&writer,
                                     br_stream_make(&source, test_bufio_native_error_stream_proc),
                                     backing,
                                     BR_ARRAY_COUNT(backing)) == BR_STATUS_OK);
  write_result = br_bufio_writer_write(&writer, "data", 4u);
  assert(write_result.status == BR_STATUS_OK);
  error = br_bufio_writer_flush(&writer);
  assert(error.status == BR_STATUS_IO_ERROR);
  assert(error.native.domain == BR_ERROR_DOMAIN_WIN32);
  assert(error.native.code == 5u);
  br_bufio_writer_discard(&writer);
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
  test_bufio_owned_result_sizes();
  test_bufio_reader_write_to();
  test_bufio_writer_basic();
  test_bufio_writer_read_from();
  test_bufio_writer_partial_writes();
  test_bufio_write_errors_after_progress();
  test_bufio_writer_destroy_flushes();
  test_bufio_writer_discard();
  test_bufio_writer_stream_destroy_flushes();
  test_bufio_read_writer_stream();
  test_bufio_native_error_propagation();
  return 0;
}
