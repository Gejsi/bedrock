#include <assert.h>

#include <bedrock.h>

static void
assert_decode_eq(br_utf8_decode_result result, br_rune expected_value, usize expected_width) {
  assert(result.value == expected_value);
  assert(result.width == expected_width);
}

static void assert_encode_eq(br_utf8_encode_result result, const u8 *expected, usize expected_len) {
  assert(result.len == expected_len);
  assert(br_bytes_equal(br_bytes_view_make(result.bytes, result.len),
                        br_bytes_view_make(expected, expected_len)));
}

static void test_utf8_encode(void) {
  static const u8 cent[] = {0xc2u, 0xa2u};
  static const u8 euro[] = {0xe2u, 0x82u, 0xacu};
  static const u8 smile[] = {0xf0u, 0x9fu, 0x99u, 0x82u};
  static const u8 replacement[] = {0xefu, 0xbfu, 0xbdu};

  assert_encode_eq(br_utf8_encode((br_rune)'A'), (const u8 *)"A", 1u);
  assert_encode_eq(br_utf8_encode((br_rune)0x00a2), cent, BR_ARRAY_COUNT(cent));
  assert_encode_eq(br_utf8_encode((br_rune)0x20ac), euro, BR_ARRAY_COUNT(euro));
  assert_encode_eq(br_utf8_encode((br_rune)0x1f642), smile, BR_ARRAY_COUNT(smile));
  assert_encode_eq(br_utf8_encode((br_rune)0xd800), replacement, BR_ARRAY_COUNT(replacement));
  assert_encode_eq(br_utf8_encode((br_rune)0x110000), replacement, BR_ARRAY_COUNT(replacement));
}

static void test_utf8_decode(void) {
  static const u8 euro[] = {0xe2u, 0x82u, 0xacu};
  static const u8 truncated[] = {0xe2u, 0x82u};
  static const u8 invalid_cont[] = {0xe2u, 0x28u, 0xa1u};
  static const u8 overlong[] = {0xc0u, 0xafu};

  assert_decode_eq(br_utf8_decode(BR_BYTES_LIT("A")), (br_rune)'A', 1u);
  assert_decode_eq(
    br_utf8_decode(br_bytes_view_make(euro, BR_ARRAY_COUNT(euro))), (br_rune)0x20ac, 3u);
  assert_decode_eq(br_utf8_decode(br_bytes_view_make(NULL, 0u)), BR_RUNE_ERROR, 0u);
  assert_decode_eq(
    br_utf8_decode(br_bytes_view_make(truncated, BR_ARRAY_COUNT(truncated))), BR_RUNE_ERROR, 1u);
  assert_decode_eq(br_utf8_decode(br_bytes_view_make(invalid_cont, BR_ARRAY_COUNT(invalid_cont))),
                   BR_RUNE_ERROR,
                   1u);
  assert_decode_eq(
    br_utf8_decode(br_bytes_view_make(overlong, BR_ARRAY_COUNT(overlong))), BR_RUNE_ERROR, 1u);
}

static void test_utf8_decode_last(void) {
  static const u8 ascii_then_euro[] = {'A', 0xe2u, 0x82u, 0xacu};
  static const u8 truncated[] = {0xe2u, 0x82u};
  static const u8 ascii_then_cont[] = {'A', 0x80u};

  assert_decode_eq(
    br_utf8_decode_last(br_bytes_view_make(ascii_then_euro, BR_ARRAY_COUNT(ascii_then_euro))),
    (br_rune)0x20ac,
    3u);
  assert_decode_eq(br_utf8_decode_last(br_bytes_view_make(truncated, BR_ARRAY_COUNT(truncated))),
                   BR_RUNE_ERROR,
                   1u);
  assert_decode_eq(
    br_utf8_decode_last(br_bytes_view_make(ascii_then_cont, BR_ARRAY_COUNT(ascii_then_cont))),
    BR_RUNE_ERROR,
    1u);
  assert_decode_eq(br_utf8_decode_last(br_bytes_view_make(NULL, 0u)), BR_RUNE_ERROR, 0u);
}

static void test_utf8_validation_and_sizes(void) {
  static const u8 valid[] = {'A', 0xe2u, 0x82u, 0xacu, 0xf0u, 0x9fu, 0x99u, 0x82u};
  static const u8 invalid_surrogate[] = {0xedu, 0xa0u, 0x80u};
  static const u8 truncated[] = {0xe2u, 0x82u};
  static const u8 invalid_cont[] = {0xe2u, 0x28u, 0xa1u};

  assert(br_utf8_valid(br_bytes_view_make(valid, BR_ARRAY_COUNT(valid))));
  assert(!br_utf8_valid(br_bytes_view_make(invalid_surrogate, BR_ARRAY_COUNT(invalid_surrogate))));
  assert(!br_utf8_valid(br_bytes_view_make(truncated, BR_ARRAY_COUNT(truncated))));
  assert(!br_utf8_valid(br_bytes_view_make(invalid_cont, BR_ARRAY_COUNT(invalid_cont))));

  assert(br_utf8_valid_rune((br_rune)0x10ffff));
  assert(!br_utf8_valid_rune((br_rune)-1));
  assert(!br_utf8_valid_rune((br_rune)0xd800));
  assert(!br_utf8_valid_rune((br_rune)0x110000));

  assert(br_utf8_rune_size((br_rune)'A') == 1);
  assert(br_utf8_rune_size((br_rune)0x20ac) == 3);
  assert(br_utf8_rune_size((br_rune)0x1f642) == 4);
  assert(br_utf8_rune_size((br_rune)0xd800) == -1);
}

static void test_utf8_count_and_boundaries(void) {
  static const u8 valid[] = {'A', 0xe2u, 0x82u, 0xacu, 0xf0u, 0x9fu, 0x99u, 0x82u};
  static const u8 invalid[] = {'A', 0x80u, 'B'};
  static const u8 truncated[] = {0xe2u, 0x82u};
  static const u8 invalid_prefix[] = {0xe2u, 0x28u};

  assert(br_utf8_rune_count(br_bytes_view_make(valid, BR_ARRAY_COUNT(valid))) == 3u);
  assert(br_utf8_rune_count(br_bytes_view_make(invalid, BR_ARRAY_COUNT(invalid))) == 3u);
  assert(br_utf8_rune_count(br_bytes_view_make(truncated, BR_ARRAY_COUNT(truncated))) == 2u);

  assert(br_utf8_rune_start((u8)'A'));
  assert(br_utf8_rune_start((u8)0xe2u));
  assert(!br_utf8_rune_start((u8)0x82u));

  assert(!br_utf8_full_rune(br_bytes_view_make(NULL, 0u)));
  assert(br_utf8_full_rune(BR_BYTES_LIT("A")));
  assert(!br_utf8_full_rune(br_bytes_view_make(truncated, BR_ARRAY_COUNT(truncated))));
  assert(br_utf8_full_rune(br_bytes_view_make(invalid_prefix, BR_ARRAY_COUNT(invalid_prefix))));
}

/* Assert a byte sequence decodes as a single width-1 replacement rune and is
   rejected by the whole-buffer validator. This is the invariant Go's
   TestDecodeInvalidSequence enforces for every malformed lead or continuation. */
static void assert_invalid_decode(const u8 *bytes, usize len) {
  assert_decode_eq(br_utf8_decode(br_bytes_view_make(bytes, len)), BR_RUNE_ERROR, 1u);
  assert(!br_utf8_valid(br_bytes_view_make(bytes, len)));
}

/* Assert value encodes to exactly `expected` and that sequence decodes back to
   value with the same width -- a boundary round-trip. */
static void assert_roundtrip(br_rune value, const u8 *expected, usize expected_len) {
  br_utf8_decode_result dec;

  assert_encode_eq(br_utf8_encode(value), expected, expected_len);
  dec = br_utf8_decode(br_bytes_view_make(expected, expected_len));
  assert(dec.value == value);
  assert(dec.width == expected_len);
}

static void test_utf8_overlong_rejected(void) {
  /* Overlong encodings: a code point packed into more bytes than canonical.
     The lead-byte lo bounds (0xa0 for E0, 0x90 for F0) reject these. */
  static const u8 overlong_2in3[] = {0xe0u, 0x80u, 0xafu};        /* '/' as 3 bytes */
  static const u8 overlong_1in4[] = {0xf0u, 0x80u, 0x80u, 0x80u}; /* NUL as 4 bytes */
  static const u8 overlong_ascii[] = {0xc1u, 0x80u};              /* lead < 0xc2 */
  static const u8 overlong_3in4[] = {0xf0u, 0x8fu, 0xbfu, 0xbfu}; /* U+FFFF as 4 bytes */

  assert_invalid_decode(overlong_2in3, BR_ARRAY_COUNT(overlong_2in3));
  assert_invalid_decode(overlong_1in4, BR_ARRAY_COUNT(overlong_1in4));
  assert_invalid_decode(overlong_ascii, BR_ARRAY_COUNT(overlong_ascii));
  assert_invalid_decode(overlong_3in4, BR_ARRAY_COUNT(overlong_3in4));
}

static void test_utf8_out_of_range_and_bad_leads(void) {
  /* U+110000 (F4 90 80 80): one past U+10FFFF, the most security-relevant
     over-range case. Rejected because the F4 lead caps its first continuation
     at 0x8f. */
  static const u8 above_max[] = {0xf4u, 0x90u, 0x80u, 0x80u};
  /* F5..FF can never be a valid lead (they would encode > U+10FFFF). */
  static const u8 lead_f5[] = {0xf5u, 0x80u, 0x80u, 0x80u};
  static const u8 lead_f8[] = {0xf8u};
  static const u8 lead_ff[] = {0xffu};
  /* Lone continuation bytes are not valid leads. */
  static const u8 lone_cont_lo[] = {0x80u};
  static const u8 lone_cont_hi[] = {0xbfu};

  assert_invalid_decode(above_max, BR_ARRAY_COUNT(above_max));
  assert_invalid_decode(lead_f5, BR_ARRAY_COUNT(lead_f5));
  assert_invalid_decode(lead_f8, BR_ARRAY_COUNT(lead_f8));
  assert_invalid_decode(lead_ff, BR_ARRAY_COUNT(lead_ff));
  assert_invalid_decode(lone_cont_lo, BR_ARRAY_COUNT(lone_cont_lo));
  assert_invalid_decode(lone_cont_hi, BR_ARRAY_COUNT(lone_cont_hi));
}

static void test_utf8_surrogates_rejected(void) {
  /* UTF-16 surrogate halves are not valid scalar values; the ED lead caps its
     first continuation at 0x9f so D800..DFFF cannot be decoded. */
  static const u8 surrogate_min[] = {0xedu, 0xa0u, 0x80u}; /* U+D800 */
  static const u8 surrogate_max[] = {0xedu, 0xbfu, 0xbfu}; /* U+DFFF */

  assert_invalid_decode(surrogate_min, BR_ARRAY_COUNT(surrogate_min));
  assert_invalid_decode(surrogate_max, BR_ARRAY_COUNT(surrogate_max));

  /* And the encoder refuses to produce them, emitting the replacement rune. */
  assert(!br_utf8_valid_rune((br_rune)0xd800));
  assert(!br_utf8_valid_rune((br_rune)0xdfff));
}

static void test_utf8_boundary_roundtrips(void) {
  /* The code points straddling each encoding-width boundary, from Go's
     utf8map. Each must encode to the canonical bytes and decode back. */
  static const u8 b_007f[] = {0x7fu};
  static const u8 b_0080[] = {0xc2u, 0x80u};
  static const u8 b_07ff[] = {0xdfu, 0xbfu};
  static const u8 b_0800[] = {0xe0u, 0xa0u, 0x80u};
  static const u8 b_d7ff[] = {0xedu, 0x9fu, 0xbfu}; /* last before surrogates */
  static const u8 b_e000[] = {0xeeu, 0x80u, 0x80u}; /* first after surrogates */
  static const u8 b_ffff[] = {0xefu, 0xbfu, 0xbfu};
  static const u8 b_10000[] = {0xf0u, 0x90u, 0x80u, 0x80u};
  static const u8 b_10ffff[] = {0xf4u, 0x8fu, 0xbfu, 0xbfu}; /* max scalar value */

  assert_roundtrip((br_rune)0x007f, b_007f, BR_ARRAY_COUNT(b_007f));
  assert_roundtrip((br_rune)0x0080, b_0080, BR_ARRAY_COUNT(b_0080));
  assert_roundtrip((br_rune)0x07ff, b_07ff, BR_ARRAY_COUNT(b_07ff));
  assert_roundtrip((br_rune)0x0800, b_0800, BR_ARRAY_COUNT(b_0800));
  assert_roundtrip((br_rune)0xd7ff, b_d7ff, BR_ARRAY_COUNT(b_d7ff));
  assert_roundtrip((br_rune)0xe000, b_e000, BR_ARRAY_COUNT(b_e000));
  assert_roundtrip((br_rune)0xffff, b_ffff, BR_ARRAY_COUNT(b_ffff));
  assert_roundtrip((br_rune)0x10000, b_10000, BR_ARRAY_COUNT(b_10000));
  assert_roundtrip((br_rune)0x10ffff, b_10ffff, BR_ARRAY_COUNT(b_10ffff));
}

static void test_utf8_negative_rune_encode(void) {
  static const u8 replacement[] = {0xefu, 0xbfu, 0xbdu};

  /* A negative rune is not a valid scalar value; encode emits U+FFFD. */
  assert_encode_eq(br_utf8_encode((br_rune)-1), replacement, BR_ARRAY_COUNT(replacement));
  assert(!br_utf8_valid_rune((br_rune)-1));
  assert(br_utf8_rune_size((br_rune)-1) == -1);
}

static void test_utf8_mixed_valid_error(void) {
  /* euro (valid 3-byte) + lone continuation (error) + ASCII 'X'. rune_count
     treats the bad byte as one width-1 error rune, so it counts 3; the
     validator rejects the whole buffer. */
  static const u8 mixed[] = {0xe2u, 0x82u, 0xacu, 0x80u, 'X'};

  assert(br_utf8_rune_count(br_bytes_view_make(mixed, BR_ARRAY_COUNT(mixed))) == 3u);
  assert(!br_utf8_valid(br_bytes_view_make(mixed, BR_ARRAY_COUNT(mixed))));
}

static void test_utf8_decode_last_incomplete(void) {
  /* A complete rune followed by a truncated multi-byte sequence: decode_last
     must report the trailing fragment as a width-1 error, not walk back into
     the valid rune before it. */
  static const u8 ascii_then_truncated[] = {'A', 0xe2u, 0x82u};
  /* A lone trailing continuation byte. */
  static const u8 ascii_then_cont[] = {'A', 0x80u};

  assert_decode_eq(br_utf8_decode_last(br_bytes_view_make(ascii_then_truncated,
                                                          BR_ARRAY_COUNT(ascii_then_truncated))),
                   BR_RUNE_ERROR,
                   1u);
  assert_decode_eq(
    br_utf8_decode_last(br_bytes_view_make(ascii_then_cont, BR_ARRAY_COUNT(ascii_then_cont))),
    BR_RUNE_ERROR,
    1u);
}

int main(void) {
  test_utf8_encode();
  test_utf8_decode();
  test_utf8_decode_last();
  test_utf8_validation_and_sizes();
  test_utf8_count_and_boundaries();
  test_utf8_overlong_rejected();
  test_utf8_out_of_range_and_bad_leads();
  test_utf8_surrogates_rejected();
  test_utf8_boundary_roundtrips();
  test_utf8_negative_rune_encode();
  test_utf8_mixed_valid_error();
  test_utf8_decode_last_incomplete();
  return 0;
}
