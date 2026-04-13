#include <assert.h>

#include <bedrock.h>

static void assert_string_view_list_eq(br_string_view_list list,
                                       const br_string_view *expected,
                                       usize expected_len) {
  assert(list.len == expected_len);
  for (usize i = 0; i < expected_len; ++i) {
    assert(br_string_equal(list.data[i], expected[i]));
  }
}

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
  assert(br_string_contains_any(BR_STR_LIT("testing"), BR_STR_LIT("xyzs")));
  assert(!br_string_contains_any(BR_STR_LIT("testing"), BR_STR_LIT("xyz")));

  assert(br_string_index_byte(BR_STR_LIT("testing"), (u8)'t') == 0);
  assert(br_string_index(s, BR_STR_LIT("def")) == 5);
  assert(br_string_index_rune(s, (br_rune)'a') == 0);
  assert(br_string_index_rune(s, (br_rune)0x00e4) == 3);
  assert(br_string_index_rune(s, (br_rune)'z') == -1);
  assert(br_string_last_index(BR_STR_LIT("abcabc"), BR_STR_LIT("abc")) == 3);
  assert(br_string_last_index_byte(BR_STR_LIT("abcabc"), (u8)'b') == 4);
  assert(br_string_index_any(BR_STR_LIT("teast"), BR_STR_LIT("s")) == 3);
  assert(br_string_index_any(BR_STR_LIT("teast"), BR_STR_LIT("se")) == 1);
  assert(br_string_last_index_any(BR_STR_LIT("teast"), BR_STR_LIT("se")) == 3);
  assert(br_string_count(BR_STR_LIT("abbccc"), BR_STR_LIT("c")) == 3u);
  assert(br_string_count(s, BR_STR_LIT("")) == 8u);
  assert(br_string_contains_rune(s, (br_rune)0x00e4));
  assert(!br_string_contains_rune(s, (br_rune)'z'));
}

static void test_strings_views(void) {
  static const u8 utf8_name[] = {'a', 'b', 'c', 0xc3u, 0xa4u, 'd', 'e', 'f'};
  br_string_view s = br_string_view_make(utf8_name, BR_ARRAY_COUNT(utf8_name));

  assert(br_string_equal(br_string_truncate_to_byte(BR_STR_LIT("name=value"), (u8)'='),
                         BR_STR_LIT("name")));
  assert(br_string_equal(br_string_truncate_to_rune(s, (br_rune)0x00e4), BR_STR_LIT("abc")));
  assert(br_string_equal(br_string_trim_prefix(BR_STR_LIT("foobar"), BR_STR_LIT("foo")),
                         BR_STR_LIT("bar")));
  assert(br_string_equal(br_string_trim_suffix(BR_STR_LIT("foobar"), BR_STR_LIT("bar")),
                         BR_STR_LIT("foo")));
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

static void test_strings_allocating_helpers(void) {
  br_string_result joined;
  br_string_result concatenated;
  br_string_result repeated;
  br_string_view parts[] = {
    BR_STR_LIT("alpha"),
    BR_STR_LIT("beta"),
    BR_STR_LIT("gamma"),
  };

  joined = br_string_join(parts, BR_ARRAY_COUNT(parts), BR_STR_LIT("|"), br_allocator_heap());
  assert(joined.status == BR_STATUS_OK);
  assert(br_string_equal(br_string_view_from_string(joined.value), BR_STR_LIT("alpha|beta|gamma")));
  assert(br_string_free(joined.value, br_allocator_heap()) == BR_STATUS_OK);

  concatenated = br_string_concat(parts, BR_ARRAY_COUNT(parts), br_allocator_heap());
  assert(concatenated.status == BR_STATUS_OK);
  assert(
    br_string_equal(br_string_view_from_string(concatenated.value), BR_STR_LIT("alphabetagamma")));
  assert(br_string_free(concatenated.value, br_allocator_heap()) == BR_STATUS_OK);

  repeated = br_string_repeat(BR_STR_LIT("ab"), 3u, br_allocator_heap());
  assert(repeated.status == BR_STATUS_OK);
  assert(br_string_equal(br_string_view_from_string(repeated.value), BR_STR_LIT("ababab")));
  assert(br_string_free(repeated.value, br_allocator_heap()) == BR_STATUS_OK);
}

static void test_strings_split_helpers(void) {
  static const u8 utf8_name[] = {'a', 'b', 'c', 0xc3u, 0xa4u, 'd', 'e', 'f'};
  br_string_view utf8_string = br_string_view_make(utf8_name, BR_ARRAY_COUNT(utf8_name));
  br_string_view_list_result split;
  br_string_view_list_result split_n;
  br_string_view_list_result split_after;
  br_string_view_list_result empty_split;
  br_string_view_list_result rune_split;
  br_string_view_list_result rune_split_n;
  const br_string_view expected_split[] = {
    BR_STR_LIT("alpha"),
    BR_STR_LIT("beta"),
    BR_STR_LIT("gamma"),
  };
  const br_string_view expected_split_n[] = {
    BR_STR_LIT("alpha"),
    BR_STR_LIT("beta,gamma"),
  };
  const br_string_view expected_split_after[] = {
    BR_STR_LIT("alpha,"),
    BR_STR_LIT("beta,"),
    BR_STR_LIT("gamma"),
  };
  const br_string_view expected_rune_split[] = {
    BR_STR_LIT("a"),
    BR_STR_LIT("b"),
    BR_STR_LIT("c"),
    br_string_view_make(utf8_name + 3, 2u),
    BR_STR_LIT("d"),
    BR_STR_LIT("e"),
    BR_STR_LIT("f"),
  };
  const br_string_view expected_rune_split_n[] = {
    BR_STR_LIT("a"),
    BR_STR_LIT("b"),
    br_string_view_make((const char *)utf8_name + 2, BR_ARRAY_COUNT(utf8_name) - 2u),
  };

  split = br_string_split(BR_STR_LIT("alpha,beta,gamma"), BR_STR_LIT(","), br_allocator_heap());
  assert(split.status == BR_STATUS_OK);
  assert_string_view_list_eq(split.value, expected_split, BR_ARRAY_COUNT(expected_split));
  assert(br_string_view_list_free(split.value, br_allocator_heap()) == BR_STATUS_OK);

  split_n =
    br_string_split_n(BR_STR_LIT("alpha,beta,gamma"), BR_STR_LIT(","), 2, br_allocator_heap());
  assert(split_n.status == BR_STATUS_OK);
  assert_string_view_list_eq(split_n.value, expected_split_n, BR_ARRAY_COUNT(expected_split_n));
  assert(br_string_view_list_free(split_n.value, br_allocator_heap()) == BR_STATUS_OK);

  split_after =
    br_string_split_after(BR_STR_LIT("alpha,beta,gamma"), BR_STR_LIT(","), br_allocator_heap());
  assert(split_after.status == BR_STATUS_OK);
  assert_string_view_list_eq(
    split_after.value, expected_split_after, BR_ARRAY_COUNT(expected_split_after));
  assert(br_string_view_list_free(split_after.value, br_allocator_heap()) == BR_STATUS_OK);

  empty_split = br_string_split_n(BR_STR_LIT("a,b,c"), BR_STR_LIT(","), 0, br_allocator_heap());
  assert(empty_split.status == BR_STATUS_OK);
  assert(empty_split.value.data == NULL);
  assert(empty_split.value.len == 0u);

  rune_split = br_string_split(utf8_string, BR_STR_LIT(""), br_allocator_heap());
  assert(rune_split.status == BR_STATUS_OK);
  assert_string_view_list_eq(
    rune_split.value, expected_rune_split, BR_ARRAY_COUNT(expected_rune_split));
  assert(br_string_view_list_free(rune_split.value, br_allocator_heap()) == BR_STATUS_OK);

  rune_split_n = br_string_split_n(utf8_string, BR_STR_LIT(""), 3, br_allocator_heap());
  assert(rune_split_n.status == BR_STATUS_OK);
  assert_string_view_list_eq(
    rune_split_n.value, expected_rune_split_n, BR_ARRAY_COUNT(expected_rune_split_n));
  assert(br_string_view_list_free(rune_split_n.value, br_allocator_heap()) == BR_STATUS_OK);
}

static void test_strings_replace_helpers(void) {
  static const u8 utf8_word[] = {'a', 0xc3u, 0xa4u, 'b'};
  static const u8 expected_inserted[] = {'.', 'a', '.', 0xc3u, 0xa4u, '.', 'b', '.'};
  br_string_view utf8_string = br_string_view_make(utf8_word, BR_ARRAY_COUNT(utf8_word));
  br_string_view expected_inserted_view =
    br_string_view_make(expected_inserted, BR_ARRAY_COUNT(expected_inserted));
  br_string_rewrite_result replaced;
  br_string_rewrite_result replaced_n;
  br_string_rewrite_result removed;
  br_string_rewrite_result noop;
  br_string_rewrite_result empty_old;

  replaced = br_string_replace_all(
    BR_STR_LIT("alpha,beta,gamma"), BR_STR_LIT(","), BR_STR_LIT("|"), br_allocator_heap());
  assert(replaced.status == BR_STATUS_OK);
  assert(replaced.allocated);
  assert(br_string_equal(replaced.value, BR_STR_LIT("alpha|beta|gamma")));
  assert(br_string_rewrite_free(replaced, br_allocator_heap()) == BR_STATUS_OK);

  replaced_n = br_string_replace(
    BR_STR_LIT("foofoofoo"), BR_STR_LIT("foo"), BR_STR_LIT("x"), 2, br_allocator_heap());
  assert(replaced_n.status == BR_STATUS_OK);
  assert(replaced_n.allocated);
  assert(br_string_equal(replaced_n.value, BR_STR_LIT("xxfoo")));
  assert(br_string_rewrite_free(replaced_n, br_allocator_heap()) == BR_STATUS_OK);

  removed = br_string_remove_all(BR_STR_LIT("a-b-c"), BR_STR_LIT("-"), br_allocator_heap());
  assert(removed.status == BR_STATUS_OK);
  assert(removed.allocated);
  assert(br_string_equal(removed.value, BR_STR_LIT("abc")));
  assert(br_string_rewrite_free(removed, br_allocator_heap()) == BR_STATUS_OK);

  noop = br_string_replace_all(
    BR_STR_LIT("unchanged"), BR_STR_LIT("zzz"), BR_STR_LIT("x"), br_allocator_heap());
  assert(noop.status == BR_STATUS_OK);
  assert(!noop.allocated);
  assert(br_string_equal(noop.value, BR_STR_LIT("unchanged")));
  assert(br_string_rewrite_free(noop, br_allocator_heap()) == BR_STATUS_OK);

  empty_old =
    br_string_replace(utf8_string, BR_STR_LIT(""), BR_STR_LIT("."), -1, br_allocator_heap());
  assert(empty_old.status == BR_STATUS_OK);
  assert(empty_old.allocated);
  assert(br_string_equal(empty_old.value, expected_inserted_view));
  assert(br_string_rewrite_free(empty_old, br_allocator_heap()) == BR_STATUS_OK);
}

int main(void) {
  test_strings_compare_and_search();
  test_strings_views();
  test_strings_utf8_helpers();
  test_strings_clone();
  test_strings_allocating_helpers();
  test_strings_split_helpers();
  test_strings_replace_helpers();
  return 0;
}
