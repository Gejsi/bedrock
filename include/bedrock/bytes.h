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
br_bytes_result br_bytes_clone(br_bytes_view src, br_allocator allocator);

int br_bytes_compare(br_bytes_view lhs, br_bytes_view rhs);
int br_bytes_equal(br_bytes_view lhs, br_bytes_view rhs);
int br_bytes_has_prefix(br_bytes_view s, br_bytes_view prefix);
int br_bytes_has_suffix(br_bytes_view s, br_bytes_view suffix);
int br_bytes_contains(br_bytes_view s, br_bytes_view needle);
int br_bytes_contains_any(br_bytes_view s, br_bytes_view chars);

isize br_bytes_index_byte(br_bytes_view s, u8 byte_value);
isize br_bytes_last_index_byte(br_bytes_view s, u8 byte_value);
isize br_bytes_index(br_bytes_view s, br_bytes_view needle);
isize br_bytes_last_index(br_bytes_view s, br_bytes_view needle);
isize br_bytes_index_any(br_bytes_view s, br_bytes_view chars);

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

BR_EXTERN_C_END

#endif
