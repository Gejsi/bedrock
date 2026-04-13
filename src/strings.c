#include <bedrock/strings.h>

static br_string_result br__string_result(void *data, usize len, br_status status) {
    br_string_result result;

    result.value = br_string_make(data, len);
    result.status = status;
    return result;
}

br_status br_string_free(br_string string, br_allocator allocator) {
    return br_allocator_free(allocator, string.data, string.len);
}

br_string_result br_string_clone(br_string_view s, br_allocator allocator) {
    br_bytes_result result;

    result = br_bytes_clone(br_string_view_as_bytes(s), allocator);
    return br__string_result(result.value.data, result.value.len, result.status);
}

int br_string_compare(br_string_view lhs, br_string_view rhs) {
    return br_bytes_compare(br_string_view_as_bytes(lhs), br_string_view_as_bytes(rhs));
}

int br_string_equal(br_string_view lhs, br_string_view rhs) {
    return br_bytes_equal(br_string_view_as_bytes(lhs), br_string_view_as_bytes(rhs));
}

int br_string_has_prefix(br_string_view s, br_string_view prefix) {
    return br_bytes_has_prefix(br_string_view_as_bytes(s), br_string_view_as_bytes(prefix));
}

int br_string_has_suffix(br_string_view s, br_string_view suffix) {
    return br_bytes_has_suffix(br_string_view_as_bytes(s), br_string_view_as_bytes(suffix));
}

int br_string_contains(br_string_view s, br_string_view needle) {
    return br_bytes_contains(br_string_view_as_bytes(s), br_string_view_as_bytes(needle));
}

int br_string_contains_rune(br_string_view s, br_rune value) {
    return br_string_index_rune(s, value) >= 0;
}

int br_string_valid(br_string_view s) {
    return br_utf8_valid(br_string_view_as_bytes(s));
}

isize br_string_index(br_string_view s, br_string_view needle) {
    return br_bytes_index(br_string_view_as_bytes(s), br_string_view_as_bytes(needle));
}

isize br_string_index_rune(br_string_view s, br_rune value) {
    usize offset = 0u;

    while (offset < s.len) {
        br_utf8_decode_result decoded;

        decoded = br_utf8_decode(br_bytes_view_make(s.data + offset, s.len - offset));
        if (decoded.value == value) {
            return (isize)offset;
        }
        if (decoded.width == 0u) {
            break;
        }
        offset += decoded.width;
    }

    return -1;
}

usize br_string_rune_count(br_string_view s) {
    return br_utf8_rune_count(br_string_view_as_bytes(s));
}

br_string_view br_string_truncate_to_byte(br_string_view s, u8 byte_value) {
    br_bytes_view truncated;

    truncated = br_bytes_truncate_to_byte(br_string_view_as_bytes(s), byte_value);
    return br_string_view_make(truncated.data, truncated.len);
}

br_string_view br_string_truncate_to_rune(br_string_view s, br_rune value) {
    isize index = br_string_index_rune(s, value);

    if (index < 0) {
        return s;
    }
    return br_string_view_make(s.data, (usize)index);
}

br_string_view br_string_trim_prefix(br_string_view s, br_string_view prefix) {
    br_bytes_view trimmed;

    trimmed = br_bytes_trim_prefix(br_string_view_as_bytes(s), br_string_view_as_bytes(prefix));
    return br_string_view_make(trimmed.data, trimmed.len);
}

br_string_view br_string_trim_suffix(br_string_view s, br_string_view suffix) {
    br_bytes_view trimmed;

    trimmed = br_bytes_trim_suffix(br_string_view_as_bytes(s), br_string_view_as_bytes(suffix));
    return br_string_view_make(trimmed.data, trimmed.len);
}
