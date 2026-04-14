#include <assert.h>
#include <string.h>

#include <bedrock.h>

static void test_bufio_lookahead_basic(void) {
  br_byte_reader source;
  br_bufio_lookahead_reader reader;
  br_bufio_lookahead_reader_peek_result peek_result;
  br_bytes_view buffer_view;
  u8 backing[4];

  br_byte_reader_init(&source, BR_BYTES_LIT("abcdef"));
  assert(br_bufio_lookahead_reader_init_with_buffer(
           &reader, br_byte_reader_as_stream(&source), backing, BR_ARRAY_COUNT(backing)) ==
         BR_STATUS_OK);

  peek_result = br_bufio_lookahead_reader_peek(&reader, 2u);
  assert(peek_result.status == BR_STATUS_OK);
  assert(peek_result.value.len == 2u);
  assert(memcmp(peek_result.value.data, "ab", 2u) == 0);

  buffer_view = br_bufio_lookahead_reader_buffer(&reader);
  assert(buffer_view.len == 4u);
  assert(memcmp(buffer_view.data, "abcd", 4u) == 0);

  assert(br_bufio_lookahead_reader_consume(&reader, 1u) == BR_STATUS_OK);
  buffer_view = br_bufio_lookahead_reader_buffer(&reader);
  assert(buffer_view.len == 3u);
  assert(memcmp(buffer_view.data, "bcd", 3u) == 0);

  peek_result = br_bufio_lookahead_reader_peek_all(&reader);
  assert(peek_result.status == BR_STATUS_OK);
  assert(peek_result.value.len == 4u);
  assert(memcmp(peek_result.value.data, "bcde", 4u) == 0);

  assert(br_bufio_lookahead_reader_consume_all(&reader) == BR_STATUS_OK);
  buffer_view = br_bufio_lookahead_reader_buffer(&reader);
  assert(buffer_view.len == 0u);

  peek_result = br_bufio_lookahead_reader_peek_all(&reader);
  assert(peek_result.status == BR_STATUS_EOF);
  assert(peek_result.value.len == 1u);
  assert(memcmp(peek_result.value.data, "f", 1u) == 0);
}

static void test_bufio_lookahead_exact_size(void) {
  br_byte_reader source;
  br_bufio_lookahead_reader reader;
  br_bufio_lookahead_reader_peek_result peek_result;

  br_byte_reader_init(&source, BR_BYTES_LIT("abcdef"));
  assert(br_bufio_lookahead_reader_init_with_size(
           &reader, br_byte_reader_as_stream(&source), 3u, br_allocator_heap()) == BR_STATUS_OK);

  peek_result = br_bufio_lookahead_reader_peek(&reader, 4u);
  assert(peek_result.status == BR_STATUS_BUFFER_FULL);
  assert(peek_result.value.len == 0u);

  peek_result = br_bufio_lookahead_reader_peek_all(&reader);
  assert(peek_result.status == BR_STATUS_OK);
  assert(peek_result.value.len == 3u);
  assert(memcmp(peek_result.value.data, "abc", 3u) == 0);

  br_bufio_lookahead_reader_destroy(&reader);
}

static void test_bufio_lookahead_short_conditions(void) {
  br_byte_reader source;
  br_bufio_lookahead_reader reader;
  br_bufio_lookahead_reader_peek_result peek_result;
  u8 backing[3];

  br_byte_reader_init(&source, BR_BYTES_LIT("xy"));
  assert(br_bufio_lookahead_reader_init_with_buffer(
           &reader, br_byte_reader_as_stream(&source), backing, BR_ARRAY_COUNT(backing)) ==
         BR_STATUS_OK);

  peek_result = br_bufio_lookahead_reader_peek_all(&reader);
  assert(peek_result.status == BR_STATUS_EOF);
  assert(peek_result.value.len == 2u);
  assert(memcmp(peek_result.value.data, "xy", 2u) == 0);

  assert(br_bufio_lookahead_reader_consume(&reader, 3u) == BR_STATUS_SHORT_BUFFER);
  assert(br_bufio_lookahead_reader_consume(&reader, 2u) == BR_STATUS_OK);
  assert(br_bufio_lookahead_reader_buffer(&reader).len == 0u);
}

int main(void) {
  test_bufio_lookahead_basic();
  test_bufio_lookahead_exact_size();
  test_bufio_lookahead_short_conditions();
  return 0;
}
