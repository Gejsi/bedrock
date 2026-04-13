#include <bedrock/strings/strings.h>

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
