#include <assert.h>

#include <bedrock.h>
#include <bedrock/encoding.h>

static void test_hex_encode_roundtrip(void) {
  static const u8 raw[] = {0x00u, 0x0fu, 0x10u, 0x7fu, 0x80u, 0xabu, 0xffu};
  br_bytes_view src = br_bytes_view_make(raw, BR_ARRAY_COUNT(raw));
  br_bytes_result lower;
  br_bytes_result upper;
  br_decode_result decoded;

  lower = br_hex_encode(src, BR_HEX_LOWER, br_allocator_heap());
  assert(lower.status == BR_STATUS_OK);
  assert(lower.value.len == br_hex_encoded_len(src.len));
  assert(br_bytes_equal(br_bytes_view_from_bytes(lower.value), BR_BYTES_LIT("000f107f80abff")));

  upper = br_hex_encode(src, BR_HEX_UPPER, br_allocator_heap());
  assert(upper.status == BR_STATUS_OK);
  assert(br_bytes_equal(br_bytes_view_from_bytes(upper.value), BR_BYTES_LIT("000F107F80ABFF")));

  /* Round-trip: decode the lowercase form back to the original bytes. */
  decoded = br_hex_decode(br_bytes_view_from_bytes(lower.value), br_allocator_heap());
  assert(decoded.status == BR_STATUS_OK);
  assert(decoded.error_offset == 0u);
  assert(br_bytes_equal(br_bytes_view_from_bytes(decoded.value), src));

  assert(br_bytes_free(lower.value, br_allocator_heap()) == BR_STATUS_OK);
  assert(br_bytes_free(upper.value, br_allocator_heap()) == BR_STATUS_OK);
  assert(br_bytes_free(decoded.value, br_allocator_heap()) == BR_STATUS_OK);
}

static void test_hex_encode_empty(void) {
  br_bytes_view empty = br_bytes_view_make(NULL, 0u);
  br_bytes_result encoded;

  encoded = br_hex_encode(empty, BR_HEX_LOWER, br_allocator_heap());
  assert(encoded.status == BR_STATUS_OK);
  assert(encoded.value.len == 0u);
  assert(encoded.value.data == NULL);
  assert(br_bytes_free(encoded.value, br_allocator_heap()) == BR_STATUS_OK);
}

static void test_hex_encode_oom(void) {
  br_bytes_result failed;

  failed = br_hex_encode(BR_BYTES_LIT("abc"), BR_HEX_LOWER, br_allocator_fail());
  assert(failed.status == BR_STATUS_OUT_OF_MEMORY);
  assert(failed.value.data == NULL);
  assert(failed.value.len == 0u);
}

static void test_hex_encode_into(void) {
  static const u8 raw[] = {0xdeu, 0xadu, 0xbeu, 0xefu};
  br_bytes_view src = br_bytes_view_make(raw, BR_ARRAY_COUNT(raw));
  u8 dst[8];
  u8 upper_dst[8];
  br_io_result exact;
  br_io_result upper;
  br_io_result short_buf;
  br_io_result empty;

  exact = br_hex_encode_into(src, BR_HEX_LOWER, dst, sizeof(dst));
  assert(exact.status == BR_STATUS_OK);
  assert(exact.count == 8u);
  assert(br_bytes_equal(br_bytes_view_make(dst, exact.count), BR_BYTES_LIT("deadbeef")));

  upper = br_hex_encode_into(src, BR_HEX_UPPER, upper_dst, sizeof(upper_dst));
  assert(upper.status == BR_STATUS_OK);
  assert(br_bytes_equal(br_bytes_view_make(upper_dst, upper.count), BR_BYTES_LIT("DEADBEEF")));

  /* One byte short of the required 8. */
  short_buf = br_hex_encode_into(src, BR_HEX_LOWER, dst, 7u);
  assert(short_buf.status == BR_STATUS_SHORT_BUFFER);
  assert(short_buf.count == 0u);

  /* Empty input needs no capacity and may pass a NULL buffer. */
  empty = br_hex_encode_into(br_bytes_view_make(NULL, 0u), BR_HEX_LOWER, NULL, 0u);
  assert(empty.status == BR_STATUS_OK);
  assert(empty.count == 0u);
}

static void test_hex_encode_to_writer(void) {
  static u8 raw[300];
  br_bytes_view src;
  br_byte_buffer sink;
  br_io_result written;
  br_bytes_result expected;

  /* 300 bytes forces more than one flush of the 512-byte (256-src) batch,
     exercising the boundary in br_hex_encode_to_writer. */
  for (usize i = 0u; i < sizeof(raw); i += 1u) {
    raw[i] = (u8)(i & 0xffu);
  }
  src = br_bytes_view_make(raw, sizeof(raw));

  br_byte_buffer_init(&sink, br_allocator_heap());
  written = br_hex_encode_to_writer(src, BR_HEX_LOWER, br_byte_buffer_as_writer(&sink));
  assert(written.status == BR_STATUS_OK);
  assert(written.count == br_hex_encoded_len(src.len));
  assert(br_byte_buffer_len(&sink) == br_hex_encoded_len(src.len));

  /* The streamed output must match the allocating encoder byte-for-byte. */
  expected = br_hex_encode(src, BR_HEX_LOWER, br_allocator_heap());
  assert(expected.status == BR_STATUS_OK);
  assert(br_bytes_equal(br_byte_buffer_view(&sink), br_bytes_view_from_bytes(expected.value)));

  assert(br_bytes_free(expected.value, br_allocator_heap()) == BR_STATUS_OK);
  br_byte_buffer_destroy(&sink);
}

static void test_hex_decode_basic(void) {
  br_decode_result lower;
  br_decode_result upper;
  br_decode_result mixed;
  br_decode_result empty;

  lower = br_hex_decode(BR_BYTES_LIT("48656c6c6f"), br_allocator_heap());
  assert(lower.status == BR_STATUS_OK);
  assert(br_bytes_equal(br_bytes_view_from_bytes(lower.value), BR_BYTES_LIT("Hello")));
  assert(br_bytes_free(lower.value, br_allocator_heap()) == BR_STATUS_OK);

  /* Decoding is case-insensitive. */
  upper = br_hex_decode(BR_BYTES_LIT("48656C6C6F"), br_allocator_heap());
  assert(upper.status == BR_STATUS_OK);
  assert(br_bytes_equal(br_bytes_view_from_bytes(upper.value), BR_BYTES_LIT("Hello")));
  assert(br_bytes_free(upper.value, br_allocator_heap()) == BR_STATUS_OK);

  mixed = br_hex_decode(BR_BYTES_LIT("dEaDbEeF"), br_allocator_heap());
  assert(mixed.status == BR_STATUS_OK);
  assert(mixed.value.len == 4u);
  assert(mixed.value.data[0] == 0xdeu && mixed.value.data[3] == 0xefu);
  assert(br_bytes_free(mixed.value, br_allocator_heap()) == BR_STATUS_OK);

  empty = br_hex_decode(br_bytes_view_make(NULL, 0u), br_allocator_heap());
  assert(empty.status == BR_STATUS_OK);
  assert(empty.value.len == 0u);
  assert(empty.value.data == NULL);
  assert(empty.error_offset == 0u);
  assert(br_bytes_free(empty.value, br_allocator_heap()) == BR_STATUS_OK);
}

static void test_hex_decode_odd_length(void) {
  br_decode_result odd;

  /* Odd length is a structural violation: INVALID_ENCODING at len - 1. */
  odd = br_hex_decode(BR_BYTES_LIT("abc"), br_allocator_heap());
  assert(odd.status == BR_STATUS_INVALID_ENCODING);
  assert(odd.error_offset == 2u);
  assert(odd.value.data == NULL);
  assert(odd.value.len == 0u);

  /* Single byte: len - 1 == 0. */
  odd = br_hex_decode(BR_BYTES_LIT("a"), br_allocator_heap());
  assert(odd.status == BR_STATUS_INVALID_ENCODING);
  assert(odd.error_offset == 0u);
  assert(odd.value.data == NULL);
}

static void test_hex_decode_bad_char(void) {
  br_decode_result leading;
  br_decode_result middle;
  br_decode_result second_nibble;

  /* Bad byte in the very first position. */
  leading = br_hex_decode(BR_BYTES_LIT("zz00"), br_allocator_heap());
  assert(leading.status == BR_STATUS_INVALID_ENCODING);
  assert(leading.error_offset == 0u);
  assert(leading.value.data == NULL);

  /* Bad byte partway in, on an even-length input (allocates, then fails). */
  middle = br_hex_decode(BR_BYTES_LIT("00zz"), br_allocator_heap());
  assert(middle.status == BR_STATUS_INVALID_ENCODING);
  assert(middle.error_offset == 2u);
  assert(middle.value.data == NULL);

  /* Bad byte in the low nibble reports that exact index. */
  second_nibble = br_hex_decode(BR_BYTES_LIT("0g"), br_allocator_heap());
  assert(second_nibble.status == BR_STATUS_INVALID_ENCODING);
  assert(second_nibble.error_offset == 1u);
  assert(second_nibble.value.data == NULL);
}

static void test_hex_decode_into(void) {
  u8 dst[4];
  br_decode_into_result ok;
  br_decode_into_result short_buf;
  br_decode_into_result odd;
  br_decode_into_result bad;
  br_decode_into_result null_dst;

  ok = br_hex_decode_into(BR_BYTES_LIT("deadbeef"), dst, sizeof(dst));
  assert(ok.status == BR_STATUS_OK);
  assert(ok.count == 4u);
  assert(ok.error_offset == 0u);
  assert(dst[0] == 0xdeu && dst[1] == 0xadu && dst[2] == 0xbeu && dst[3] == 0xefu);

  /* Needs 4 bytes, only 3 available. */
  short_buf = br_hex_decode_into(BR_BYTES_LIT("deadbeef"), dst, 3u);
  assert(short_buf.status == BR_STATUS_SHORT_BUFFER);
  assert(short_buf.count == 0u);

  /* Parity is checked first: odd length classifies as INVALID_ENCODING even
     when the buffer would also be too small. */
  odd = br_hex_decode_into(BR_BYTES_LIT("abc"), dst, 0u);
  assert(odd.status == BR_STATUS_INVALID_ENCODING);
  assert(odd.error_offset == 2u);
  assert(odd.count == 0u);

  /* Bad byte: count 0, offset locates the fault. */
  bad = br_hex_decode_into(BR_BYTES_LIT("00zz"), dst, sizeof(dst));
  assert(bad.status == BR_STATUS_INVALID_ENCODING);
  assert(bad.error_offset == 2u);
  assert(bad.count == 0u);

  /* NULL destination with required output is caller misuse. */
  null_dst = br_hex_decode_into(BR_BYTES_LIT("00"), NULL, 4u);
  assert(null_dst.status == BR_STATUS_INVALID_ARGUMENT);
  assert(null_dst.count == 0u);
}

static void test_hex_decode_sequence(void) {
  br_io_byte_result plain;
  br_io_byte_result prefixed_lower;
  br_io_byte_result prefixed_upper;
  br_io_byte_result max_value;
  br_io_byte_result too_long;
  br_io_byte_result too_short;
  br_io_byte_result bad_char;
  br_io_byte_result prefix_only;

  plain = br_hex_decode_sequence(BR_BYTES_LIT("23"));
  assert(plain.status == BR_STATUS_OK);
  assert(plain.value == 0x23u);

  prefixed_lower = br_hex_decode_sequence(BR_BYTES_LIT("0x23"));
  assert(prefixed_lower.status == BR_STATUS_OK);
  assert(prefixed_lower.value == 0x23u);

  prefixed_upper = br_hex_decode_sequence(BR_BYTES_LIT("0X23"));
  assert(prefixed_upper.status == BR_STATUS_OK);
  assert(prefixed_upper.value == 0x23u);

  max_value = br_hex_decode_sequence(BR_BYTES_LIT("ff"));
  assert(max_value.status == BR_STATUS_OK);
  assert(max_value.value == 0xffu);

  /* More than two characters after the prefix strip. */
  too_long = br_hex_decode_sequence(BR_BYTES_LIT("0x234"));
  assert(too_long.status == BR_STATUS_INVALID_ENCODING);

  /* One character is not a valid fixed-width byte. */
  too_short = br_hex_decode_sequence(BR_BYTES_LIT("2"));
  assert(too_short.status == BR_STATUS_INVALID_ENCODING);

  bad_char = br_hex_decode_sequence(BR_BYTES_LIT("2g"));
  assert(bad_char.status == BR_STATUS_INVALID_ENCODING);

  /* The prefix alone leaves zero characters. */
  prefix_only = br_hex_decode_sequence(BR_BYTES_LIT("0x"));
  assert(prefix_only.status == BR_STATUS_INVALID_ENCODING);
}

/*
Platform-independent proof of the leak fix: a failing decode on even-length
input allocates the destination buffer, then hits a bad byte. Running it against
the tracking allocator and asserting zero live allocations afterward proves the
buffer was freed on the error path, without depending on LSan (unavailable under
Apple clang).
*/
static void test_hex_decode_frees_on_error(void) {
  br_tracking_allocator tracking;
  br_allocator allocator;
  br_decode_result failed;

  br_tracking_allocator_init(&tracking, br_allocator_heap(), br_allocator_heap());
  allocator = br_tracking_allocator_allocator(&tracking);

  /* Even length so the decode allocates before it reaches the invalid 'z'. */
  failed = br_hex_decode(BR_BYTES_LIT("00zz"), allocator);
  assert(failed.status == BR_STATUS_INVALID_ENCODING);
  assert(failed.error_offset == 2u);
  assert(failed.value.data == NULL);
  assert(failed.value.len == 0u);

  /* The scratch buffer was allocated, then freed on the failure path. */
  assert(tracking.stats.total_allocation_count == 1u);
  assert(tracking.stats.total_free_count == 1u);
  assert(tracking.stats.live_allocation_count == 0u);
  assert(tracking.stats.current_memory_allocated == 0u);
  assert(tracking.bad_free_count == 0u);

  br_tracking_allocator_destroy(&tracking);
}

int main(void) {
  test_hex_encode_roundtrip();
  test_hex_encode_empty();
  test_hex_encode_oom();
  test_hex_encode_into();
  test_hex_encode_to_writer();
  test_hex_decode_basic();
  test_hex_decode_odd_length();
  test_hex_decode_bad_char();
  test_hex_decode_into();
  test_hex_decode_sequence();
  test_hex_decode_frees_on_error();
  return 0;
}
