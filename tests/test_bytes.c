#include <assert.h>

#include <bedrock.h>

static void assert_bytes_view_list_eq(br_bytes_view_list list,
                                      const br_bytes_view *expected,
                                      usize expected_len) {
  assert(list.len == expected_len);
  for (usize i = 0; i < expected_len; ++i) {
    assert(br_bytes_equal(list.data[i], expected[i]));
  }
}

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
  assert(br_bytes_count(abcabc, BR_BYTES_LIT("abc")) == 2);
  assert(br_bytes_count(abcabc, BR_BYTES_LIT("b")) == 2);
  assert(br_bytes_count(abcabc, empty) == 7);
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
  assert(br_bytes_equal(br_bytes_trim_prefix(BR_BYTES_LIT("foobar"), BR_BYTES_LIT("zzz")),
                        BR_BYTES_LIT("foobar")));
  assert(br_bytes_equal(br_bytes_trim_suffix(BR_BYTES_LIT("foobar"), BR_BYTES_LIT("zzz")),
                        BR_BYTES_LIT("foobar")));
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
  assert(
    br_bytes_equal(br_bytes_view_from_bytes(concatenated.value), BR_BYTES_LIT("alphabetagamma")));
  assert(br_bytes_free(concatenated.value, br_allocator_heap()) == BR_STATUS_OK);

  repeated = br_bytes_repeat(BR_BYTES_LIT("ab"), 3u, br_allocator_heap());
  assert(repeated.status == BR_STATUS_OK);
  assert(br_bytes_equal(br_bytes_view_from_bytes(repeated.value), BR_BYTES_LIT("ababab")));
  assert(br_bytes_free(repeated.value, br_allocator_heap()) == BR_STATUS_OK);

  failed_clone = br_bytes_clone(BR_BYTES_LIT("x"), br_allocator_fail());
  assert(failed_clone.status == BR_STATUS_OUT_OF_MEMORY);
}

static void test_bytes_split_helpers(void) {
  br_bytes_view_list_result split;
  br_bytes_view_list_result split_n;
  br_bytes_view_list_result split_after;
  br_bytes_view_list_result split_after_n;
  br_bytes_view_list_result invalid;
  const br_bytes_view expected_split[] = {
    BR_BYTES_LIT("alpha"),
    BR_BYTES_LIT("beta"),
    BR_BYTES_LIT("gamma"),
  };
  const br_bytes_view expected_split_n[] = {
    BR_BYTES_LIT("alpha"),
    BR_BYTES_LIT("beta,gamma"),
  };
  const br_bytes_view expected_split_after[] = {
    BR_BYTES_LIT("alpha,"),
    BR_BYTES_LIT("beta,"),
    BR_BYTES_LIT("gamma"),
  };
  const br_bytes_view expected_split_after_n[] = {
    BR_BYTES_LIT("alpha,"),
    BR_BYTES_LIT("beta,gamma"),
  };

  split = br_bytes_split(BR_BYTES_LIT("alpha,beta,gamma"), BR_BYTES_LIT(","), br_allocator_heap());
  assert(split.status == BR_STATUS_OK);
  assert_bytes_view_list_eq(split.value, expected_split, BR_ARRAY_COUNT(expected_split));
  assert(br_bytes_view_list_free(split.value, br_allocator_heap()) == BR_STATUS_OK);

  split_n =
    br_bytes_split_n(BR_BYTES_LIT("alpha,beta,gamma"), BR_BYTES_LIT(","), 2, br_allocator_heap());
  assert(split_n.status == BR_STATUS_OK);
  assert_bytes_view_list_eq(split_n.value, expected_split_n, BR_ARRAY_COUNT(expected_split_n));
  assert(br_bytes_view_list_free(split_n.value, br_allocator_heap()) == BR_STATUS_OK);

  split_after =
    br_bytes_split_after(BR_BYTES_LIT("alpha,beta,gamma"), BR_BYTES_LIT(","), br_allocator_heap());
  assert(split_after.status == BR_STATUS_OK);
  assert_bytes_view_list_eq(
    split_after.value, expected_split_after, BR_ARRAY_COUNT(expected_split_after));
  assert(br_bytes_view_list_free(split_after.value, br_allocator_heap()) == BR_STATUS_OK);

  split_after_n = br_bytes_split_after_n(
    BR_BYTES_LIT("alpha,beta,gamma"), BR_BYTES_LIT(","), 2, br_allocator_heap());
  assert(split_after_n.status == BR_STATUS_OK);
  assert_bytes_view_list_eq(
    split_after_n.value, expected_split_after_n, BR_ARRAY_COUNT(expected_split_after_n));
  assert(br_bytes_view_list_free(split_after_n.value, br_allocator_heap()) == BR_STATUS_OK);

  invalid = br_bytes_split(BR_BYTES_LIT("abc"), BR_BYTES_LIT(""), br_allocator_heap());
  assert(invalid.status == BR_STATUS_INVALID_ARGUMENT);
  assert(invalid.value.data == NULL);
  assert(invalid.value.len == 0u);

  split_n = br_bytes_split_n(BR_BYTES_LIT("a,b,c"), BR_BYTES_LIT(","), 0, br_allocator_heap());
  assert(split_n.status == BR_STATUS_OK);
  assert(split_n.value.data == NULL);
  assert(split_n.value.len == 0u);
}

static void test_bytes_replace_helpers(void) {
  br_bytes_rewrite_result replaced;
  br_bytes_rewrite_result replaced_n;
  br_bytes_rewrite_result removed;
  br_bytes_rewrite_result noop;
  br_bytes_rewrite_result empty_old;

  replaced = br_bytes_replace_all(
    BR_BYTES_LIT("alpha,beta,gamma"), BR_BYTES_LIT(","), BR_BYTES_LIT("|"), br_allocator_heap());
  assert(replaced.status == BR_STATUS_OK);
  assert(replaced.allocated);
  assert(br_bytes_equal(replaced.value, BR_BYTES_LIT("alpha|beta|gamma")));
  assert(br_bytes_rewrite_free(replaced, br_allocator_heap()) == BR_STATUS_OK);

  replaced_n = br_bytes_replace(
    BR_BYTES_LIT("foofoofoo"), BR_BYTES_LIT("foo"), BR_BYTES_LIT("x"), 2, br_allocator_heap());
  assert(replaced_n.status == BR_STATUS_OK);
  assert(replaced_n.allocated);
  assert(br_bytes_equal(replaced_n.value, BR_BYTES_LIT("xxfoo")));
  assert(br_bytes_rewrite_free(replaced_n, br_allocator_heap()) == BR_STATUS_OK);

  removed = br_bytes_remove_all(BR_BYTES_LIT("a-b-c"), BR_BYTES_LIT("-"), br_allocator_heap());
  assert(removed.status == BR_STATUS_OK);
  assert(removed.allocated);
  assert(br_bytes_equal(removed.value, BR_BYTES_LIT("abc")));
  assert(br_bytes_rewrite_free(removed, br_allocator_heap()) == BR_STATUS_OK);

  noop = br_bytes_replace_all(
    BR_BYTES_LIT("unchanged"), BR_BYTES_LIT("zzz"), BR_BYTES_LIT("x"), br_allocator_heap());
  assert(noop.status == BR_STATUS_OK);
  assert(!noop.allocated);
  assert(br_bytes_equal(noop.value, BR_BYTES_LIT("unchanged")));
  assert(br_bytes_rewrite_free(noop, br_allocator_heap()) == BR_STATUS_OK);

  empty_old = br_bytes_replace(
    BR_BYTES_LIT("ab"), BR_BYTES_LIT(""), BR_BYTES_LIT("."), -1, br_allocator_heap());
  assert(empty_old.status == BR_STATUS_OK);
  assert(empty_old.allocated);
  assert(br_bytes_equal(empty_old.value, BR_BYTES_LIT(".a.b.")));
  assert(br_bytes_rewrite_free(empty_old, br_allocator_heap()) == BR_STATUS_OK);
}

int main(void) {
  test_bytes_compare_and_search();
  test_bytes_views();
  test_bytes_allocating_helpers();
  test_bytes_split_helpers();
  test_bytes_replace_helpers();
  return 0;
}
