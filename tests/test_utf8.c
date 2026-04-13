#include <assert.h>

#include <bedrock.h>

static void assert_decode_eq(br_utf8_decode_result result, br_rune expected_value, usize expected_width) {
    assert(result.value == expected_value);
    assert(result.width == expected_width);
}

static void assert_encode_eq(br_utf8_encode_result result, const u8 *expected, usize expected_len) {
    assert(result.len == expected_len);
    assert(br_bytes_equal(br_bytes_view_make(result.bytes, result.len), br_bytes_view_make(expected, expected_len)));
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
        br_utf8_decode(br_bytes_view_make(euro, BR_ARRAY_COUNT(euro))),
        (br_rune)0x20ac,
        3u
    );
    assert_decode_eq(br_utf8_decode(br_bytes_view_make(NULL, 0u)), BR_RUNE_ERROR, 0u);
    assert_decode_eq(br_utf8_decode(br_bytes_view_make(truncated, BR_ARRAY_COUNT(truncated))), BR_RUNE_ERROR, 1u);
    assert_decode_eq(br_utf8_decode(br_bytes_view_make(invalid_cont, BR_ARRAY_COUNT(invalid_cont))), BR_RUNE_ERROR, 1u);
    assert_decode_eq(br_utf8_decode(br_bytes_view_make(overlong, BR_ARRAY_COUNT(overlong))), BR_RUNE_ERROR, 1u);
}

static void test_utf8_decode_last(void) {
    static const u8 ascii_then_euro[] = {'A', 0xe2u, 0x82u, 0xacu};
    static const u8 truncated[] = {0xe2u, 0x82u};
    static const u8 ascii_then_cont[] = {'A', 0x80u};

    assert_decode_eq(
        br_utf8_decode_last(br_bytes_view_make(ascii_then_euro, BR_ARRAY_COUNT(ascii_then_euro))),
        (br_rune)0x20ac,
        3u
    );
    assert_decode_eq(br_utf8_decode_last(br_bytes_view_make(truncated, BR_ARRAY_COUNT(truncated))), BR_RUNE_ERROR, 1u);
    assert_decode_eq(
        br_utf8_decode_last(br_bytes_view_make(ascii_then_cont, BR_ARRAY_COUNT(ascii_then_cont))),
        BR_RUNE_ERROR,
        1u
    );
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

int main(void) {
    test_utf8_encode();
    test_utf8_decode();
    test_utf8_decode_last();
    test_utf8_validation_and_sizes();
    test_utf8_count_and_boundaries();
    return 0;
}
