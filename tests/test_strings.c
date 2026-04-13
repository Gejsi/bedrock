#include <assert.h>

#include <bedrock.h>

static void test_strings_compare_and_search(void) {
    static const u8 utf8_name[] = {'a', 'b', 'c', 0xc3u, 0xa4u, 'd', 'e', 'f'};
    br_string_view s = br_string_view_make(utf8_name, BR_ARRAY_COUNT(utf8_name));

    assert(br_string_compare(BR_STR_LIT("abc"), BR_STR_LIT("abc")) == 0);
    assert(br_string_compare(BR_STR_LIT("abc"), BR_STR_LIT("abd")) < 0);
    assert(br_string_compare(BR_STR_LIT("abd"), BR_STR_LIT("abc")) > 0);

    assert(br_string_equal(BR_STR_LIT("alpha"), BR_STR_LIT("alpha")));
    assert(!br_string_equal(BR_STR_LIT("alpha"), BR_STR_LIT("alph")));
    assert(br_string_has_prefix(BR_STR_LIT("testing"), BR_STR_LIT("test")));
    assert(br_string_has_suffix(BR_STR_LIT("todo.txt"), BR_STR_LIT(".txt")));
    assert(br_string_contains(BR_STR_LIT("testing"), BR_STR_LIT("sti")));

    assert(br_string_index(s, BR_STR_LIT("def")) == 5);
    assert(br_string_index_rune(s, (br_rune)'a') == 0);
    assert(br_string_index_rune(s, (br_rune)0x00e4) == 3);
    assert(br_string_index_rune(s, (br_rune)'z') == -1);
    assert(br_string_contains_rune(s, (br_rune)0x00e4));
    assert(!br_string_contains_rune(s, (br_rune)'z'));
}

static void test_strings_views(void) {
    static const u8 utf8_name[] = {'a', 'b', 'c', 0xc3u, 0xa4u, 'd', 'e', 'f'};
    br_string_view s = br_string_view_make(utf8_name, BR_ARRAY_COUNT(utf8_name));

    assert(br_string_equal(br_string_truncate_to_byte(BR_STR_LIT("name=value"), (u8)'='), BR_STR_LIT("name")));
    assert(br_string_equal(br_string_truncate_to_rune(s, (br_rune)0x00e4), BR_STR_LIT("abc")));
    assert(br_string_equal(br_string_trim_prefix(BR_STR_LIT("foobar"), BR_STR_LIT("foo")), BR_STR_LIT("bar")));
    assert(br_string_equal(br_string_trim_suffix(BR_STR_LIT("foobar"), BR_STR_LIT("bar")), BR_STR_LIT("foo")));
}

static void test_strings_utf8_helpers(void) {
    static const u8 valid[] = {'A', 0xe2u, 0x82u, 0xacu, 0xf0u, 0x9fu, 0x99u, 0x82u};
    static const u8 invalid[] = {'A', 0x80u, 'B'};

    assert(br_string_rune_count(br_string_view_make(valid, BR_ARRAY_COUNT(valid))) == 3u);
    assert(br_string_valid(br_string_view_make(valid, BR_ARRAY_COUNT(valid))));
    assert(!br_string_valid(br_string_view_make(invalid, BR_ARRAY_COUNT(invalid))));
}

static void test_strings_clone(void) {
    br_string_result clone;

    clone = br_string_clone(BR_STR_LIT("hello"), br_allocator_heap());
    assert(clone.status == BR_STATUS_OK);
    assert(clone.value.data != NULL);
    assert(br_string_equal(br_string_view_from_string(clone.value), BR_STR_LIT("hello")));
    assert(br_string_free(clone.value, br_allocator_heap()) == BR_STATUS_OK);
}

int main(void) {
    test_strings_compare_and_search();
    test_strings_views();
    test_strings_utf8_helpers();
    test_strings_clone();
    return 0;
}
