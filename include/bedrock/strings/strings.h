#ifndef BEDROCK_STRINGS_STRINGS_H
#define BEDROCK_STRINGS_STRINGS_H

#include <bedrock/unicode/utf8.h>

BR_EXTERN_C_BEGIN

typedef struct br_string {
  char *data;
  size_t len;
} br_string;

typedef struct br_string_view {
  const char *data;
  size_t len;
} br_string_view;

typedef struct br_string_result {
  br_string value;
  br_status status;
} br_string_result;

typedef struct br_string_view_list {
  br_string_view *data;
  size_t len;
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

static inline br_string br_string_make(void *data, size_t len) {
  br_string string;

  string.data = (char *)data;
  string.len = len;
  return string;
}

static inline br_string_view br_string_view_make(const void *data, size_t len) {
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
Clone `s` into an owned string with ASCII letters mapped to a single case.

Only `A`-`Z` (or `a`-`z`) are remapped; all other bytes, including the bytes of
multibyte UTF-8 runes, are copied unchanged. Unicode-aware case conversion is
deferred until the case tables land, so these are the ASCII-only forms.
*/
br_string_result br_string_to_lower_ascii(br_string_view s, br_allocator allocator);
br_string_result br_string_to_upper_ascii(br_string_view s, br_allocator allocator);

/*
Compare two strings lexicographically.
*/
int32_t br_string_compare(br_string_view lhs, br_string_view rhs);

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
ptrdiff_t br_string_index(br_string_view s, br_string_view needle);
ptrdiff_t br_string_index_byte(br_string_view s, uint8_t byte_value);

/*
Return the byte offset of the first occurrence of rune `value`, or `-1` if it
is absent.
*/
ptrdiff_t br_string_index_rune(br_string_view s, br_rune value);
ptrdiff_t br_string_last_index(br_string_view s, br_string_view needle);
ptrdiff_t br_string_last_index_byte(br_string_view s, uint8_t byte_value);
ptrdiff_t br_string_index_any(br_string_view s, br_string_view chars);
ptrdiff_t br_string_last_index_any(br_string_view s, br_string_view chars);
size_t br_string_count(br_string_view s, br_string_view needle);

/*
Return the number of UTF-8 runes in `s`.
*/
size_t br_string_rune_count(br_string_view s);

br_string_result br_string_join(const br_string_view *parts,
                                size_t part_count,
                                br_string_view sep,
                                br_allocator allocator);
br_string_result
br_string_concat(const br_string_view *parts, size_t part_count, br_allocator allocator);
br_string_result br_string_repeat(br_string_view s, size_t count, br_allocator allocator);

/*
Split `s` around runs of ASCII whitespace, returning the non-empty fields.

Leading/trailing whitespace yield no empty fields and consecutive whitespace
collapses. Only ASCII whitespace separates; bytes at or above `0x80` are field
content, so multibyte UTF-8 runes stay within a field. ASCII-only, unlike
Odin's Unicode `is_space` fallback (deferred until the space tables land).
*/
br_string_view_list_result br_string_fields(br_string_view s, br_allocator allocator);

/*
Split `s` around separator `sep`.

Unlike the current byte-oriented `bytes` layer, an empty separator follows
Odin's string behavior and splits on UTF-8 rune boundaries.
*/
br_string_view_list_result
br_string_split(br_string_view s, br_string_view sep, br_allocator allocator);
br_string_view_list_result
br_string_split_n(br_string_view s, br_string_view sep, ptrdiff_t n, br_allocator allocator);
br_string_view_list_result
br_string_split_after(br_string_view s, br_string_view sep, br_allocator allocator);
br_string_view_list_result
br_string_split_after_n(br_string_view s, br_string_view sep, ptrdiff_t n, br_allocator allocator);

/*
Allocation-free split iterator.

`br_string_split_iter` is a caller-owned cursor advanced by
`br_string_split_iter_next`. Each call writes the next field through `out` and
returns true; at the end it returns false and leaves `out` unmodified, so
callers must check the return before reading `out`.

The iterator yields exactly the sequence `br_string_split` would produce for the
same `(s, sep)`, element for element -- the allocation-free form of the split
list. It KEEPS the trailing empty field and yields one empty field when
iterating an empty input against a non-empty separator, deviating from Odin's
`split_iterator` (which drops the trailing empty). Callers wanting empties
skipped use `br_string_fields`. An empty separator walks one UTF-8 rune per step.

`br_string_split_after_iter_next` keeps each field's trailing separator,
mirroring `br_string_split_after`.
*/
typedef struct br_string_split_iter {
  br_string_view rest;
  br_string_view sep;
  bool done;
} br_string_split_iter;

static inline br_string_split_iter br_string_split_iter_make(br_string_view s, br_string_view sep) {
  br_string_split_iter it;

  it.rest = s;
  it.sep = sep;
  it.done = false;
  return it;
}

bool br_string_split_iter_next(br_string_split_iter *it, br_string_view *out);
bool br_string_split_after_iter_next(br_string_split_iter *it, br_string_view *out);

/*
Allocation-free fields iterator.

`br_string_fields_iter` is a caller-owned cursor advanced by
`br_string_fields_iter_next`, yielding each ASCII-whitespace-separated field in
order. It never yields empty fields, matching `br_string_fields`. Returns false
and leaves `out` unmodified once the input is exhausted.
*/
typedef struct br_string_fields_iter {
  br_string_view rest;
} br_string_fields_iter;

static inline br_string_fields_iter br_string_fields_iter_make(br_string_view s) {
  br_string_fields_iter it;

  it.rest = s;
  return it;
}

bool br_string_fields_iter_next(br_string_fields_iter *it, br_string_view *out);

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
                                           ptrdiff_t n,
                                           br_allocator allocator);
br_string_rewrite_result br_string_replace_all(br_string_view s,
                                               br_string_view old_string,
                                               br_string_view new_string,
                                               br_allocator allocator);
br_string_rewrite_result
br_string_remove(br_string_view s, br_string_view key, ptrdiff_t n, br_allocator allocator);
br_string_rewrite_result
br_string_remove_all(br_string_view s, br_string_view key, br_allocator allocator);

/*
Truncate `s` at the first occurrence of byte `byte_value`.

If `byte_value` does not occur, the original string view is returned.
*/
br_string_view br_string_truncate_to_byte(br_string_view s, uint8_t byte_value);

/*
Truncate `s` at the first occurrence of rune `value`.

If `value` does not occur, the original string view is returned.
*/
br_string_view br_string_truncate_to_rune(br_string_view s, br_rune value);

/*
Return the substring of `s` starting at rune index `rune_offset` spanning
`rune_length` runes. A `rune_length` of 0 means "to the end of the string".

`rune_offset` past the end yields an empty view; `rune_length` past the end is
clamped to the end. Rune-indexed, following Odin's `cut`.
*/
br_string_view br_string_cut(br_string_view s, size_t rune_offset, size_t rune_length);

/*
Return the rune subrange `[rune_start, rune_end)` of `s`.

`ok` (when non-NULL) reports whether both indices were in bounds
(`rune_start <= rune_end` and both no greater than the rune count). When the
range is out of bounds, an empty view is returned and `*ok` is false. Follows
Odin's `substring`.
*/
br_string_view br_string_substring(br_string_view s, size_t rune_start, size_t rune_end, bool *ok);

/*
Return the length in bytes of the longest common prefix of `a` and `b`, not
splitting a multibyte UTF-8 rune. Follows Odin's `prefix_length`.
*/
size_t br_string_prefix_length(br_string_view a, br_string_view b);

/*
Return the longest common prefix of `a` and `b` as a sub-view of `a`.
*/
br_string_view br_string_common_prefix(br_string_view a, br_string_view b);

/*
Trim `prefix` from the start of `s` when present.
*/
br_string_view br_string_trim_prefix(br_string_view s, br_string_view prefix);

/*
Trim `suffix` from the end of `s` when present.
*/
br_string_view br_string_trim_suffix(br_string_view s, br_string_view suffix);

/*
Trim runes belonging to `cutset` from one or both ends of `s`.

`cutset` is a set of Unicode code points (decoded as UTF-8), matching Odin and
Go. Returns a sub-view of `s`.
*/
br_string_view br_string_trim_left(br_string_view s, br_string_view cutset);
br_string_view br_string_trim_right(br_string_view s, br_string_view cutset);
br_string_view br_string_trim(br_string_view s, br_string_view cutset);

/*
Trim ASCII whitespace (` `, `\t`, `\n`, `\v`, `\f`, `\r`) from one or both ends.

ASCII-only: Odin's `trim_space` uses the Unicode `is_space` classifier, which
Bedrock defers until the case/space tables land. Documented deviation.
*/
br_string_view br_string_trim_left_space(br_string_view s);
br_string_view br_string_trim_right_space(br_string_view s);
br_string_view br_string_trim_space(br_string_view s);

/*
Trim NUL (`0x00`) runes from one or both ends of `s`.
*/
br_string_view br_string_trim_left_null(br_string_view s);
br_string_view br_string_trim_right_null(br_string_view s);
br_string_view br_string_trim_null(br_string_view s);

BR_EXTERN_C_END

#endif
