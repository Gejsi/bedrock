#ifndef BEDROCK_BYTES_BYTES_H
#define BEDROCK_BYTES_BYTES_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_bytes {
  uint8_t *data;
  size_t len;
} br_bytes;

typedef struct br_bytes_view {
  const uint8_t *data;
  size_t len;
} br_bytes_view;

typedef struct br_bytes_result {
  br_bytes value;
  br_status status;
} br_bytes_result;

typedef struct br_bytes_view_list {
  br_bytes_view *data;
  size_t len;
} br_bytes_view_list;

typedef struct br_bytes_view_list_result {
  br_bytes_view_list value;
  br_status status;
} br_bytes_view_list_result;

typedef struct br_bytes_rewrite_result {
  br_bytes_view value;
  br_bytes owned;
  bool allocated;
  br_status status;
} br_bytes_rewrite_result;

#define BR_BYTES_LIT(s) br_bytes_view_make((const void *)(s), sizeof(s) - 1u)

static inline br_bytes br_bytes_make(void *data, size_t len) {
  br_bytes bytes;

  bytes.data = (uint8_t *)data;
  bytes.len = len;
  return bytes;
}

static inline br_bytes_view br_bytes_view_make(const void *data, size_t len) {
  br_bytes_view bytes;

  bytes.data = (const uint8_t *)data;
  bytes.len = len;
  return bytes;
}

static inline br_bytes_view br_bytes_view_from_bytes(br_bytes bytes) {
  return br_bytes_view_make(bytes.data, bytes.len);
}

static inline br_bytes_view br_bytes_view_from_cstr(const char *s) {
  return br_bytes_view_make(s, s != NULL ? strlen(s) : 0u);
}

br_status br_bytes_free(br_bytes bytes, br_allocator allocator);
br_status br_bytes_view_list_free(br_bytes_view_list list, br_allocator allocator);
br_status br_bytes_rewrite_free(br_bytes_rewrite_result result, br_allocator allocator);
br_bytes_result br_bytes_clone(br_bytes_view src, br_allocator allocator);

/*
Clone `src` into an owned buffer with ASCII letters mapped to a single case.

Only bytes `A`-`Z` (or `a`-`z`) are remapped; every other byte, including UTF-8
continuation and lead bytes at or above `0x80`, is copied unchanged, so valid
UTF-8 stays valid. Unicode-aware case conversion is deferred until the case
tables land; these are the ASCII-only forms, hence the `_ascii` suffix.
*/
br_bytes_result br_bytes_to_lower_ascii(br_bytes_view src, br_allocator allocator);
br_bytes_result br_bytes_to_upper_ascii(br_bytes_view src, br_allocator allocator);

int32_t br_bytes_compare(br_bytes_view lhs, br_bytes_view rhs);
bool br_bytes_equal(br_bytes_view lhs, br_bytes_view rhs);
bool br_bytes_has_prefix(br_bytes_view s, br_bytes_view prefix);
bool br_bytes_has_suffix(br_bytes_view s, br_bytes_view suffix);
bool br_bytes_contains(br_bytes_view s, br_bytes_view needle);
bool br_bytes_contains_any(br_bytes_view s, br_bytes_view chars);

ptrdiff_t br_bytes_index_byte(br_bytes_view s, uint8_t byte_value);
ptrdiff_t br_bytes_last_index_byte(br_bytes_view s, uint8_t byte_value);
ptrdiff_t br_bytes_index(br_bytes_view s, br_bytes_view needle);
ptrdiff_t br_bytes_last_index(br_bytes_view s, br_bytes_view needle);
ptrdiff_t br_bytes_index_any(br_bytes_view s, br_bytes_view chars);
ptrdiff_t br_bytes_count(br_bytes_view s, br_bytes_view needle);

br_bytes_view br_bytes_truncate_to_byte(br_bytes_view s, uint8_t byte_value);
br_bytes_view br_bytes_trim_prefix(br_bytes_view s, br_bytes_view prefix);
br_bytes_view br_bytes_trim_suffix(br_bytes_view s, br_bytes_view suffix);

/*
Trim runes belonging to `cutset` from one or both ends of `s`.

`cutset` is treated as a set of Unicode code points (decoded as UTF-8): each
edge rune of `s` is removed while it is a member. Returns a sub-view of `s` (no
allocation).
*/
br_bytes_view br_bytes_trim_left(br_bytes_view s, br_bytes_view cutset);
br_bytes_view br_bytes_trim_right(br_bytes_view s, br_bytes_view cutset);
br_bytes_view br_bytes_trim(br_bytes_view s, br_bytes_view cutset);

/*
Trim ASCII whitespace (` `, `\t`, `\n`, `\v`, `\f`, `\r`) from one or both ends.

This is ASCII-only (the Unicode whitespace classifier is deferred until the
space tables land).
*/
br_bytes_view br_bytes_trim_left_space(br_bytes_view s);
br_bytes_view br_bytes_trim_right_space(br_bytes_view s);
br_bytes_view br_bytes_trim_space(br_bytes_view s);

/*
Trim NUL (`0x00`) bytes from one or both ends.
*/
br_bytes_view br_bytes_trim_left_null(br_bytes_view s);
br_bytes_view br_bytes_trim_right_null(br_bytes_view s);
br_bytes_view br_bytes_trim_null(br_bytes_view s);

/*
Split `s` around runs of ASCII whitespace, returning the non-empty fields.

Leading and trailing whitespace produce no empty fields, and consecutive
whitespace bytes collapse into a single split point. Only ASCII whitespace
(` `, `\t`, `\n`, `\v`, `\f`, `\r`) separates; bytes at or above `0x80` are
field content, so multibyte UTF-8 runes stay within a field. This is
ASCII-only; the Unicode `is_space` fallback is deferred until the space tables
land.
*/
br_bytes_view_list_result br_bytes_fields(br_bytes_view s, br_allocator allocator);

br_bytes_result br_bytes_join(const br_bytes_view *parts,
                              size_t part_count,
                              br_bytes_view sep,
                              br_allocator allocator);
br_bytes_result
br_bytes_concat(const br_bytes_view *parts, size_t part_count, br_allocator allocator);
br_bytes_result br_bytes_repeat(br_bytes_view s, size_t count, br_allocator allocator);

/*
Split `s` around separator `sep`.

An empty separator splits on UTF-8 rune boundaries rather than being rejected.
*/
br_bytes_view_list_result
br_bytes_split(br_bytes_view s, br_bytes_view sep, br_allocator allocator);
br_bytes_view_list_result
br_bytes_split_n(br_bytes_view s, br_bytes_view sep, ptrdiff_t n, br_allocator allocator);
br_bytes_view_list_result
br_bytes_split_after(br_bytes_view s, br_bytes_view sep, br_allocator allocator);
br_bytes_view_list_result
br_bytes_split_after_n(br_bytes_view s, br_bytes_view sep, ptrdiff_t n, br_allocator allocator);

/*
Allocation-free split iterator.

`br_bytes_split_iter` is a caller-owned cursor advanced by
`br_bytes_split_iter_next`. Each call writes the next field through `out` and
returns true; when the input is exhausted it returns false and leaves `out`
unmodified, so callers must check the return before reading `out`.

The iterator yields exactly the sequence `br_bytes_split` would produce for the
same `(s, sep)`, element for element -- it is the allocation-free form of the
split list. In particular it KEEPS the trailing empty field (`"a,"` split on
`","` yields `"a"` then `""`) and iterating an empty input against a non-empty
separator yields one empty field. Callers wanting empties skipped use
`br_bytes_fields`. An empty separator walks one UTF-8 rune per step.

`br_bytes_split_after_iter_next` is the same iterator except each field keeps
its trailing separator, mirroring `br_bytes_split_after`.
*/
typedef struct br_bytes_split_iter {
  br_bytes_view rest;
  br_bytes_view sep;
  bool done;
} br_bytes_split_iter;

static inline br_bytes_split_iter br_bytes_split_iter_make(br_bytes_view s, br_bytes_view sep) {
  br_bytes_split_iter it;

  it.rest = s;
  it.sep = sep;
  it.done = false;
  return it;
}

bool br_bytes_split_iter_next(br_bytes_split_iter *it, br_bytes_view *out);
bool br_bytes_split_after_iter_next(br_bytes_split_iter *it, br_bytes_view *out);

/*
Replace up to `n` occurrences of `old_bytes` with `new_bytes`.

If no rewrite is needed, `allocated` will be false and `value` will alias the
original input. If a rewrite allocates, `allocated` will be true, `owned` will
hold the allocation, and `value` will view that owned storage.

When `old_bytes` is empty, replacements are inserted at UTF-8 rune boundaries.
*/
br_bytes_rewrite_result br_bytes_replace(br_bytes_view s,
                                         br_bytes_view old_bytes,
                                         br_bytes_view new_bytes,
                                         ptrdiff_t n,
                                         br_allocator allocator);
br_bytes_rewrite_result br_bytes_replace_all(br_bytes_view s,
                                             br_bytes_view old_bytes,
                                             br_bytes_view new_bytes,
                                             br_allocator allocator);
br_bytes_rewrite_result
br_bytes_remove(br_bytes_view s, br_bytes_view key, ptrdiff_t n, br_allocator allocator);
br_bytes_rewrite_result
br_bytes_remove_all(br_bytes_view s, br_bytes_view key, br_allocator allocator);

BR_EXTERN_C_END

#endif
