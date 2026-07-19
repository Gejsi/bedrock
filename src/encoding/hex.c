#include <bedrock/encoding/hex.h>

static const u8 br__hex_lower[16] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

static const u8 br__hex_upper[16] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static const u8 *br__hex_table(br_hex_case letter_case) {
  return letter_case == BR_HEX_UPPER ? br__hex_upper : br__hex_lower;
}

/* Map one hex character to its 0-15 nibble value; false if not [0-9a-fA-F]. */
static bool br__hex_digit(u8 c, u8 *out) {
  if (c >= '0' && c <= '9') {
    *out = (u8)(c - '0');
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    *out = (u8)(c - 'a' + 10);
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    *out = (u8)(c - 'A' + 10);
    return true;
  }

  return false;
}

static br_bytes_result br__hex_bytes_result(void *data, usize len, br_status status) {
  br_bytes_result result;

  result.value = br_bytes_make(data, len);
  result.status = status;
  return result;
}

static br_decode_result
br__hex_decode_result(br_bytes value, usize error_offset, br_status status) {
  br_decode_result result;

  result.value = value;
  result.error_offset = error_offset;
  result.status = status;
  return result;
}

static br_decode_into_result
br__hex_decode_into_result(usize count, usize error_offset, br_status status) {
  br_decode_into_result result;

  result.count = count;
  result.error_offset = error_offset;
  result.status = status;
  return result;
}

br_bytes_result br_hex_encode(br_bytes_view src, br_hex_case letter_case, br_allocator allocator) {
  const u8 *table = br__hex_table(letter_case);
  br_alloc_result alloc;
  u8 *dst;
  usize out_len;

  if (src.len == 0u) {
    return br__hex_bytes_result(NULL, 0u, BR_STATUS_OK);
  }

  /* out_len = src.len * 2; guard the (practically unreachable) overflow. */
  if (src.len > SIZE_MAX / 2u) {
    return br__hex_bytes_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }
  out_len = src.len * 2u;

  alloc = br_allocator_alloc_uninit(allocator, out_len, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__hex_bytes_result(NULL, 0u, alloc.status);
  }

  dst = (u8 *)alloc.ptr;
  for (usize i = 0u, j = 0u; i < src.len; i += 1u, j += 2u) {
    u8 v = src.data[i];

    dst[j] = table[v >> 4];
    dst[j + 1u] = table[v & 0x0fu];
  }

  return br__hex_bytes_result(dst, out_len, BR_STATUS_OK);
}

br_io_result
br_hex_encode_into(br_bytes_view src, br_hex_case letter_case, u8 *dst, usize dst_cap) {
  const u8 *table = br__hex_table(letter_case);
  usize out_len;

  /* out_len = src.len * 2; guard the (practically unreachable) overflow. */
  if (src.len > SIZE_MAX / 2u) {
    return br_io_result_make(0u, BR_STATUS_SHORT_BUFFER);
  }
  out_len = src.len * 2u;

  if (out_len > 0u && dst == NULL) {
    return br_io_result_make(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_cap < out_len) {
    return br_io_result_make(0u, BR_STATUS_SHORT_BUFFER);
  }

  for (usize i = 0u, j = 0u; i < src.len; i += 1u, j += 2u) {
    u8 v = src.data[i];

    dst[j] = table[v >> 4];
    dst[j + 1u] = table[v & 0x0fu];
  }

  return br_io_result_make(out_len, BR_STATUS_OK);
}

br_io_result br_hex_encode_to_writer(br_bytes_view src, br_hex_case letter_case, br_writer w) {
  const u8 *table = br__hex_table(letter_case);
  u8 buffer[512];
  usize buffered = 0u;
  usize total = 0u;

  for (usize i = 0u; i < src.len; i += 1u) {
    u8 v = src.data[i];

    buffer[buffered] = table[v >> 4];
    buffer[buffered + 1u] = table[v & 0x0fu];
    buffered += 2u;

    if (buffered == sizeof(buffer)) {
      br_io_result written = br_write_full(w, buffer, buffered);

      total += written.count;
      if (written.status != BR_STATUS_OK) {
        return br_io_result_make(total, written.status);
      }
      buffered = 0u;
    }
  }

  if (buffered > 0u) {
    br_io_result written = br_write_full(w, buffer, buffered);

    total += written.count;
    if (written.status != BR_STATUS_OK) {
      return br_io_result_make(total, written.status);
    }
  }

  return br_io_result_make(total, BR_STATUS_OK);
}

br_decode_result br_hex_decode(br_bytes_view src, br_allocator allocator) {
  br_alloc_result alloc;
  u8 *dst;
  usize out_len;

  /* Length parity is a structural property, so it is checked before allocating.
     This also keeps the odd-length path off the allocate-then-fail branch. */
  if ((src.len & 1u) != 0u) {
    return br__hex_decode_result(br_bytes_make(NULL, 0u), src.len - 1u, BR_STATUS_INVALID_ENCODING);
  }

  out_len = src.len / 2u;
  if (out_len == 0u) {
    return br__hex_decode_result(br_bytes_make(NULL, 0u), 0u, BR_STATUS_OK);
  }

  alloc = br_allocator_alloc_uninit(allocator, out_len, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__hex_decode_result(br_bytes_make(NULL, 0u), 0u, alloc.status);
  }
  dst = (u8 *)alloc.ptr;

  for (usize i = 0u, j = 0u; j < src.len; i += 1u, j += 2u) {
    u8 hi;
    u8 lo;

    /* Free before returning any failure: this fixes Odin's decode leak, where
       the destination buffer is abandoned on an invalid byte. */
    if (!br__hex_digit(src.data[j], &hi)) {
      br_allocator_free(allocator, dst, out_len);
      return br__hex_decode_result(br_bytes_make(NULL, 0u), j, BR_STATUS_INVALID_ENCODING);
    }
    if (!br__hex_digit(src.data[j + 1u], &lo)) {
      br_allocator_free(allocator, dst, out_len);
      return br__hex_decode_result(br_bytes_make(NULL, 0u), j + 1u, BR_STATUS_INVALID_ENCODING);
    }

    dst[i] = (u8)((hi << 4) | lo);
  }

  return br__hex_decode_result(br_bytes_make(dst, out_len), 0u, BR_STATUS_OK);
}

br_decode_into_result br_hex_decode_into(br_bytes_view src, u8 *dst, usize dst_cap) {
  usize out_len;

  /* Parity is checked first so odd-length input classifies as INVALID_ENCODING
     identically to br_hex_decode, independent of the caller's buffer state. */
  if ((src.len & 1u) != 0u) {
    return br__hex_decode_into_result(0u, src.len - 1u, BR_STATUS_INVALID_ENCODING);
  }

  out_len = src.len / 2u;

  if (out_len > 0u && dst == NULL) {
    return br__hex_decode_into_result(0u, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_cap < out_len) {
    return br__hex_decode_into_result(0u, 0u, BR_STATUS_SHORT_BUFFER);
  }

  for (usize i = 0u, j = 0u; j < src.len; i += 1u, j += 2u) {
    u8 hi;
    u8 lo;

    /* On a malformed byte the caller's buffer holds only partial scratch, so
       count reports 0: no valid output was produced (mirrors the allocating
       decode returning an empty value). error_offset locates the fault. */
    if (!br__hex_digit(src.data[j], &hi)) {
      return br__hex_decode_into_result(0u, j, BR_STATUS_INVALID_ENCODING);
    }
    if (!br__hex_digit(src.data[j + 1u], &lo)) {
      return br__hex_decode_into_result(0u, j + 1u, BR_STATUS_INVALID_ENCODING);
    }

    dst[i] = (u8)((hi << 4) | lo);
  }

  return br__hex_decode_into_result(out_len, 0u, BR_STATUS_OK);
}

br_io_byte_result br_hex_decode_sequence(br_bytes_view seq) {
  u8 hi;
  u8 lo;

  /* Strip an optional "0x"/"0X" prefix (seq is a by-value view). */
  if (seq.len >= 2u && seq.data[0] == '0' && (seq.data[1] == 'x' || seq.data[1] == 'X')) {
    seq.data += 2;
    seq.len -= 2u;
  }

  if (seq.len != 2u) {
    return br_io_byte_result_make(0u, BR_STATUS_INVALID_ENCODING);
  }
  if (!br__hex_digit(seq.data[0], &hi) || !br__hex_digit(seq.data[1], &lo)) {
    return br_io_byte_result_make(0u, BR_STATUS_INVALID_ENCODING);
  }

  return br_io_byte_result_make((u8)((hi << 4) | lo), BR_STATUS_OK);
}
