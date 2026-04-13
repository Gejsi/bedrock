#include <assert.h>

#include <bedrock.h>

static void test_string_reader_basic_read(void) {
    br_string_reader reader;
    br_string_reader_io_result io_result;
    br_string_reader_byte_result byte_result;
    u8 scratch[8];

    br_string_reader_init(&reader, BR_STR_LIT("abcdef"));
    assert(br_string_reader_len(&reader) == 6u);
    assert(br_string_reader_size(&reader) == 6u);
    assert(br_string_equal(br_string_reader_view(&reader), BR_STR_LIT("abcdef")));

    io_result = br_string_reader_read(&reader, scratch, 2u);
    assert(io_result.status == BR_STATUS_OK);
    assert(io_result.count == 2u);
    assert(br_string_equal(br_string_view_make(scratch, 2u), BR_STR_LIT("ab")));
    assert(br_string_equal(br_string_reader_view(&reader), BR_STR_LIT("cdef")));

    byte_result = br_string_reader_read_byte(&reader);
    assert(byte_result.status == BR_STATUS_OK);
    assert(byte_result.value == (u8)'c');
    assert(br_string_equal(br_string_reader_view(&reader), BR_STR_LIT("def")));

    assert(br_string_reader_unread_byte(&reader) == BR_STATUS_OK);
    assert(br_string_equal(br_string_reader_view(&reader), BR_STR_LIT("cdef")));

    br_string_reader_reset(&reader);
    assert(br_string_equal(br_string_reader_view(&reader), BR_STR_LIT("abcdef")));
}

static void test_string_reader_read_at_and_seek(void) {
    br_string_reader reader;
    br_string_reader_io_result io_result;
    br_string_reader_seek_result seek_result;
    u8 scratch[8];

    br_string_reader_init(&reader, BR_STR_LIT("abcd"));

    io_result = br_string_reader_read_at(&reader, scratch, 2u, 1);
    assert(io_result.status == BR_STATUS_OK);
    assert(io_result.count == 2u);
    assert(br_string_equal(br_string_view_make(scratch, 2u), BR_STR_LIT("bc")));
    assert(br_string_equal(br_string_reader_view(&reader), BR_STR_LIT("abcd")));

    io_result = br_string_reader_read_at(&reader, scratch, 4u, 2);
    assert(io_result.status == BR_STATUS_EOF);
    assert(io_result.count == 2u);
    assert(br_string_equal(br_string_view_make(scratch, 2u), BR_STR_LIT("cd")));

    seek_result = br_string_reader_seek(&reader, 3, BR_SEEK_FROM_START);
    assert(seek_result.status == BR_STATUS_OK);
    assert(seek_result.offset == 3);
    assert(br_string_equal(br_string_reader_view(&reader), BR_STR_LIT("d")));

    seek_result = br_string_reader_seek(&reader, 3, BR_SEEK_FROM_END);
    assert(seek_result.status == BR_STATUS_OK);
    assert(seek_result.offset == 7);
    assert(br_string_reader_len(&reader) == 0u);

    io_result = br_string_reader_read(&reader, scratch, 1u);
    assert(io_result.status == BR_STATUS_EOF);
    assert(io_result.count == 0u);

    seek_result = br_string_reader_seek(&reader, -10, BR_SEEK_FROM_CURRENT);
    assert(seek_result.status == BR_STATUS_INVALID_ARGUMENT);
    seek_result = br_string_reader_seek(&reader, 0, (br_seek_from)99);
    assert(seek_result.status == BR_STATUS_INVALID_ARGUMENT);
}

static void test_string_reader_rune_semantics(void) {
    static const u8 utf8_data[] = {'a', 0xc3u, 0xa4u, 'b'};
    br_string_reader reader;
    br_string_reader_rune_result rune_result;
    br_string_reader_byte_result byte_result;

    br_string_reader_init(&reader, br_string_view_make(utf8_data, BR_ARRAY_COUNT(utf8_data)));

    rune_result = br_string_reader_read_rune(&reader);
    assert(rune_result.status == BR_STATUS_OK);
    assert(rune_result.value == (br_rune)'a');
    assert(rune_result.width == 1u);

    rune_result = br_string_reader_read_rune(&reader);
    assert(rune_result.status == BR_STATUS_OK);
    assert(rune_result.value == (br_rune)0x00e4);
    assert(rune_result.width == 2u);
    assert(br_string_reader_unread_rune(&reader) == BR_STATUS_OK);

    rune_result = br_string_reader_read_rune(&reader);
    assert(rune_result.status == BR_STATUS_OK);
    assert(rune_result.value == (br_rune)0x00e4);
    assert(rune_result.width == 2u);

    byte_result = br_string_reader_read_byte(&reader);
    assert(byte_result.status == BR_STATUS_OK);
    assert(byte_result.value == (u8)'b');
    assert(br_string_reader_unread_rune(&reader) == BR_STATUS_INVALID_STATE);

    rune_result = br_string_reader_read_rune(&reader);
    assert(rune_result.status == BR_STATUS_EOF);
    assert(rune_result.width == 0u);
}

static void test_string_reader_invalid_utf8_rune(void) {
    static const u8 invalid[] = {'a', 0x80u, 'b'};
    br_string_reader reader;
    br_string_reader_rune_result rune_result;

    br_string_reader_init(&reader, br_string_view_make(invalid, BR_ARRAY_COUNT(invalid)));

    rune_result = br_string_reader_read_rune(&reader);
    assert(rune_result.status == BR_STATUS_OK);
    assert(rune_result.value == (br_rune)'a');
    assert(rune_result.width == 1u);

    rune_result = br_string_reader_read_rune(&reader);
    assert(rune_result.status == BR_STATUS_OK);
    assert(rune_result.value == BR_RUNE_ERROR);
    assert(rune_result.width == 1u);
    assert(br_string_reader_unread_rune(&reader) == BR_STATUS_OK);

    rune_result = br_string_reader_read_rune(&reader);
    assert(rune_result.status == BR_STATUS_OK);
    assert(rune_result.value == BR_RUNE_ERROR);
    assert(rune_result.width == 1u);

    rune_result = br_string_reader_read_rune(&reader);
    assert(rune_result.status == BR_STATUS_OK);
    assert(rune_result.value == (br_rune)'b');
    assert(rune_result.width == 1u);
}

int main(void) {
    test_string_reader_basic_read();
    test_string_reader_read_at_and_seek();
    test_string_reader_rune_semantics();
    test_string_reader_invalid_utf8_rune();
    return 0;
}
