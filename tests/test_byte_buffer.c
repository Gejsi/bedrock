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
    assert(br_byte_buffer_unread_byte(&buffer) == BR_STATUS_OK);
    assert(br_bytes_equal(br_byte_buffer_view(&buffer), BR_BYTES_LIT("b")));

    br_byte_buffer_reset(&buffer);
    assert(br_byte_buffer_is_empty(&buffer));
    assert(br_byte_buffer_capacity(&buffer) >= 3u);

    br_byte_buffer_destroy(&buffer);
}

static void test_byte_buffer_compaction_and_copy_init(void) {
    br_byte_buffer buffer;
    u8 scratch[2];
    br_byte_buffer_io_result io_result;

    assert(br_byte_buffer_init_copy(&buffer, BR_BYTES_LIT("abcd"), br_allocator_heap()) == BR_STATUS_OK);
    assert(br_bytes_equal(br_byte_buffer_view(&buffer), BR_BYTES_LIT("abcd")));

    io_result = br_byte_buffer_read(&buffer, scratch, 2u);
    assert(io_result.status == BR_STATUS_OK);
    assert(io_result.count == 2u);
    assert(br_bytes_equal(br_bytes_view_make(scratch, 2u), BR_BYTES_LIT("ab")));

    assert(br_byte_buffer_write(&buffer, BR_BYTES_LIT("efghijklmnopqrstuvwxyz")).status == BR_STATUS_OK);
    assert(br_bytes_equal(br_byte_buffer_view(&buffer), BR_BYTES_LIT("cdefghijklmnopqrstuvwxyz")));

    br_byte_buffer_destroy(&buffer);
}

int main(void) {
    test_byte_buffer_basic_write_read();
    test_byte_buffer_unread_and_truncate();
    test_byte_buffer_compaction_and_copy_init();
    return 0;
}
