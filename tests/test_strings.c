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

static void test_strings_case_conversion(void) {
  /* "Héllo9" with é = U+00E9 (0xC3 0xA9): ASCII letters flip, the rune and the
     digit are preserved. */
  static const u8 mixed[] = {'H', 0xc3u, 0xa9u, 'l', 'l', 'o', '9'};
  br_string_view src = br_string_view_make(mixed, BR_ARRAY_COUNT(mixed));
  static const u8 lowered[] = {'h', 0xc3u, 0xa9u, 'l', 'l', 'o', '9'};
  static const u8 uppered[] = {'H', 0xc3u, 0xa9u, 'L', 'L', 'O', '9'};
  br_string_result lower;
  br_string_result upper;
  br_string_result failed;

  lower = br_string_to_lower_ascii(src, br_allocator_heap());
  assert(lower.status == BR_STATUS_OK);
  assert(br_string_equal(br_string_view_from_string(lower.value),
                         br_string_view_make(lowered, BR_ARRAY_COUNT(lowered))));
  assert(br_string_free(lower.value, br_allocator_heap()) == BR_STATUS_OK);

  upper = br_string_to_upper_ascii(src, br_allocator_heap());
  assert(upper.status == BR_STATUS_OK);
  assert(br_string_equal(br_string_view_from_string(upper.value),
                         br_string_view_make(uppered, BR_ARRAY_COUNT(uppered))));
  assert(br_string_free(upper.value, br_allocator_heap()) == BR_STATUS_OK);

  failed = br_string_to_lower_ascii(BR_STR_LIT("ABC"), br_allocator_fail());
  assert(failed.status == BR_STATUS_OUT_OF_MEMORY);
  assert(failed.value.data == NULL);
}

static void test_strings_trim(void) {
  assert(br_string_equal(br_string_trim(BR_STR_LIT("xxhixx"), BR_STR_LIT("x")), BR_STR_LIT("hi")));
  assert(
    br_string_equal(br_string_trim_left(BR_STR_LIT("xxhi"), BR_STR_LIT("x")), BR_STR_LIT("hi")));
  assert(
    br_string_equal(br_string_trim_right(BR_STR_LIT("hixx"), BR_STR_LIT("x")), BR_STR_LIT("hi")));
  assert(br_string_equal(br_string_trim_space(BR_STR_LIT("\t hello \n")), BR_STR_LIT("hello")));
  assert(br_string_equal(br_string_trim_space(BR_STR_LIT("   ")), BR_STR_LIT("")));

  /* Rune-aware cutset: strip a multibyte rune (U+00E9) from both ends. */
  {
    static const u8 input[] = {0xc3u, 0xa9u, 'o', 'k', 0xc3u, 0xa9u};
    static const u8 cutset[] = {0xc3u, 0xa9u};
    br_string_view got = br_string_trim(br_string_view_make(input, sizeof(input)),
                                        br_string_view_make(cutset, sizeof(cutset)));
    assert(br_string_equal(got, BR_STR_LIT("ok")));
  }

  /* NUL trim. */
  {
    static const u8 padded[] = {0x00u, 't', 'x', 't', 0x00u, 0x00u};
    br_string_view got = br_string_trim_null(br_string_view_make(padded, sizeof(padded)));
    assert(br_string_equal(got, BR_STR_LIT("txt")));
  }
}

static void test_strings_fields(void) {
  br_string_view_list_result r;

  r = br_string_fields(BR_STR_LIT("  the quick\tbrown \n fox "), br_allocator_heap());
  assert(r.status == BR_STATUS_OK);
  assert(r.value.len == 4u);
  assert(br_string_equal(r.value.data[0], BR_STR_LIT("the")));
  assert(br_string_equal(r.value.data[1], BR_STR_LIT("quick")));
  assert(br_string_equal(r.value.data[2], BR_STR_LIT("brown")));
  assert(br_string_equal(r.value.data[3], BR_STR_LIT("fox")));
  assert(br_string_view_list_free(r.value, br_allocator_heap()) == BR_STATUS_OK);

  r = br_string_fields(BR_STR_LIT("   "), br_allocator_heap());
  assert(r.status == BR_STATUS_OK);
  assert(r.value.len == 0u);

  r = br_string_fields(BR_STR_LIT("single"), br_allocator_heap());
  assert(r.status == BR_STATUS_OK);
  assert(r.value.len == 1u);
  assert(br_string_equal(r.value.data[0], BR_STR_LIT("single")));
  assert(br_string_view_list_free(r.value, br_allocator_heap()) == BR_STATUS_OK);
}

static void test_strings_cut_substring(void) {
  /* "héllo" with é = U+00E9 (2 bytes): 5 runes, 6 bytes. */
  static const u8 hello[] = {'h', 0xc3u, 0xa9u, 'l', 'l', 'o'};
  br_string_view s = br_string_view_make(hello, BR_ARRAY_COUNT(hello));
  bool ok;
  br_string_view got;

  /* cut(offset, length) is rune-indexed. */
  assert(
    br_string_equal(br_string_cut(s, 0u, 2u), br_string_view_make(hello, 3u))); /* "hé" = 3 bytes */
  assert(br_string_equal(br_string_cut(s, 3u, 0u), BR_STR_LIT("lo"))); /* runes 3..4 to end */
  assert(br_string_equal(br_string_cut(s, 1u, 1u), br_string_view_make(hello + 1, 2u))); /* "é" */
  /* Offset past the end yields empty. */
  assert(br_string_cut(s, 10u, 0u).len == 0u);
  /* Length past the end clamps to the end. */
  assert(br_string_equal(br_string_cut(s, 3u, 99u), BR_STR_LIT("lo")));

  /* substring(start, end) with bounds flag. */
  got = br_string_substring(s, 0u, 5u, &ok);
  assert(ok);
  assert(br_string_equal(got, s));
  got = br_string_substring(s, 1u, 3u, &ok);
  assert(ok);
  assert(br_string_equal(got, br_string_view_make(hello + 1, 3u))); /* "él" */
  /* Out of range and inverted range report ok = false, empty view. */
  got = br_string_substring(s, 0u, 99u, &ok);
  assert(!ok);
  assert(got.len == 0u);
  got = br_string_substring(s, 3u, 1u, &ok);
  assert(!ok);
  assert(got.len == 0u);
}

static void test_strings_prefix(void) {
  /* Common prefix stops at a whole-rune boundary; the shared "té" is 3 bytes. */
  static const u8 a[] = {'t', 0xc3u, 0xa9u, 'a'};
  static const u8 b[] = {'t', 0xc3u, 0xa9u, 'b'};
  br_string_view va = br_string_view_make(a, BR_ARRAY_COUNT(a));
  br_string_view vb = br_string_view_make(b, BR_ARRAY_COUNT(b));

  assert(br_string_prefix_length(BR_STR_LIT("testing"), BR_STR_LIT("test")) == 4u);
  assert(br_string_prefix_length(BR_STR_LIT("telephone"), BR_STR_LIT("te")) == 2u);
  assert(br_string_prefix_length(BR_STR_LIT("abc"), BR_STR_LIT("xyz")) == 0u);
  assert(br_string_equal(br_string_common_prefix(BR_STR_LIT("testing"), BR_STR_LIT("test")),
                         BR_STR_LIT("test")));

  /* A rune that shares its lead byte but differs later must not partially match. */
  assert(br_string_prefix_length(va, vb) == 3u);
  assert(br_string_equal(br_string_common_prefix(va, vb), br_string_view_make(a, 3u)));
}

int main(void) {
  test_strings_compare_and_search();
  test_strings_views();
  test_strings_utf8_helpers();
  test_strings_clone();
  test_strings_allocating_helpers();
  test_strings_split_helpers();
  test_strings_replace_helpers();
  test_strings_case_conversion();
  test_strings_trim();
  test_strings_fields();
  test_strings_cut_substring();
  test_strings_prefix();
  return 0;
}
