#include <bedrock/bytes.h>

static br_bytes_result br__bytes_result(void *data, usize len, br_status status) {
    br_bytes_result result;

    result.value = br_bytes_make(data, len);
    result.status = status;
    return result;
}

static br_bytes_view_list_result br__bytes_view_list_result(
    br_bytes_view *data,
    usize len,
    br_status status
) {
    br_bytes_view_list_result result;

    result.value.data = data;
    result.value.len = len;
    result.status = status;
    return result;
}

static br_bytes_rewrite_result br__bytes_rewrite_alias_result(br_bytes_view value) {
    br_bytes_rewrite_result result;

    result.value = value;
    result.owned = br_bytes_make(NULL, 0u);
    result.allocated = 0;
    result.status = BR_STATUS_OK;
    return result;
}

static br_bytes_rewrite_result br__bytes_rewrite_owned_result(
    void *data,
    usize len,
    br_status status
) {
    br_bytes_rewrite_result result;

    result.owned = br_bytes_make(data, len);
    result.value = br_bytes_view_from_bytes(result.owned);
    result.allocated = status == BR_STATUS_OK;
    result.status = status;
    return result;
}

static int br__bytes_contains_byte(br_bytes_view chars, u8 byte_value) {
    for (usize i = 0; i < chars.len; ++i) {
        if (chars.data[i] == byte_value) {
            return 1;
        }
    }

    return 0;
}

static int br__bytes_add_overflow(usize lhs, usize rhs, usize *out) {
    if (lhs > SIZE_MAX - rhs) {
        return 1;
    }

    *out = lhs + rhs;
    return 0;
}

br_status br_bytes_free(br_bytes bytes, br_allocator allocator) {
    return br_allocator_free(allocator, bytes.data, bytes.len);
}

br_status br_bytes_view_list_free(br_bytes_view_list list, br_allocator allocator) {
    return br_allocator_free(allocator, list.data, list.len * sizeof(br_bytes_view));
}

br_status br_bytes_rewrite_free(br_bytes_rewrite_result result, br_allocator allocator) {
    if (!result.allocated) {
        return BR_STATUS_OK;
    }
    return br_bytes_free(result.owned, allocator);
}

br_bytes_result br_bytes_clone(br_bytes_view src, br_allocator allocator) {
    br_alloc_result alloc;

    if (src.len == 0u) {
        return br__bytes_result(NULL, 0u, BR_STATUS_OK);
    }

    alloc = br_allocator_alloc_uninit(allocator, src.len, 1u);
    if (alloc.status != BR_STATUS_OK) {
        return br__bytes_result(NULL, 0u, alloc.status);
    }

    memcpy(alloc.ptr, src.data, src.len);
    return br__bytes_result(alloc.ptr, src.len, BR_STATUS_OK);
}

int br_bytes_compare(br_bytes_view lhs, br_bytes_view rhs) {
    usize common_len = br_min_size(lhs.len, rhs.len);
    int cmp;

    if (common_len == 0u) {
        if (lhs.len == rhs.len) {
            return 0;
        }
        return lhs.len < rhs.len ? -1 : 1;
    }

    cmp = memcmp(lhs.data, rhs.data, common_len);
    if (cmp < 0) {
        return -1;
    }
    if (cmp > 0) {
        return 1;
    }
    if (lhs.len == rhs.len) {
        return 0;
    }
    return lhs.len < rhs.len ? -1 : 1;
}

int br_bytes_equal(br_bytes_view lhs, br_bytes_view rhs) {
    if (lhs.len != rhs.len) {
        return 0;
    }
    if (lhs.len == 0u) {
        return 1;
    }
    return memcmp(lhs.data, rhs.data, lhs.len) == 0;
}

int br_bytes_has_prefix(br_bytes_view s, br_bytes_view prefix) {
    if (prefix.len > s.len) {
        return 0;
    }
    if (prefix.len == 0u) {
        return 1;
    }
    return memcmp(s.data, prefix.data, prefix.len) == 0;
}

int br_bytes_has_suffix(br_bytes_view s, br_bytes_view suffix) {
    if (suffix.len > s.len) {
        return 0;
    }
    if (suffix.len == 0u) {
        return 1;
    }
    return memcmp(s.data + (s.len - suffix.len), suffix.data, suffix.len) == 0;
}

int br_bytes_contains(br_bytes_view s, br_bytes_view needle) {
    return br_bytes_index(s, needle) >= 0;
}

int br_bytes_contains_any(br_bytes_view s, br_bytes_view chars) {
    return br_bytes_index_any(s, chars) >= 0;
}

isize br_bytes_index_byte(br_bytes_view s, u8 byte_value) {
    const void *ptr = memchr(s.data, byte_value, s.len);

    if (ptr == NULL) {
        return -1;
    }

    return (isize)((const u8 *)ptr - s.data);
}

isize br_bytes_last_index_byte(br_bytes_view s, u8 byte_value) {
    for (usize i = s.len; i > 0u; --i) {
        if (s.data[i - 1u] == byte_value) {
            return (isize)(i - 1u);
        }
    }

    return -1;
}

isize br_bytes_index(br_bytes_view s, br_bytes_view needle) {
    if (needle.len == 0u) {
        return 0;
    }
    if (needle.len == 1u) {
        return br_bytes_index_byte(s, needle.data[0]);
    }
    if (needle.len > s.len) {
        return -1;
    }
    if (needle.len == s.len) {
        return br_bytes_equal(s, needle) ? 0 : -1;
    }

    for (usize i = 0; i <= s.len - needle.len; ++i) {
        if (s.data[i] == needle.data[0] &&
            memcmp(s.data + i, needle.data, needle.len) == 0) {
            return (isize)i;
        }
    }

    return -1;
}

isize br_bytes_last_index(br_bytes_view s, br_bytes_view needle) {
    if (needle.len == 0u) {
        return (isize)s.len;
    }
    if (needle.len == 1u) {
        return br_bytes_last_index_byte(s, needle.data[0]);
    }
    if (needle.len > s.len) {
        return -1;
    }
    if (needle.len == s.len) {
        return br_bytes_equal(s, needle) ? 0 : -1;
    }

    for (usize i = s.len - needle.len + 1u; i > 0u; --i) {
        usize pos = i - 1u;
        if (s.data[pos] == needle.data[0] &&
            memcmp(s.data + pos, needle.data, needle.len) == 0) {
            return (isize)pos;
        }
    }

    return -1;
}

isize br_bytes_index_any(br_bytes_view s, br_bytes_view chars) {
    if (chars.len == 0u) {
        return -1;
    }

    for (usize i = 0; i < s.len; ++i) {
        if (br__bytes_contains_byte(chars, s.data[i])) {
            return (isize)i;
        }
    }

    return -1;
}

isize br_bytes_count(br_bytes_view s, br_bytes_view needle) {
    isize count = 0;
    isize index;

    if (needle.len == 0u) {
        return (isize)s.len + 1;
    }
    if (needle.len == 1u) {
        for (usize i = 0; i < s.len; ++i) {
            if (s.data[i] == needle.data[0]) {
                count += 1;
            }
        }
        return count;
    }

    while ((index = br_bytes_index(s, needle)) >= 0) {
        count += 1;
        s = br_bytes_view_make(
            s.data + (usize)index + needle.len,
            s.len - ((usize)index + needle.len)
        );
    }

    return count;
}

br_bytes_view br_bytes_truncate_to_byte(br_bytes_view s, u8 byte_value) {
    isize index = br_bytes_index_byte(s, byte_value);

    if (index < 0) {
        return s;
    }

    return br_bytes_view_make(s.data, (usize)index);
}

br_bytes_view br_bytes_trim_prefix(br_bytes_view s, br_bytes_view prefix) {
    if (br_bytes_has_prefix(s, prefix)) {
        return br_bytes_view_make(s.data + prefix.len, s.len - prefix.len);
    }

    return s;
}

br_bytes_view br_bytes_trim_suffix(br_bytes_view s, br_bytes_view suffix) {
    if (br_bytes_has_suffix(s, suffix)) {
        return br_bytes_view_make(s.data, s.len - suffix.len);
    }

    return s;
}

br_bytes_result br_bytes_join(
    const br_bytes_view *parts,
    usize part_count,
    br_bytes_view sep,
    br_allocator allocator
) {
    br_alloc_result alloc;
    usize total = 0u;
    usize offset = 0u;

    if (part_count == 0u) {
        return br__bytes_result(NULL, 0u, BR_STATUS_OK);
    }
    if (parts == NULL) {
        return br__bytes_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
    }

    for (usize i = 0; i < part_count; ++i) {
        if (br__bytes_add_overflow(total, parts[i].len, &total)) {
            return br__bytes_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
        }
    }

    if (part_count > 1u) {
        usize sep_total;

        if (sep.len > 0u && (part_count - 1u) > (SIZE_MAX / sep.len)) {
            return br__bytes_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
        }
        sep_total = sep.len * (part_count - 1u);
        if (br__bytes_add_overflow(total, sep_total, &total)) {
            return br__bytes_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
        }
    }

    if (total == 0u) {
        return br__bytes_result(NULL, 0u, BR_STATUS_OK);
    }

    alloc = br_allocator_alloc_uninit(allocator, total, 1u);
    if (alloc.status != BR_STATUS_OK) {
        return br__bytes_result(NULL, 0u, alloc.status);
    }

    for (usize i = 0; i < part_count; ++i) {
        if (i > 0u && sep.len > 0u) {
            memcpy((u8 *)alloc.ptr + offset, sep.data, sep.len);
            offset += sep.len;
        }
        if (parts[i].len > 0u) {
            memcpy((u8 *)alloc.ptr + offset, parts[i].data, parts[i].len);
            offset += parts[i].len;
        }
    }

    return br__bytes_result(alloc.ptr, total, BR_STATUS_OK);
}

br_bytes_result br_bytes_concat(
    const br_bytes_view *parts,
    usize part_count,
    br_allocator allocator
) {
    return br_bytes_join(parts, part_count, br_bytes_view_make(NULL, 0u), allocator);
}

br_bytes_result br_bytes_repeat(br_bytes_view s, usize count, br_allocator allocator) {
    br_alloc_result alloc;
    usize total;
    usize copied;

    if (count == 0u || s.len == 0u) {
        return br__bytes_result(NULL, 0u, BR_STATUS_OK);
    }
    if (count > (SIZE_MAX / s.len)) {
        return br__bytes_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }

    total = s.len * count;
    alloc = br_allocator_alloc_uninit(allocator, total, 1u);
    if (alloc.status != BR_STATUS_OK) {
        return br__bytes_result(NULL, 0u, alloc.status);
    }

    memcpy(alloc.ptr, s.data, s.len);
    copied = s.len;
    while (copied < total) {
        usize chunk = br_min_size(copied, total - copied);
        memcpy((u8 *)alloc.ptr + copied, alloc.ptr, chunk);
        copied += chunk;
    }

    return br__bytes_result(alloc.ptr, total, BR_STATUS_OK);
}

static br_bytes_view_list_result br__bytes_split_impl(
    br_bytes_view s,
    br_bytes_view sep,
    usize sep_save,
    isize n,
    br_allocator allocator
) {
    br_alloc_result alloc;
    br_bytes_view *parts;
    usize target_count;
    usize part_index = 0u;

    if (sep.len == 0u) {
        return br__bytes_view_list_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
    }
    if (n == 0) {
        return br__bytes_view_list_result(NULL, 0u, BR_STATUS_OK);
    }

    if (n < 0) {
        target_count = (usize)br_bytes_count(s, sep) + 1u;
    } else {
        target_count = (usize)n;
    }

    if (target_count == 0u) {
        return br__bytes_view_list_result(NULL, 0u, BR_STATUS_OK);
    }

    alloc = br_allocator_alloc_uninit(
        allocator,
        target_count * sizeof(br_bytes_view),
        _Alignof(br_bytes_view)
    );
    if (alloc.status != BR_STATUS_OK) {
        return br__bytes_view_list_result(NULL, 0u, alloc.status);
    }

    parts = alloc.ptr;
    if (target_count > 0u) {
        usize remaining_slots = target_count - 1u;

        while (part_index < remaining_slots) {
            isize split_at = br_bytes_index(s, sep);
            if (split_at < 0) {
                break;
            }

            parts[part_index] = br_bytes_view_make(s.data, (usize)split_at + sep_save);
            s = br_bytes_view_make(s.data + (usize)split_at + sep.len, s.len - ((usize)split_at + sep.len));
            part_index += 1u;
        }
    }

    parts[part_index] = s;
    part_index += 1u;
    return br__bytes_view_list_result(parts, part_index, BR_STATUS_OK);
}

br_bytes_view_list_result br_bytes_split(
    br_bytes_view s,
    br_bytes_view sep,
    br_allocator allocator
) {
    return br__bytes_split_impl(s, sep, 0u, -1, allocator);
}

br_bytes_view_list_result br_bytes_split_n(
    br_bytes_view s,
    br_bytes_view sep,
    isize n,
    br_allocator allocator
) {
    return br__bytes_split_impl(s, sep, 0u, n, allocator);
}

br_bytes_view_list_result br_bytes_split_after(
    br_bytes_view s,
    br_bytes_view sep,
    br_allocator allocator
) {
    return br__bytes_split_impl(s, sep, sep.len, -1, allocator);
}

br_bytes_view_list_result br_bytes_split_after_n(
    br_bytes_view s,
    br_bytes_view sep,
    isize n,
    br_allocator allocator
) {
    return br__bytes_split_impl(s, sep, sep.len, n, allocator);
}

br_bytes_rewrite_result br_bytes_replace(
    br_bytes_view s,
    br_bytes_view old_bytes,
    br_bytes_view new_bytes,
    isize n,
    br_allocator allocator
) {
    br_alloc_result alloc;
    isize match_count;
    isize replace_count;
    usize output_len;
    usize start = 0u;
    usize write_offset = 0u;

    if ((old_bytes.len == new_bytes.len &&
         (old_bytes.len == 0u ||
          (old_bytes.data != NULL &&
           new_bytes.data != NULL &&
           memcmp(old_bytes.data, new_bytes.data, old_bytes.len) == 0))) ||
        n == 0) {
        return br__bytes_rewrite_alias_result(s);
    }

    match_count = br_bytes_count(s, old_bytes);
    if (match_count == 0) {
        return br__bytes_rewrite_alias_result(s);
    }

    replace_count = match_count;
    if (n >= 0 && n < replace_count) {
        replace_count = n;
    }

    if (new_bytes.len >= old_bytes.len) {
        usize extra_per_match = new_bytes.len - old_bytes.len;
        if ((usize)replace_count > 0u && extra_per_match > SIZE_MAX / (usize)replace_count) {
            return br__bytes_rewrite_owned_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
        }
        if (s.len > SIZE_MAX - (extra_per_match * (usize)replace_count)) {
            return br__bytes_rewrite_owned_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
        }
        output_len = s.len + (extra_per_match * (usize)replace_count);
    } else {
        output_len = s.len - ((old_bytes.len - new_bytes.len) * (usize)replace_count);
    }

    if (output_len == 0u) {
        return br__bytes_rewrite_owned_result(NULL, 0u, BR_STATUS_OK);
    }

    alloc = br_allocator_alloc_uninit(allocator, output_len, 1u);
    if (alloc.status != BR_STATUS_OK) {
        return br__bytes_rewrite_owned_result(NULL, 0u, alloc.status);
    }

    for (isize i = 0; i < replace_count; ++i) {
        usize split_at;
        usize next_start;

        if (old_bytes.len == 0u) {
            split_at = start;
            if (i > 0 && split_at < s.len) {
                split_at += 1u;
            }
            next_start = split_at;
        } else {
            isize local_index = br_bytes_index(
                br_bytes_view_make(s.data + start, s.len - start),
                old_bytes
            );
            split_at = start + (usize)local_index;
            next_start = split_at + old_bytes.len;
        }

        if (split_at > start) {
            memcpy((u8 *)alloc.ptr + write_offset, s.data + start, split_at - start);
            write_offset += split_at - start;
        }
        if (new_bytes.len > 0u) {
            memcpy((u8 *)alloc.ptr + write_offset, new_bytes.data, new_bytes.len);
            write_offset += new_bytes.len;
        }
        start = next_start;
    }

    if (start < s.len) {
        memcpy((u8 *)alloc.ptr + write_offset, s.data + start, s.len - start);
        write_offset += s.len - start;
    }

    return br__bytes_rewrite_owned_result(alloc.ptr, write_offset, BR_STATUS_OK);
}

br_bytes_rewrite_result br_bytes_replace_all(
    br_bytes_view s,
    br_bytes_view old_bytes,
    br_bytes_view new_bytes,
    br_allocator allocator
) {
    return br_bytes_replace(s, old_bytes, new_bytes, -1, allocator);
}

br_bytes_rewrite_result br_bytes_remove(
    br_bytes_view s,
    br_bytes_view key,
    isize n,
    br_allocator allocator
) {
    return br_bytes_replace(s, key, br_bytes_view_make(NULL, 0u), n, allocator);
}

br_bytes_rewrite_result br_bytes_remove_all(
    br_bytes_view s,
    br_bytes_view key,
    br_allocator allocator
) {
    return br_bytes_remove(s, key, -1, allocator);
}
