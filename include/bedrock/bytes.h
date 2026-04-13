#ifndef BEDROCK_BYTES_H
#define BEDROCK_BYTES_H

#include <bedrock/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_bytes {
    u8 *data;
    usize len;
} br_bytes;

typedef struct br_bytes_view {
    const u8 *data;
    usize len;
} br_bytes_view;

typedef struct br_bytes_result {
    br_bytes value;
    br_status status;
} br_bytes_result;

typedef struct br_bytes_view_list {
    br_bytes_view *data;
    usize len;
} br_bytes_view_list;

typedef struct br_bytes_view_list_result {
    br_bytes_view_list value;
    br_status status;
} br_bytes_view_list_result;

typedef struct br_bytes_rewrite_result {
    br_bytes_view value;
    br_bytes owned;
    int allocated;
    br_status status;
} br_bytes_rewrite_result;

#define BR_BYTES_LIT(s) br_bytes_view_make((const void *)(s), sizeof(s) - 1u)

static inline br_bytes br_bytes_make(void *data, usize len) {
    br_bytes bytes;

    bytes.data = (u8 *)data;
    bytes.len = len;
    return bytes;
}

static inline br_bytes_view br_bytes_view_make(const void *data, usize len) {
    br_bytes_view bytes;

    bytes.data = (const u8 *)data;
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

int br_bytes_compare(br_bytes_view lhs, br_bytes_view rhs);
bool br_bytes_equal(br_bytes_view lhs, br_bytes_view rhs);
bool br_bytes_has_prefix(br_bytes_view s, br_bytes_view prefix);
bool br_bytes_has_suffix(br_bytes_view s, br_bytes_view suffix);
bool br_bytes_contains(br_bytes_view s, br_bytes_view needle);
bool br_bytes_contains_any(br_bytes_view s, br_bytes_view chars);

isize br_bytes_index_byte(br_bytes_view s, u8 byte_value);
isize br_bytes_last_index_byte(br_bytes_view s, u8 byte_value);
isize br_bytes_index(br_bytes_view s, br_bytes_view needle);
isize br_bytes_last_index(br_bytes_view s, br_bytes_view needle);
isize br_bytes_index_any(br_bytes_view s, br_bytes_view chars);
isize br_bytes_count(br_bytes_view s, br_bytes_view needle);

br_bytes_view br_bytes_truncate_to_byte(br_bytes_view s, u8 byte_value);
br_bytes_view br_bytes_trim_prefix(br_bytes_view s, br_bytes_view prefix);
br_bytes_view br_bytes_trim_suffix(br_bytes_view s, br_bytes_view suffix);

br_bytes_result br_bytes_join(
    const br_bytes_view *parts,
    usize part_count,
    br_bytes_view sep,
    br_allocator allocator
);
br_bytes_result br_bytes_concat(
    const br_bytes_view *parts,
    usize part_count,
    br_allocator allocator
);
br_bytes_result br_bytes_repeat(br_bytes_view s, usize count, br_allocator allocator);

/*
Split `s` around a non-empty byte separator.

This follows the broad shape of Odin's `split` family, but this first C pass is
strictly byte-oriented: an empty separator is rejected instead of triggering the
Unicode-aware rune splitting behavior Odin supports with `nil`.
*/
br_bytes_view_list_result br_bytes_split(
    br_bytes_view s,
    br_bytes_view sep,
    br_allocator allocator
);
br_bytes_view_list_result br_bytes_split_n(
    br_bytes_view s,
    br_bytes_view sep,
    isize n,
    br_allocator allocator
);
br_bytes_view_list_result br_bytes_split_after(
    br_bytes_view s,
    br_bytes_view sep,
    br_allocator allocator
);
br_bytes_view_list_result br_bytes_split_after_n(
    br_bytes_view s,
    br_bytes_view sep,
    isize n,
    br_allocator allocator
);

/*
Replace up to `n` occurrences of `old_bytes` with `new_bytes`.

If no rewrite is needed, `allocated` will be false and `value` will alias the
original input. If a rewrite allocates, `allocated` will be true, `owned` will
hold the allocation, and `value` will view that owned storage.

This first C pass is byte-oriented. When `old_bytes` is empty, replacements are
inserted at byte boundaries rather than Unicode rune boundaries.
*/
br_bytes_rewrite_result br_bytes_replace(
    br_bytes_view s,
    br_bytes_view old_bytes,
    br_bytes_view new_bytes,
    isize n,
    br_allocator allocator
);
br_bytes_rewrite_result br_bytes_replace_all(
    br_bytes_view s,
    br_bytes_view old_bytes,
    br_bytes_view new_bytes,
    br_allocator allocator
);
br_bytes_rewrite_result br_bytes_remove(
    br_bytes_view s,
    br_bytes_view key,
    isize n,
    br_allocator allocator
);
br_bytes_rewrite_result br_bytes_remove_all(
    br_bytes_view s,
    br_bytes_view key,
    br_allocator allocator
);

BR_EXTERN_C_END

#endif
