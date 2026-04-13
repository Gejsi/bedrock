#include <assert.h>

#include <bedrock.h>

static void test_bytes_compare_and_search(void) {
    br_bytes_view abc = BR_BYTES_LIT("abc");
    br_bytes_view abcabc = BR_BYTES_LIT("abcabc");
    br_bytes_view empty = br_bytes_view_make(NULL, 0u);

    assert(br_bytes_compare(abc, BR_BYTES_LIT("abc")) == 0);
    assert(br_bytes_compare(abc, BR_BYTES_LIT("abd")) < 0);
    assert(br_bytes_compare(BR_BYTES_LIT("abd"), abc) > 0);
    assert(br_bytes_equal(abc, BR_BYTES_LIT("abc")));
    assert(!br_bytes_equal(abc, BR_BYTES_LIT("ab")));

    assert(br_bytes_has_prefix(abcabc, abc));
    assert(br_bytes_has_suffix(abcabc, abc));
    assert(br_bytes_has_prefix(abc, empty));
    assert(br_bytes_has_suffix(abc, empty));

    assert(br_bytes_index_byte(abcabc, (u8)'b') == 1);
    assert(br_bytes_last_index_byte(abcabc, (u8)'b') == 4);
    assert(br_bytes_index(abcabc, abc) == 0);
    assert(br_bytes_last_index(abcabc, abc) == 3);
    assert(br_bytes_index(abcabc, BR_BYTES_LIT("bc")) == 1);
    assert(br_bytes_last_index(abcabc, BR_BYTES_LIT("bc")) == 4);
    assert(br_bytes_index(abcabc, empty) == 0);
    assert(br_bytes_last_index(abcabc, empty) == 6);

    assert(br_bytes_contains(abcabc, BR_BYTES_LIT("cab")));
    assert(!br_bytes_contains(abcabc, BR_BYTES_LIT("zzz")));
    assert(br_bytes_contains_any(abcabc, BR_BYTES_LIT("zxby")));
    assert(!br_bytes_contains_any(abcabc, BR_BYTES_LIT("xyz")));
    assert(br_bytes_index_any(abcabc, BR_BYTES_LIT("xz")) == -1);
    assert(br_bytes_index_any(abcabc, BR_BYTES_LIT("zx")) == -1);
    assert(br_bytes_index_any(abcabc, BR_BYTES_LIT("zb")) == 1);
}

static void test_bytes_views(void) {
    br_bytes_view truncated = br_bytes_truncate_to_byte(BR_BYTES_LIT("name=value"), (u8)'=');
    br_bytes_view no_match = br_bytes_truncate_to_byte(BR_BYTES_LIT("plain"), (u8)'=');
    br_bytes_view trimmed_prefix = br_bytes_trim_prefix(BR_BYTES_LIT("foobar"), BR_BYTES_LIT("foo"));
    br_bytes_view trimmed_suffix = br_bytes_trim_suffix(BR_BYTES_LIT("foobar"), BR_BYTES_LIT("bar"));

    assert(br_bytes_equal(truncated, BR_BYTES_LIT("name")));
    assert(br_bytes_equal(no_match, BR_BYTES_LIT("plain")));
    assert(br_bytes_equal(trimmed_prefix, BR_BYTES_LIT("bar")));
    assert(br_bytes_equal(trimmed_suffix, BR_BYTES_LIT("foo")));
    assert(br_bytes_equal(br_bytes_trim_prefix(BR_BYTES_LIT("foobar"), BR_BYTES_LIT("zzz")), BR_BYTES_LIT("foobar")));
    assert(br_bytes_equal(br_bytes_trim_suffix(BR_BYTES_LIT("foobar"), BR_BYTES_LIT("zzz")), BR_BYTES_LIT("foobar")));
}

static void test_bytes_allocating_helpers(void) {
    br_bytes_result clone;
    br_bytes_result joined;
    br_bytes_result concatenated;
    br_bytes_result repeated;
    br_bytes_result failed_clone;
    br_bytes_view parts[] = {
        BR_BYTES_LIT("alpha"),
        BR_BYTES_LIT("beta"),
        BR_BYTES_LIT("gamma"),
    };

    clone = br_bytes_clone(BR_BYTES_LIT("clone"), br_allocator_heap());
    assert(clone.status == BR_STATUS_OK);
    assert(clone.value.data != NULL);
    assert(br_bytes_equal(br_bytes_view_from_bytes(clone.value), BR_BYTES_LIT("clone")));
    assert(br_bytes_free(clone.value, br_allocator_heap()) == BR_STATUS_OK);

    joined = br_bytes_join(parts, BR_ARRAY_COUNT(parts), BR_BYTES_LIT("|"), br_allocator_heap());
    assert(joined.status == BR_STATUS_OK);
    assert(br_bytes_equal(br_bytes_view_from_bytes(joined.value), BR_BYTES_LIT("alpha|beta|gamma")));
    assert(br_bytes_free(joined.value, br_allocator_heap()) == BR_STATUS_OK);

    concatenated = br_bytes_concat(parts, BR_ARRAY_COUNT(parts), br_allocator_heap());
    assert(concatenated.status == BR_STATUS_OK);
    assert(br_bytes_equal(br_bytes_view_from_bytes(concatenated.value), BR_BYTES_LIT("alphabetagamma")));
    assert(br_bytes_free(concatenated.value, br_allocator_heap()) == BR_STATUS_OK);

    repeated = br_bytes_repeat(BR_BYTES_LIT("ab"), 3u, br_allocator_heap());
    assert(repeated.status == BR_STATUS_OK);
    assert(br_bytes_equal(br_bytes_view_from_bytes(repeated.value), BR_BYTES_LIT("ababab")));
    assert(br_bytes_free(repeated.value, br_allocator_heap()) == BR_STATUS_OK);

    failed_clone = br_bytes_clone(BR_BYTES_LIT("x"), br_allocator_fail());
    assert(failed_clone.status == BR_STATUS_OUT_OF_MEMORY);
}

int main(void) {
    test_bytes_compare_and_search();
    test_bytes_views();
    test_bytes_allocating_helpers();
    return 0;
}
