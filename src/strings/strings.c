#include <bedrock/strings/strings.h>

static br_string_result br__string_result(void *data, usize len, br_status status) {
  br_string_result result;

  result.value = br_string_make(data, len);
  result.status = status;
  return result;
}

static br_string_view_list_result
br__string_view_list_result(br_string_view *data, usize len, br_status status) {
  br_string_view_list_result result;

  result.value.data = data;
  result.value.len = len;
  result.status = status;
  return result;
}

static br_string_rewrite_result br__string_rewrite_alias_result(br_string_view value) {
  br_string_rewrite_result result;

  result.value = value;
  result.owned = br_string_make(NULL, 0u);
  result.allocated = false;
  result.status = BR_STATUS_OK;
  return result;
}

static br_string_rewrite_result
br__string_rewrite_owned_result(void *data, usize len, br_status status) {
  br_string_rewrite_result result;

  result.owned = br_string_make(data, len);
  result.value = br_string_view_from_string(result.owned);
  result.allocated = status == BR_STATUS_OK;
  result.status = status;
  return result;
}

static bool br__string_add_overflow(usize lhs, usize rhs, usize *out) {
  if (lhs > SIZE_MAX - rhs) {
    return true;
  }

  *out = lhs + rhs;
  return false;
}

br_status br_string_free(br_string string, br_allocator allocator) {
  return br_allocator_free(allocator, string.data, string.len);
}

br_status br_string_view_list_free(br_string_view_list list, br_allocator allocator) {
  return br_allocator_free(allocator, list.data, list.len * sizeof(br_string_view));
}

br_status br_string_rewrite_free(br_string_rewrite_result result, br_allocator allocator) {
  if (!result.allocated) {
    return BR_STATUS_OK;
  }
  return br_string_free(result.owned, allocator);
}

br_string_result br_string_clone(br_string_view s, br_allocator allocator) {
  br_bytes_result result;

  result = br_bytes_clone(br_string_view_as_bytes(s), allocator);
  return br__string_result(result.value.data, result.value.len, result.status);
}

br_string_result br_string_to_lower_ascii(br_string_view s, br_allocator allocator) {
  br_bytes_result result = br_bytes_to_lower_ascii(br_string_view_as_bytes(s), allocator);

  return br__string_result(result.value.data, result.value.len, result.status);
}

br_string_result br_string_to_upper_ascii(br_string_view s, br_allocator allocator) {
  br_bytes_result result = br_bytes_to_upper_ascii(br_string_view_as_bytes(s), allocator);

  return br__string_result(result.value.data, result.value.len, result.status);
}

i32 br_string_compare(br_string_view lhs, br_string_view rhs) {
  return br_bytes_compare(br_string_view_as_bytes(lhs), br_string_view_as_bytes(rhs));
}

bool br_string_equal(br_string_view lhs, br_string_view rhs) {
  return br_bytes_equal(br_string_view_as_bytes(lhs), br_string_view_as_bytes(rhs));
}

bool br_string_has_prefix(br_string_view s, br_string_view prefix) {
  return br_bytes_has_prefix(br_string_view_as_bytes(s), br_string_view_as_bytes(prefix));
}

bool br_string_has_suffix(br_string_view s, br_string_view suffix) {
  return br_bytes_has_suffix(br_string_view_as_bytes(s), br_string_view_as_bytes(suffix));
}

bool br_string_contains(br_string_view s, br_string_view needle) {
  return br_bytes_contains(br_string_view_as_bytes(s), br_string_view_as_bytes(needle));
}

bool br_string_contains_any(br_string_view s, br_string_view chars) {
  return br_string_index_any(s, chars) >= 0;
}

bool br_string_contains_rune(br_string_view s, br_rune value) {
  return br_string_index_rune(s, value) >= 0;
}

bool br_string_valid(br_string_view s) {
  return br_utf8_valid(br_string_view_as_bytes(s));
}

isize br_string_index(br_string_view s, br_string_view needle) {
  return br_bytes_index(br_string_view_as_bytes(s), br_string_view_as_bytes(needle));
}

isize br_string_index_byte(br_string_view s, u8 byte_value) {
  return br_bytes_index_byte(br_string_view_as_bytes(s), byte_value);
}

isize br_string_index_rune(br_string_view s, br_rune value) {
  br_utf8_encode_result encoded;
  br_string_view encoded_view;
  usize offset = 0u;

  if ((u32)value < (u32)BR_RUNE_SELF) {
    return br_string_index_byte(s, (u8)value);
  }
  if (value == BR_RUNE_ERROR) {
    while (offset < s.len) {
      br_utf8_decode_result decoded;

      decoded = br_utf8_decode(br_bytes_view_make(s.data + offset, s.len - offset));
      if (decoded.value == BR_RUNE_ERROR) {
        return (isize)offset;
      }
      if (decoded.width == 0u) {
        break;
      }
      offset += decoded.width;
    }
    return -1;
  }
  if (!br_utf8_valid_rune(value)) {
    return -1;
  }

  encoded = br_utf8_encode(value);
  encoded_view = br_string_view_make(encoded.bytes, encoded.len);
  return br_string_index(s, encoded_view);
}

isize br_string_last_index(br_string_view s, br_string_view needle) {
  return br_bytes_last_index(br_string_view_as_bytes(s), br_string_view_as_bytes(needle));
}

isize br_string_last_index_byte(br_string_view s, u8 byte_value) {
  return br_bytes_last_index_byte(br_string_view_as_bytes(s), byte_value);
}

isize br_string_index_any(br_string_view s, br_string_view chars) {
  usize offset = 0u;

  if (chars.len == 0u) {
    return -1;
  }
  if (chars.len == 1u) {
    br_rune value = (u8)chars.data[0];

    if (value >= BR_RUNE_SELF) {
      value = BR_RUNE_ERROR;
    }
    return br_string_index_rune(s, value);
  }

  while (offset < s.len) {
    br_utf8_decode_result decoded;

    decoded = br_utf8_decode(br_bytes_view_make(s.data + offset, s.len - offset));
    if (br_string_index_rune(chars, decoded.value) >= 0) {
      return (isize)offset;
    }
    if (decoded.width == 0u) {
      break;
    }
    offset += decoded.width;
  }

  return -1;
}

isize br_string_last_index_any(br_string_view s, br_string_view chars) {
  usize end = s.len;

  if (chars.len == 0u) {
    return -1;
  }

  while (end > 0u) {
    br_utf8_decode_result decoded;
    usize offset;

    decoded = br_utf8_decode_last(br_bytes_view_make(s.data, end));
    if (decoded.width == 0u) {
      break;
    }
    offset = end - decoded.width;
    if (br_string_index_rune(chars, decoded.value) >= 0) {
      return (isize)offset;
    }
    end = offset;
  }

  return -1;
}

usize br_string_count(br_string_view s, br_string_view needle) {
  if (needle.len == 0u) {
    return br_string_rune_count(s) + 1u;
  }
  return (usize)br_bytes_count(br_string_view_as_bytes(s), br_string_view_as_bytes(needle));
}

usize br_string_rune_count(br_string_view s) {
  return br_utf8_rune_count(br_string_view_as_bytes(s));
}

br_string_result br_string_join(const br_string_view *parts,
                                usize part_count,
                                br_string_view sep,
                                br_allocator allocator) {
  br_bytes_result result;

  result = br_bytes_join(
    (const br_bytes_view *)parts, part_count, br_string_view_as_bytes(sep), allocator);
  return br__string_result(result.value.data, result.value.len, result.status);
}

br_string_result
br_string_concat(const br_string_view *parts, usize part_count, br_allocator allocator) {
  br_bytes_result result;

  result = br_bytes_concat((const br_bytes_view *)parts, part_count, allocator);
  return br__string_result(result.value.data, result.value.len, result.status);
}

br_string_result br_string_repeat(br_string_view s, usize count, br_allocator allocator) {
  br_bytes_result result;

  result = br_bytes_repeat(br_string_view_as_bytes(s), count, allocator);
  return br__string_result(result.value.data, result.value.len, result.status);
}

static br_string_view_list_result br__string_split_impl(
  br_string_view s, br_string_view sep, usize sep_save, isize n, br_allocator allocator) {
  br_alloc_result alloc;
  br_string_view *parts;
  usize target_count;
  usize part_index = 0u;

  if (n == 0) {
    return br__string_view_list_result(NULL, 0u, BR_STATUS_OK);
  }

  if (sep.len == 0u) {
    target_count = br_string_rune_count(s);
    if (n >= 0 && (usize)n < target_count) {
      target_count = (usize)n;
    }
    if (target_count == 0u) {
      return br__string_view_list_result(NULL, 0u, BR_STATUS_OK);
    }

    alloc = br_allocator_alloc_uninit(
      allocator, target_count * sizeof(br_string_view), _Alignof(br_string_view));
    if (alloc.status != BR_STATUS_OK) {
      return br__string_view_list_result(NULL, 0u, alloc.status);
    }

    parts = alloc.ptr;
    for (; part_index + 1u < target_count; ++part_index) {
      br_utf8_decode_result decoded;

      decoded = br_utf8_decode(br_string_view_as_bytes(s));
      if (decoded.width == 0u) {
        break;
      }
      parts[part_index] = br_string_view_make(s.data, decoded.width);
      s = br_string_view_make(s.data + decoded.width, s.len - decoded.width);
    }
    parts[part_index] = s;
    return br__string_view_list_result(parts, part_index + 1u, BR_STATUS_OK);
  }

  if (n < 0) {
    target_count = br_string_count(s, sep) + 1u;
  } else {
    target_count = (usize)n;
  }

  if (target_count == 0u) {
    return br__string_view_list_result(NULL, 0u, BR_STATUS_OK);
  }

  alloc = br_allocator_alloc_uninit(
    allocator, target_count * sizeof(br_string_view), _Alignof(br_string_view));
  if (alloc.status != BR_STATUS_OK) {
    return br__string_view_list_result(NULL, 0u, alloc.status);
  }

  parts = alloc.ptr;
  if (target_count > 0u) {
    usize remaining_slots = target_count - 1u;

    while (part_index < remaining_slots) {
      isize split_at = br_string_index(s, sep);
      if (split_at < 0) {
        break;
      }

      parts[part_index] = br_string_view_make(s.data, (usize)split_at + sep_save);
      s = br_string_view_make(s.data + (usize)split_at + sep.len,
                              s.len - ((usize)split_at + sep.len));
      part_index += 1u;
    }
  }

  parts[part_index] = s;
  return br__string_view_list_result(parts, part_index + 1u, BR_STATUS_OK);
}

br_string_view_list_result
br_string_split(br_string_view s, br_string_view sep, br_allocator allocator) {
  return br__string_split_impl(s, sep, 0u, -1, allocator);
}

br_string_view_list_result
br_string_split_n(br_string_view s, br_string_view sep, isize n, br_allocator allocator) {
  return br__string_split_impl(s, sep, 0u, n, allocator);
}

br_string_view_list_result
br_string_split_after(br_string_view s, br_string_view sep, br_allocator allocator) {
  return br__string_split_impl(s, sep, sep.len, -1, allocator);
}

br_string_view_list_result
br_string_split_after_n(br_string_view s, br_string_view sep, isize n, br_allocator allocator) {
  return br__string_split_impl(s, sep, sep.len, n, allocator);
}

static bool br__string_byte_is_ascii_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

br_string_view_list_result br_string_fields(br_string_view s, br_allocator allocator) {
  br_alloc_result alloc;
  br_string_view *parts;
  usize field_count = 0u;
  usize i = 0u;
  usize part_index = 0u;
  bool in_field = false;

  /* First pass: count fields so the output list is sized exactly. */
  for (usize k = 0u; k < s.len; k += 1u) {
    if (br__string_byte_is_ascii_space(s.data[k])) {
      in_field = false;
    } else if (!in_field) {
      in_field = true;
      field_count += 1u;
    }
  }

  if (field_count == 0u) {
    return br__string_view_list_result(NULL, 0u, BR_STATUS_OK);
  }

  alloc = br_allocator_alloc_uninit(
    allocator, field_count * sizeof(br_string_view), _Alignof(br_string_view));
  if (alloc.status != BR_STATUS_OK) {
    return br__string_view_list_result(NULL, 0u, alloc.status);
  }
  parts = alloc.ptr;

  /* Second pass: record each field as a sub-view of the input. */
  while (i < s.len) {
    usize start;

    while (i < s.len && br__string_byte_is_ascii_space(s.data[i])) {
      i += 1u;
    }
    if (i >= s.len) {
      break;
    }
    start = i;
    while (i < s.len && !br__string_byte_is_ascii_space(s.data[i])) {
      i += 1u;
    }
    parts[part_index] = br_string_view_make(s.data + start, i - start);
    part_index += 1u;
  }

  return br__string_view_list_result(parts, part_index, BR_STATUS_OK);
}

/* Produce the next split field, matching br__string_split_impl element-for-element.
   `sep_save` is 0 for split or sep.len for split_after. Returns false once the
   final remainder has been yielded, leaving `out` untouched. */
static bool
br__string_split_iter_step(br_string_split_iter *it, usize sep_save, br_string_view *out) {
  if (it->done) {
    return false;
  }

  if (it->sep.len == 0u) {
    br_utf8_decode_result decoded;

    if (it->rest.len == 0u) {
      it->done = true;
      return false;
    }
    decoded = br_utf8_decode(br_string_view_as_bytes(it->rest));
    if (decoded.width == 0u) {
      it->done = true;
      return false;
    }
    *out = br_string_view_make(it->rest.data, decoded.width);
    it->rest = br_string_view_make(it->rest.data + decoded.width, it->rest.len - decoded.width);
    if (it->rest.len == 0u) {
      it->done = true;
    }
    return true;
  }

  {
    isize split_at = br_string_index(it->rest, it->sep);

    if (split_at < 0) {
      *out = it->rest;
      it->done = true;
      return true;
    }

    *out = br_string_view_make(it->rest.data, (usize)split_at + sep_save);
    it->rest = br_string_view_make(it->rest.data + (usize)split_at + it->sep.len,
                                   it->rest.len - ((usize)split_at + it->sep.len));
    return true;
  }
}

bool br_string_split_iter_next(br_string_split_iter *it, br_string_view *out) {
  return br__string_split_iter_step(it, 0u, out);
}

bool br_string_split_after_iter_next(br_string_split_iter *it, br_string_view *out) {
  return br__string_split_iter_step(it, it->sep.len, out);
}

bool br_string_fields_iter_next(br_string_fields_iter *it, br_string_view *out) {
  usize i = 0u;
  usize start;

  while (i < it->rest.len && br__string_byte_is_ascii_space(it->rest.data[i])) {
    i += 1u;
  }
  if (i >= it->rest.len) {
    it->rest = br_string_view_make(it->rest.data + it->rest.len, 0u);
    return false;
  }

  start = i;
  while (i < it->rest.len && !br__string_byte_is_ascii_space(it->rest.data[i])) {
    i += 1u;
  }
  *out = br_string_view_make(it->rest.data + start, i - start);
  it->rest = br_string_view_make(it->rest.data + i, it->rest.len - i);
  return true;
}

br_string_rewrite_result br_string_replace(br_string_view s,
                                           br_string_view old_string,
                                           br_string_view new_string,
                                           isize n,
                                           br_allocator allocator) {
  br_alloc_result alloc;
  usize match_count;
  usize replace_count;
  usize output_len;
  usize start = 0u;
  usize write_offset = 0u;

  if ((old_string.len == new_string.len &&
       (old_string.len == 0u || br_string_equal(old_string, new_string))) ||
      n == 0) {
    return br__string_rewrite_alias_result(s);
  }

  match_count = br_string_count(s, old_string);
  if (match_count == 0u) {
    return br__string_rewrite_alias_result(s);
  }

  replace_count = match_count;
  if (n >= 0 && (usize)n < replace_count) {
    replace_count = (usize)n;
  }

  if (new_string.len >= old_string.len) {
    usize extra_per_match = new_string.len - old_string.len;
    usize extra_total;

    if (replace_count > 0u && extra_per_match > SIZE_MAX / replace_count) {
      return br__string_rewrite_owned_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
    extra_total = extra_per_match * replace_count;
    if (br__string_add_overflow(s.len, extra_total, &output_len)) {
      return br__string_rewrite_owned_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }
  } else {
    output_len = s.len - ((old_string.len - new_string.len) * replace_count);
  }

  if (output_len == 0u) {
    return br__string_rewrite_owned_result(NULL, 0u, BR_STATUS_OK);
  }

  alloc = br_allocator_alloc_uninit(allocator, output_len, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__string_rewrite_owned_result(NULL, 0u, alloc.status);
  }

  for (usize i = 0; i < replace_count; ++i) {
    usize split_at;
    usize next_start;

    if (old_string.len == 0u) {
      split_at = start;
      if (i > 0u && split_at < s.len) {
        br_utf8_decode_result decoded;

        decoded = br_utf8_decode(br_bytes_view_make(s.data + start, s.len - start));
        if (decoded.width > 0u) {
          split_at += decoded.width;
        }
      }
      next_start = split_at;
    } else {
      isize local_index =
        br_string_index(br_string_view_make(s.data + start, s.len - start), old_string);
      split_at = start + (usize)local_index;
      next_start = split_at + old_string.len;
    }

    if (split_at > start) {
      memcpy((u8 *)alloc.ptr + write_offset, s.data + start, split_at - start);
      write_offset += split_at - start;
    }
    if (new_string.len > 0u) {
      memcpy((u8 *)alloc.ptr + write_offset, new_string.data, new_string.len);
      write_offset += new_string.len;
    }
    start = next_start;
  }

  if (start < s.len) {
    memcpy((u8 *)alloc.ptr + write_offset, s.data + start, s.len - start);
    write_offset += s.len - start;
  }

  return br__string_rewrite_owned_result(alloc.ptr, write_offset, BR_STATUS_OK);
}

br_string_rewrite_result br_string_replace_all(br_string_view s,
                                               br_string_view old_string,
                                               br_string_view new_string,
                                               br_allocator allocator) {
  return br_string_replace(s, old_string, new_string, -1, allocator);
}

br_string_rewrite_result
br_string_remove(br_string_view s, br_string_view key, isize n, br_allocator allocator) {
  return br_string_replace(s, key, br_string_view_make(NULL, 0u), n, allocator);
}

br_string_rewrite_result
br_string_remove_all(br_string_view s, br_string_view key, br_allocator allocator) {
  return br_string_remove(s, key, -1, allocator);
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

/* Byte offset of rune index `rune_index` in `s`. If the string has fewer runes,
   returns s.len and sets *in_bounds to false; otherwise sets it true. */
static usize br__string_rune_offset(br_string_view s, usize rune_index, bool *in_bounds) {
  usize offset = 0u;
  usize count = 0u;

  while (offset < s.len) {
    br_utf8_decode_result decoded;

    if (count == rune_index) {
      *in_bounds = true;
      return offset;
    }
    decoded = br_utf8_decode(br_bytes_view_make(s.data + offset, s.len - offset));
    offset += decoded.width > 0u ? decoded.width : 1u;
    count += 1u;
  }

  /* Reaching the end is in bounds only when the index lands exactly there. */
  *in_bounds = (count == rune_index);
  return s.len;
}

br_string_view br_string_cut(br_string_view s, usize rune_offset, usize rune_length) {
  bool in_bounds;
  usize start = br__string_rune_offset(s, rune_offset, &in_bounds);
  br_string_view rest = br_string_view_make(s.data + start, s.len - start);
  usize end;

  if (rune_length == 0u) {
    return rest;
  }

  end = br__string_rune_offset(rest, rune_length, &in_bounds);
  return br_string_view_make(rest.data, end);
}

br_string_view br_string_substring(br_string_view s, usize rune_start, usize rune_end, bool *ok) {
  bool start_ok;
  bool end_ok;
  usize start_off;
  usize end_off;

  if (rune_end < rune_start) {
    if (ok != NULL) {
      *ok = false;
    }
    return br_string_view_make(s.data, 0u);
  }

  start_off = br__string_rune_offset(s, rune_start, &start_ok);
  end_off = br__string_rune_offset(s, rune_end, &end_ok);
  if (!start_ok || !end_ok) {
    if (ok != NULL) {
      *ok = false;
    }
    return br_string_view_make(s.data, 0u);
  }

  if (ok != NULL) {
    *ok = true;
  }
  return br_string_view_make(s.data + start_off, end_off - start_off);
}

size_t br_string_prefix_length(br_string_view a, br_string_view b) {
  usize common = br_min_size(a.len, b.len);
  usize i = 0u;
  usize last_boundary = 0u;

  /* Walk runes of `a` in lockstep with `b`, stopping at the first rune that
     differs or that would exceed the shared byte length. Returns the byte
     length up to the last whole matching rune. */
  while (i < common) {
    br_utf8_decode_result da = br_utf8_decode(br_bytes_view_make(a.data + i, a.len - i));
    usize width = da.width > 0u ? da.width : 1u;

    if (i + width > common || memcmp(a.data + i, b.data + i, width) != 0) {
      break;
    }
    i += width;
    last_boundary = i;
  }

  return last_boundary;
}

br_string_view br_string_common_prefix(br_string_view a, br_string_view b) {
  return br_string_view_make(a.data, br_string_prefix_length(a, b));
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

br_string_view br_string_trim_left(br_string_view s, br_string_view cutset) {
  br_bytes_view trimmed =
    br_bytes_trim_left(br_string_view_as_bytes(s), br_string_view_as_bytes(cutset));

  return br_string_view_make(trimmed.data, trimmed.len);
}

br_string_view br_string_trim_right(br_string_view s, br_string_view cutset) {
  br_bytes_view trimmed =
    br_bytes_trim_right(br_string_view_as_bytes(s), br_string_view_as_bytes(cutset));

  return br_string_view_make(trimmed.data, trimmed.len);
}

br_string_view br_string_trim(br_string_view s, br_string_view cutset) {
  br_bytes_view trimmed =
    br_bytes_trim(br_string_view_as_bytes(s), br_string_view_as_bytes(cutset));

  return br_string_view_make(trimmed.data, trimmed.len);
}

br_string_view br_string_trim_left_space(br_string_view s) {
  br_bytes_view trimmed = br_bytes_trim_left_space(br_string_view_as_bytes(s));

  return br_string_view_make(trimmed.data, trimmed.len);
}

br_string_view br_string_trim_right_space(br_string_view s) {
  br_bytes_view trimmed = br_bytes_trim_right_space(br_string_view_as_bytes(s));

  return br_string_view_make(trimmed.data, trimmed.len);
}

br_string_view br_string_trim_space(br_string_view s) {
  br_bytes_view trimmed = br_bytes_trim_space(br_string_view_as_bytes(s));

  return br_string_view_make(trimmed.data, trimmed.len);
}

br_string_view br_string_trim_left_null(br_string_view s) {
  br_bytes_view trimmed = br_bytes_trim_left_null(br_string_view_as_bytes(s));

  return br_string_view_make(trimmed.data, trimmed.len);
}

br_string_view br_string_trim_right_null(br_string_view s) {
  br_bytes_view trimmed = br_bytes_trim_right_null(br_string_view_as_bytes(s));

  return br_string_view_make(trimmed.data, trimmed.len);
}

br_string_view br_string_trim_null(br_string_view s) {
  br_bytes_view trimmed = br_bytes_trim_null(br_string_view_as_bytes(s));

  return br_string_view_make(trimmed.data, trimmed.len);
}
