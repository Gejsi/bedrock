#include <assert.h>

#include <bedrock.h>

static void test_string_builder_heap_backing(void) {
    br_string_builder builder;
    br_string_builder_io_result io_result;
    br_string_builder_rune_result rune_result;
    br_string_builder_byte_result byte_result;
    br_string_result clone;

    br_string_builder_init(&builder, br_allocator_heap());

    assert(br_string_builder_is_empty(&builder));
    assert(br_string_builder_write(&builder, BR_STR_LIT("hello")).status == BR_STATUS_OK);
    assert(br_string_builder_write_byte(&builder, (u8)' ') == BR_STATUS_OK);
    io_result = br_string_builder_write_rune(&builder, (br_rune)0x00e4);
    assert(io_result.status == BR_STATUS_OK);
    assert(io_result.count == 2u);
    assert(br_string_builder_len(&builder) == 8u);
    assert(br_string_equal(br_string_builder_view(&builder),
                           br_string_view_make("hello \xc3\xa4", 8u)));

    rune_result = br_string_builder_pop_rune(&builder);
    assert(rune_result.status == BR_STATUS_OK);
    assert(rune_result.value == (br_rune)0x00e4);
    assert(rune_result.width == 2u);
    assert(br_string_equal(br_string_builder_view(&builder), BR_STR_LIT("hello ")));

    byte_result = br_string_builder_pop_byte(&builder);
    assert(byte_result.status == BR_STATUS_OK);
    assert(byte_result.value == (u8)' ');
    assert(br_string_equal(br_string_builder_view(&builder), BR_STR_LIT("hello")));

    clone = br_string_builder_clone(&builder, br_allocator_heap());
    assert(clone.status == BR_STATUS_OK);
    assert(br_string_equal(br_string_view_from_string(clone.value), BR_STR_LIT("hello")));
    assert(br_string_free(clone.value, br_allocator_heap()) == BR_STATUS_OK);

    assert(br_string_builder_truncate(&builder, 2u) == BR_STATUS_OK);
    assert(br_string_equal(br_string_builder_view(&builder), BR_STR_LIT("he")));
    br_string_builder_reset(&builder);
    assert(br_string_builder_is_empty(&builder));

    br_string_builder_destroy(&builder);
}

static void test_string_builder_fixed_backing(void) {
    char storage[8];
    br_string_builder builder;
    br_string_builder_io_result io_result;

    br_string_builder_init_with_backing(&builder, storage, sizeof(storage));

    assert(br_string_builder_capacity(&builder) == sizeof(storage));
    assert(br_string_builder_write(&builder, BR_STR_LIT("abcd")).status == BR_STATUS_OK);
    assert(br_string_builder_space(&builder) == 4u);

    io_result = br_string_builder_write(&builder, BR_STR_LIT("efgh"));
    assert(io_result.status == BR_STATUS_OK);
    assert(io_result.count == 4u);
    assert(br_string_equal(br_string_builder_view(&builder), BR_STR_LIT("abcdefgh")));

    io_result = br_string_builder_write(&builder, BR_STR_LIT("i"));
    assert(io_result.status == BR_STATUS_OUT_OF_MEMORY);
    assert(io_result.count == 0u);
    assert(br_string_equal(br_string_builder_view(&builder), BR_STR_LIT("abcdefgh")));

    br_string_builder_destroy(&builder);
}

static void test_string_builder_init_with_capacity_and_invalid_pop(void) {
    br_string_builder builder;
    br_string_builder_byte_result byte_result;
    br_string_builder_rune_result rune_result;

    assert(br_string_builder_init_with_capacity(&builder, 32u, br_allocator_heap()) ==
           BR_STATUS_OK);
    assert(br_string_builder_capacity(&builder) >= 32u);

    byte_result = br_string_builder_pop_byte(&builder);
    assert(byte_result.status == BR_STATUS_INVALID_STATE);

    assert(br_string_builder_write(&builder, br_string_view_make("a\x80", 2u)).status ==
           BR_STATUS_OK);
    rune_result = br_string_builder_pop_rune(&builder);
    assert(rune_result.status == BR_STATUS_OK);
    assert(rune_result.value == BR_RUNE_ERROR);
    assert(rune_result.width == 1u);
    assert(br_string_equal(br_string_builder_view(&builder), BR_STR_LIT("a")));

    br_string_builder_destroy(&builder);
}

int main(void) {
    test_string_builder_heap_backing();
    test_string_builder_fixed_backing();
    test_string_builder_init_with_capacity_and_invalid_pop();
    return 0;
}
