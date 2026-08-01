#include <assert.h>

#include <bedrock.h>

static void test_byte_buffer_basic_write_read(void) {
  br_byte_buffer buffer;
  br_byte_buffer_io_result io_result;
  br_byte_buffer_byte_result byte_result;
  br_bytes_view next;
  u8 scratch[8];

  br_byte_buffer_init(&buffer, br_allocator_heap());

  assert(br_byte_buffer_write(&buffer, BR_BYTES_LIT("hello")).status == BR_STATUS_OK);
  assert(br_byte_buffer_write_byte(&buffer, (u8)'!') == BR_STATUS_OK);
  assert(br_byte_buffer_len(&buffer) == 6u);
  assert(br_bytes_equal(br_byte_buffer_view(&buffer), BR_BYTES_LIT("hello!")));

  next = br_byte_buffer_next(&buffer, 2u);
  assert(br_bytes_equal(next, BR_BYTES_LIT("he")));
  assert(br_bytes_equal(br_byte_buffer_view(&buffer), BR_BYTES_LIT("llo!")));

  io_result = br_byte_buffer_read(&buffer, scratch, 3u);
  assert(io_result.status == BR_STATUS_OK);
  assert(io_result.count == 3u);
  assert(br_bytes_equal(br_bytes_view_make(scratch, 3u), BR_BYTES_LIT("llo")));

  byte_result = br_byte_buffer_read_byte(&buffer);
  assert(byte_result.status == BR_STATUS_OK);
  assert(byte_result.value == (u8)'!');
  assert(br_byte_buffer_is_empty(&buffer));

  byte_result = br_byte_buffer_read_byte(&buffer);
  assert(byte_result.status == BR_STATUS_EOF);

  br_byte_buffer_destroy(&buffer);
}

static void test_byte_buffer_unread_and_truncate(void) {
  br_byte_buffer buffer;
  br_byte_buffer_byte_result byte_result;
  br_byte_buffer_io_result io_result;
  u8 scratch[3];

  br_byte_buffer_init(&buffer, br_allocator_heap());
  assert(br_byte_buffer_write(&buffer, BR_BYTES_LIT("abc")).status == BR_STATUS_OK);

  byte_result = br_byte_buffer_read_byte(&buffer);
  assert(byte_result.status == BR_STATUS_OK);
  assert(byte_result.value == (u8)'a');
  assert(br_byte_buffer_unread_byte(&buffer) == BR_STATUS_OK);
  assert(br_bytes_equal(br_byte_buffer_view(&buffer), BR_BYTES_LIT("abc")));
  assert(br_byte_buffer_unread_byte(&buffer) == BR_STATUS_INVALID_STATE);

  assert(br_byte_buffer_truncate(&buffer, 2u) == BR_STATUS_OK);
  assert(br_bytes_equal(br_byte_buffer_view(&buffer), BR_BYTES_LIT("ab")));
  assert(br_byte_buffer_truncate(&buffer, 3u) == BR_STATUS_INVALID_ARGUMENT);

  io_result = br_byte_buffer_read(&buffer, scratch, 2u);
  assert(io_result.status == BR_STATUS_OK);
  assert(io_result.count == 2u);
  assert(io_result.native_error.domain == BR_ERROR_DOMAIN_NONE);
  assert(io_result.native_error.code == 0u);
  assert(br_byte_buffer_unread_byte(&buffer) == BR_STATUS_OK);
  assert(br_bytes_equal(br_byte_buffer_view(&buffer), BR_BYTES_LIT("b")));

  assert(br_byte_buffer_read_byte(&buffer).status == BR_STATUS_OK);
  io_result = br_byte_buffer_read(&buffer, NULL, 0u);
  assert(io_result.status == BR_STATUS_OK);
  assert(br_byte_buffer_unread_byte(&buffer) == BR_STATUS_INVALID_STATE);

  br_byte_buffer_reset(&buffer);
  assert(br_byte_buffer_is_empty(&buffer));
  assert(br_byte_buffer_capacity(&buffer) >= 3u);

  br_byte_buffer_destroy(&buffer);
}

static void test_byte_buffer_compaction_and_copy_init(void) {
  br_byte_buffer buffer;
  u8 scratch[2];
  br_byte_buffer_io_result io_result;

  assert(br_byte_buffer_init_copy(&buffer, BR_BYTES_LIT("abcd"), br_allocator_heap()) ==
         BR_STATUS_OK);
  assert(br_bytes_equal(br_byte_buffer_view(&buffer), BR_BYTES_LIT("abcd")));

  io_result = br_byte_buffer_read(&buffer, scratch, 2u);
  assert(io_result.status == BR_STATUS_OK);
  assert(io_result.count == 2u);
  assert(br_bytes_equal(br_bytes_view_make(scratch, 2u), BR_BYTES_LIT("ab")));

  assert(br_byte_buffer_write(&buffer, BR_BYTES_LIT("efghijklmnopqrstuvwxyz")).status ==
         BR_STATUS_OK);
  assert(br_bytes_equal(br_byte_buffer_view(&buffer), BR_BYTES_LIT("cdefghijklmnopqrstuvwxyz")));

  br_byte_buffer_destroy(&buffer);
}

static void test_byte_buffer_next_can_unread(void) {
  br_byte_buffer buffer;
  br_bytes_view next;

  assert(br_byte_buffer_init_copy(&buffer, BR_BYTES_LIT("abc"), br_allocator_heap()) ==
         BR_STATUS_OK);

  next = br_byte_buffer_next(&buffer, 3u);
  assert(br_bytes_equal(next, BR_BYTES_LIT("abc")));
  assert(br_byte_buffer_unread_byte(&buffer) == BR_STATUS_OK);
  assert(br_bytes_equal(br_byte_buffer_view(&buffer), BR_BYTES_LIT("c")));

  next = br_byte_buffer_next(&buffer, 0u);
  assert(next.len == 0u);
  assert(br_byte_buffer_unread_byte(&buffer) == BR_STATUS_INVALID_STATE);

  br_byte_buffer_destroy(&buffer);
}

static void test_byte_buffer_self_append(void) {
  br_byte_buffer buffer;
  br_bytes_view original;
  br_bytes_view unread;
  u8 initial[64];
  u8 prefix[48];
  u8 consumed;
  u8 scratch[32];
  usize i;

  for (i = 0u; i < BR_ARRAY_COUNT(initial); ++i) {
    initial[i] = (u8)i;
  }
  memset(prefix, (int)'x', sizeof(prefix));

  br_byte_buffer_init(&buffer, br_allocator_heap());
  assert(br_byte_buffer_reserve(&buffer, 64u) == BR_STATUS_OK);
  assert(br_byte_buffer_write(&buffer, br_bytes_view_make(prefix, sizeof(prefix))).status ==
         BR_STATUS_OK);
  assert(br_byte_buffer_read(&buffer, scratch, sizeof(scratch)).status == BR_STATUS_OK);
  original = br_byte_buffer_view(&buffer);
  assert(br_byte_buffer_write(&buffer, original).status == BR_STATUS_OK);
  unread = br_byte_buffer_view(&buffer);
  assert(unread.len == 32u);
  assert(memcmp(unread.data, prefix + 32u, 16u) == 0);
  assert(memcmp(unread.data + 16u, prefix + 32u, 16u) == 0);
  br_byte_buffer_destroy(&buffer);

  assert(br_byte_buffer_init_copy(&buffer,
                                  br_bytes_view_make(initial, BR_ARRAY_COUNT(initial)),
                                  br_allocator_heap()) == BR_STATUS_OK);
  assert(br_byte_buffer_read(&buffer, &consumed, 1u).status == BR_STATUS_OK);

  original = br_byte_buffer_view(&buffer);
  assert(br_byte_buffer_write(&buffer, original).status == BR_STATUS_OK);

  unread = br_byte_buffer_view(&buffer);
  assert(unread.len == 126u);
  assert(memcmp(unread.data, initial + 1u, 63u) == 0);
  assert(memcmp(unread.data + 63u, initial + 1u, 63u) == 0);

  br_byte_buffer_destroy(&buffer);
}

int main(void) {
  test_byte_buffer_basic_write_read();
  test_byte_buffer_unread_and_truncate();
  test_byte_buffer_compaction_and_copy_init();
  test_byte_buffer_next_can_unread();
  test_byte_buffer_self_append();
  return 0;
}
