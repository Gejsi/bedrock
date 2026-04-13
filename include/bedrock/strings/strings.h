#ifndef BEDROCK_STRINGS_STRINGS_H
#define BEDROCK_STRINGS_STRINGS_H

#include <bedrock/unicode/utf8.h>

BR_EXTERN_C_BEGIN

typedef struct br_string {
  char *data;
  usize len;
} br_string;

typedef struct br_string_view {
  const char *data;
  usize len;
} br_string_view;

typedef struct br_string_result {
  br_string value;
  br_status status;
} br_string_result;

typedef struct br_string_view_list {
  br_string_view *data;
  usize len;
} br_string_view_list;

typedef struct br_string_view_list_result {
  br_string_view_list value;
  br_status status;
} br_string_view_list_result;

typedef struct br_string_rewrite_result {
  br_string_view value;
  br_string owned;
  bool allocated;
  br_status status;
} br_string_rewrite_result;

#define BR_STR_LIT(s) br_string_view_make((s), sizeof(s) - 1u)

static inline br_string br_string_make(void *data, usize len) {
  br_string string;

  string.data = (char *)data;
  string.len = len;
  return string;
}

static inline br_string_view br_string_view_make(const void *data, usize len) {
  br_string_view string;

  string.data = (const char *)data;
  string.len = len;
  return string;
}

static inline br_string_view br_string_view_from_string(br_string string) {
  return br_string_view_make(string.data, string.len);
}

static inline br_string_view br_string_view_from_cstr(const char *s) {
  return br_string_view_make(s, s != NULL ? strlen(s) : 0u);
}

static inline br_bytes_view br_string_view_as_bytes(br_string_view s) {
  return br_bytes_view_make(s.data, s.len);
}

/*
Free an owned string allocation.
*/
br_status br_string_free(br_string string, br_allocator allocator);
br_status br_string_view_list_free(br_string_view_list list, br_allocator allocator);
br_status br_string_rewrite_free(br_string_rewrite_result result, br_allocator allocator);

/*
Clone `s` into an owned string allocation.

This follows the shape of Odin's `strings.clone`, but keeps allocation explicit
instead of using an implicit context allocator.
*/
br_string_result br_string_clone(br_string_view s, br_allocator allocator);

/*
Compare two strings lexicographically.
*/
int br_string_compare(br_string_view lhs, br_string_view rhs);

/*
Return whether two strings have the same bytes.
*/
bool br_string_equal(br_string_view lhs, br_string_view rhs);

/*
Return whether `s` begins with `prefix`.
*/
bool br_string_has_prefix(br_string_view s, br_string_view prefix);

/*
Return whether `s` ends with `suffix`.
*/
bool br_string_has_suffix(br_string_view s, br_string_view suffix);

/*
Return whether `needle` occurs within `s`.
*/
bool br_string_contains(br_string_view s, br_string_view needle);
bool br_string_contains_any(br_string_view s, br_string_view chars);

/*
Return whether Unicode scalar value `value` occurs within `s`.

This follows Odin's `contains_rune` and walks the string as UTF-8 rather than
scanning raw bytes.
*/
bool br_string_contains_rune(br_string_view s, br_rune value);

/*
Report whether `s` is valid UTF-8.
*/
bool br_string_valid(br_string_view s);

/*
Return the byte offset of `needle` within `s`, or `-1` if it is absent.
*/
isize br_string_index(br_string_view s, br_string_view needle);
isize br_string_index_byte(br_string_view s, u8 byte_value);

/*
Return the byte offset of the first occurrence of rune `value`, or `-1` if it
is absent.
*/
isize br_string_index_rune(br_string_view s, br_rune value);
isize br_string_last_index(br_string_view s, br_string_view needle);
isize br_string_last_index_byte(br_string_view s, u8 byte_value);
isize br_string_index_any(br_string_view s, br_string_view chars);
isize br_string_last_index_any(br_string_view s, br_string_view chars);
usize br_string_count(br_string_view s, br_string_view needle);

/*
Return the number of UTF-8 runes in `s`.
*/
usize br_string_rune_count(br_string_view s);

br_string_result br_string_join(const br_string_view *parts,
                                usize part_count,
                                br_string_view sep,
                                br_allocator allocator);
br_string_result
br_string_concat(const br_string_view *parts, usize part_count, br_allocator allocator);
br_string_result br_string_repeat(br_string_view s, usize count, br_allocator allocator);

/*
Split `s` around separator `sep`.

Unlike the current byte-oriented `bytes` layer, an empty separator follows
Odin's string behavior and splits on UTF-8 rune boundaries.
*/
br_string_view_list_result
br_string_split(br_string_view s, br_string_view sep, br_allocator allocator);
br_string_view_list_result
br_string_split_n(br_string_view s, br_string_view sep, isize n, br_allocator allocator);
br_string_view_list_result
br_string_split_after(br_string_view s, br_string_view sep, br_allocator allocator);
br_string_view_list_result
br_string_split_after_n(br_string_view s, br_string_view sep, isize n, br_allocator allocator);

/*
Replace up to `n` occurrences of `old_string` with `new_string`.

If no rewrite is needed, `allocated` will be false and `value` will alias the
original input. If a rewrite allocates, `allocated` will be true, `owned` will
hold the allocation, and `value` will view that owned storage.

When `old_string` is empty, replacements are inserted at UTF-8 rune
boundaries, following Odin's string semantics.
*/
br_string_rewrite_result br_string_replace(br_string_view s,
                                           br_string_view old_string,
                                           br_string_view new_string,
                                           isize n,
                                           br_allocator allocator);
br_string_rewrite_result br_string_replace_all(br_string_view s,
                                               br_string_view old_string,
                                               br_string_view new_string,
                                               br_allocator allocator);
br_string_rewrite_result
br_string_remove(br_string_view s, br_string_view key, isize n, br_allocator allocator);
br_string_rewrite_result
br_string_remove_all(br_string_view s, br_string_view key, br_allocator allocator);

/*
Truncate `s` at the first occurrence of byte `byte_value`.

If `byte_value` does not occur, the original string view is returned.
*/
br_string_view br_string_truncate_to_byte(br_string_view s, u8 byte_value);

/*
Truncate `s` at the first occurrence of rune `value`.

If `value` does not occur, the original string view is returned.
*/
br_string_view br_string_truncate_to_rune(br_string_view s, br_rune value);

/*
Trim `prefix` from the start of `s` when present.
*/
br_string_view br_string_trim_prefix(br_string_view s, br_string_view prefix);

/*
Trim `suffix` from the end of `s` when present.
*/
br_string_view br_string_trim_suffix(br_string_view s, br_string_view suffix);

BR_EXTERN_C_END

#endif
