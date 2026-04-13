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

/*
Return the byte offset of the first occurrence of rune `value`, or `-1` if it
is absent.
*/
isize br_string_index_rune(br_string_view s, br_rune value);

/*
Return the number of UTF-8 runes in `s`.
*/
usize br_string_rune_count(br_string_view s);

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
