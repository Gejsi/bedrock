#include <bedrock/encoding/base64.h>

static const u8 br__base64_std_alphabet[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

static const u8 br__base64_url_alphabet[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_'};

#define BR__BASE64_INVALID 0xFFu

static const u8 *br__base64_alphabet(br_base64_alphabet which) {
  return which == BR_BASE64_URL ? br__base64_url_alphabet : br__base64_std_alphabet;
}

/*
Map an encoded character to its 0-63 value under `which`, or BR__BASE64_INVALID.
The two alphabets share the first 62 entries and differ only in the last two, so
a small branch on those covers both without per-alphabet 256-byte tables.
*/
static u8 br__base64_decode_value(br_base64_alphabet which, u8 c) {
  if (c >= 'A' && c <= 'Z') {
    return (u8)(c - 'A');
  }
  if (c >= 'a' && c <= 'z') {
    return (u8)(c - 'a' + 26);
  }
  if (c >= '0' && c <= '9') {
    return (u8)(c - '0' + 52);
  }
  if (which == BR_BASE64_URL) {
    if (c == '-') {
      return 62u;
    }
    if (c == '_') {
      return 63u;
    }
  } else {
    if (c == '+') {
      return 62u;
    }
    if (c == '/') {
      return 63u;
    }
  }
  return BR__BASE64_INVALID;
}

static br_bytes_result br__base64_bytes_result(u8 *data, usize len, br_status status) {
  br_bytes_result result;

  result.value = br_bytes_make(data, len);
  result.status = status;
  return result;
}

static br_decode_result br__base64_decode_result(u8 *data, usize len, usize off, br_status status) {
  br_decode_result result;

  result.value = br_bytes_make(data, len);
  result.error_offset = off;
  result.status = status;
  return result;
}

static br_decode_into_result
br__base64_decode_into_result(usize count, usize off, br_status status) {
  br_decode_into_result result;

  result.count = count;
  result.error_offset = off;
  result.status = status;
  return result;
}

size_t br_base64_encoded_len(br_base64_encoding enc, size_t src_len) {
  usize groups = src_len / 3u;
  usize rem = src_len % 3u;

  if (enc.padded) {
    return (src_len + 2u) / 3u * 4u;
  }
  /* raw: full groups are 4 chars; a 1- or 2-byte tail is 2 or 3 chars. */
  return groups * 4u + (rem == 0u ? 0u : rem + 1u);
}

size_t br_base64_decoded_len(br_base64_encoding enc, br_bytes_view src) {
  usize n = src.len;

  if (enc.padded) {
    if (n == 0u || n % 4u != 0u) {
      return n == 0u ? 0u : 0u;
    }
    /* Subtract padding characters from the last group. */
    {
      usize pad = 0u;
      if (src.data[n - 1u] == '=') {
        pad += 1u;
      }
      if (n >= 2u && src.data[n - 2u] == '=') {
        pad += 1u;
      }
      return n / 4u * 3u - pad;
    }
  }
  /* raw: length %4 == 1 is impossible. */
  if (n % 4u == 1u) {
    return 0u;
  }
  return n / 4u * 3u + (n % 4u == 0u ? 0u : n % 4u - 1u);
}

/* --- encode --- */

/*
Encode `src` fully into `dst`, which the caller has sized to at least
`br_base64_encoded_len(enc, src.len)`. Returns the number of bytes written.
*/
static usize br__base64_encode_raw(br_base64_encoding enc, br_bytes_view src, u8 *dst) {
  const u8 *tbl = br__base64_alphabet(enc.alphabet);
  usize i = 0u;
  usize o = 0u;

  while (i + 3u <= src.len) {
    u32 n = ((u32)src.data[i] << 16) | ((u32)src.data[i + 1u] << 8) | (u32)src.data[i + 2u];
    dst[o] = tbl[(n >> 18) & 0x3Fu];
    dst[o + 1u] = tbl[(n >> 12) & 0x3Fu];
    dst[o + 2u] = tbl[(n >> 6) & 0x3Fu];
    dst[o + 3u] = tbl[n & 0x3Fu];
    i += 3u;
    o += 4u;
  }

  if (i < src.len) {
    usize rem = src.len - i;
    u32 n = (u32)src.data[i] << 16;
    if (rem == 2u) {
      n |= (u32)src.data[i + 1u] << 8;
    }
    dst[o] = tbl[(n >> 18) & 0x3Fu];
    dst[o + 1u] = tbl[(n >> 12) & 0x3Fu];
    o += 2u;
    if (rem == 2u) {
      dst[o] = tbl[(n >> 6) & 0x3Fu];
      o += 1u;
    }
    if (enc.padded) {
      dst[o] = '=';
      o += 1u;
      if (rem == 1u) {
        dst[o] = '=';
        o += 1u;
      }
    }
  }

  return o;
}

br_bytes_result
br_base64_encode(br_base64_encoding enc, br_bytes_view src, br_allocator allocator) {
  usize out_len;
  br_alloc_result alloc;
  usize written;

  if (src.len == 0u) {
    return br__base64_bytes_result(NULL, 0u, BR_STATUS_OK);
  }

  out_len = br_base64_encoded_len(enc, src.len);
  alloc = br_allocator_alloc_uninit(allocator, out_len, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__base64_bytes_result(NULL, 0u, alloc.status);
  }

  written = br__base64_encode_raw(enc, src, (u8 *)alloc.ptr);
  return br__base64_bytes_result((u8 *)alloc.ptr, written, BR_STATUS_OK);
}

br_io_result
br_base64_encode_into(br_base64_encoding enc, br_bytes_view src, u8 *dst, usize dst_cap) {
  usize out_len = br_base64_encoded_len(enc, src.len);
  usize written;

  if (out_len == 0u) {
    return br_io_result_make(0u, BR_STATUS_OK);
  }
  if (dst == NULL || dst_cap < out_len) {
    return br_io_result_make(0u, BR_STATUS_SHORT_BUFFER);
  }
  written = br__base64_encode_raw(enc, src, dst);
  return br_io_result_make(written, BR_STATUS_OK);
}

br_io_result br_base64_encode_to_writer(br_base64_encoding enc, br_bytes_view src, br_writer w) {
  const u8 *tbl = br__base64_alphabet(enc.alphabet);
  u8 buffer[512]; /* multiple of 4 so quanta never split across flushes */
  usize buffered = 0u;
  usize total = 0u;
  usize i = 0u;

  while (i + 3u <= src.len) {
    u32 n = ((u32)src.data[i] << 16) | ((u32)src.data[i + 1u] << 8) | (u32)src.data[i + 2u];
    buffer[buffered] = tbl[(n >> 18) & 0x3Fu];
    buffer[buffered + 1u] = tbl[(n >> 12) & 0x3Fu];
    buffer[buffered + 2u] = tbl[(n >> 6) & 0x3Fu];
    buffer[buffered + 3u] = tbl[n & 0x3Fu];
    buffered += 4u;
    i += 3u;

    if (buffered == sizeof(buffer)) {
      br_io_result written = br_write_full(w, buffer, buffered);
      total += written.count;
      if (written.status != BR_STATUS_OK) {
        return br_io_result_make(total, written.status);
      }
      buffered = 0u;
    }
  }

  if (i < src.len) {
    usize rem = src.len - i;
    u32 n = (u32)src.data[i] << 16;
    if (rem == 2u) {
      n |= (u32)src.data[i + 1u] << 8;
    }
    buffer[buffered] = tbl[(n >> 18) & 0x3Fu];
    buffer[buffered + 1u] = tbl[(n >> 12) & 0x3Fu];
    buffered += 2u;
    if (rem == 2u) {
      buffer[buffered] = tbl[(n >> 6) & 0x3Fu];
      buffered += 1u;
    }
    if (enc.padded) {
      buffer[buffered] = '=';
      buffered += 1u;
      if (rem == 1u) {
        buffer[buffered] = '=';
        buffered += 1u;
      }
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

/* --- decode --- */

/*
Validate `src`'s structure under `enc` and report the payload character count
(excluding padding) plus the decoded byte count. On a structural violation
returns false with `*err_off`/`*status` set. Content (per-character alphabet
validity) is checked during the decode pass, not here.
*/
typedef struct br__base64_decode_shape {
  usize chars;   /* significant (non-padding) characters */
  usize decoded; /* output byte count */
} br__base64_decode_shape;

static bool br__base64_decode_shape_of(br_base64_encoding enc,
                                       br_bytes_view src,
                                       br__base64_decode_shape *out,
                                       usize *err_off) {
  usize n = src.len;
  usize pad = 0u;
  usize chars;
  usize i;

  if (n == 0u) {
    out->chars = 0u;
    out->decoded = 0u;
    return true;
  }

  /* Count trailing '=' padding (padded encodings only; raw forbids '='). */
  if (enc.padded) {
    if (src.data[n - 1u] == '=') {
      pad = 1u;
      if (n >= 2u && src.data[n - 2u] == '=') {
        pad = 2u;
      }
    }
  }
  chars = n - pad;

  /*
  A byte outside the active alphabet is a content error reported at its index,
  and it takes precedence over the structural length check (so "Zm\n9v" points
  at the '\n', not the last byte). '=' anywhere but the counted trailing padding
  is such a byte. This scan runs before the length rule below.
  */
  for (i = 0u; i < chars; i += 1u) {
    if (br__base64_decode_value(enc.alphabet, src.data[i]) == BR__BASE64_INVALID) {
      *err_off = i;
      return false;
    }
  }

  /* All significant bytes are valid; now enforce the structural length rule. */
  if (enc.padded) {
    if (n % 4u != 0u) {
      *err_off = n - 1u;
      return false;
    }
    out->chars = chars;
    out->decoded = n / 4u * 3u - pad;
    return true;
  }

  /* raw: length %4 == 1 is impossible. */
  if (n % 4u == 1u) {
    *err_off = n - 1u;
    return false;
  }
  out->chars = n;
  out->decoded = n / 4u * 3u + (n % 4u == 0u ? 0u : n % 4u - 1u);
  return true;
}

/*
Decode the `shape.chars` significant characters of `src` into `dst`. Returns OK,
or INVALID_ENCODING with `*err_off` at the first bad character or the first
final-quantum trailing-bit violation (strict only). Assumes `dst` holds
`shape.decoded` bytes.
*/
static br_status br__base64_decode_chars(br_base64_encoding enc,
                                         br_bytes_view src,
                                         br__base64_decode_shape shape,
                                         u8 *dst,
                                         usize *err_off) {
  usize i = 0u;
  usize o = 0u;

  while (i + 4u <= shape.chars) {
    u8 v0 = br__base64_decode_value(enc.alphabet, src.data[i]);
    u8 v1 = br__base64_decode_value(enc.alphabet, src.data[i + 1u]);
    u8 v2 = br__base64_decode_value(enc.alphabet, src.data[i + 2u]);
    u8 v3 = br__base64_decode_value(enc.alphabet, src.data[i + 3u]);
    u32 n;

    if (v0 == BR__BASE64_INVALID) {
      *err_off = i;
      return BR_STATUS_INVALID_ENCODING;
    }
    if (v1 == BR__BASE64_INVALID) {
      *err_off = i + 1u;
      return BR_STATUS_INVALID_ENCODING;
    }
    if (v2 == BR__BASE64_INVALID) {
      *err_off = i + 2u;
      return BR_STATUS_INVALID_ENCODING;
    }
    if (v3 == BR__BASE64_INVALID) {
      *err_off = i + 3u;
      return BR_STATUS_INVALID_ENCODING;
    }
    n = ((u32)v0 << 18) | ((u32)v1 << 12) | ((u32)v2 << 6) | (u32)v3;
    dst[o] = (u8)(n >> 16);
    dst[o + 1u] = (u8)(n >> 8);
    dst[o + 2u] = (u8)n;
    i += 4u;
    o += 3u;
  }

  /* Final partial quantum of 2 or 3 characters (1 is structurally rejected). */
  if (i < shape.chars) {
    usize rem = shape.chars - i;
    u8 v0 = br__base64_decode_value(enc.alphabet, src.data[i]);
    u8 v1 = br__base64_decode_value(enc.alphabet, src.data[i + 1u]);
    u32 n;

    if (v0 == BR__BASE64_INVALID) {
      *err_off = i;
      return BR_STATUS_INVALID_ENCODING;
    }
    if (v1 == BR__BASE64_INVALID) {
      *err_off = i + 1u;
      return BR_STATUS_INVALID_ENCODING;
    }
    n = ((u32)v0 << 18) | ((u32)v1 << 12);
    if (rem == 2u) {
      /* 2 chars -> 1 byte; low 4 bits of v1 are unused. */
      if (enc.strict && (v1 & 0x0Fu) != 0u) {
        *err_off = i + 1u;
        return BR_STATUS_INVALID_ENCODING;
      }
      dst[o] = (u8)(n >> 16);
    } else {
      /* rem == 3: 3 chars -> 2 bytes; low 2 bits of v2 are unused. */
      u8 v2 = br__base64_decode_value(enc.alphabet, src.data[i + 2u]);
      if (v2 == BR__BASE64_INVALID) {
        *err_off = i + 2u;
        return BR_STATUS_INVALID_ENCODING;
      }
      if (enc.strict && (v2 & 0x03u) != 0u) {
        *err_off = i + 2u;
        return BR_STATUS_INVALID_ENCODING;
      }
      n |= (u32)v2 << 6;
      dst[o] = (u8)(n >> 16);
      dst[o + 1u] = (u8)(n >> 8);
    }
  }

  return BR_STATUS_OK;
}

br_decode_result
br_base64_decode(br_base64_encoding enc, br_bytes_view src, br_allocator allocator) {
  br__base64_decode_shape shape;
  usize err_off = 0u;
  br_alloc_result alloc;
  br_status status;

  if (!br__base64_decode_shape_of(enc, src, &shape, &err_off)) {
    return br__base64_decode_result(NULL, 0u, err_off, BR_STATUS_INVALID_ENCODING);
  }
  if (shape.decoded == 0u) {
    return br__base64_decode_result(NULL, 0u, 0u, BR_STATUS_OK);
  }

  alloc = br_allocator_alloc_uninit(allocator, shape.decoded, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br__base64_decode_result(NULL, 0u, 0u, alloc.status);
  }

  status = br__base64_decode_chars(enc, src, shape, (u8 *)alloc.ptr, &err_off);
  if (status != BR_STATUS_OK) {
    /* Free the scratch so a failed decode never returns a live allocation. */
    (void)br_allocator_free(allocator, alloc.ptr, shape.decoded);
    return br__base64_decode_result(NULL, 0u, err_off, status);
  }
  return br__base64_decode_result((u8 *)alloc.ptr, shape.decoded, 0u, BR_STATUS_OK);
}

br_decode_into_result
br_base64_decode_into(br_base64_encoding enc, br_bytes_view src, u8 *dst, usize dst_cap) {
  br__base64_decode_shape shape;
  usize err_off = 0u;
  br_status status;

  if (!br__base64_decode_shape_of(enc, src, &shape, &err_off)) {
    return br__base64_decode_into_result(0u, err_off, BR_STATUS_INVALID_ENCODING);
  }
  if (shape.decoded == 0u) {
    return br__base64_decode_into_result(0u, 0u, BR_STATUS_OK);
  }
  if (dst == NULL || dst_cap < shape.decoded) {
    return br__base64_decode_into_result(0u, 0u, BR_STATUS_SHORT_BUFFER);
  }

  status = br__base64_decode_chars(enc, src, shape, dst, &err_off);
  if (status != BR_STATUS_OK) {
    return br__base64_decode_into_result(0u, err_off, status);
  }
  return br__base64_decode_into_result(shape.decoded, 0u, BR_STATUS_OK);
}

br_decode_into_result
br_base64_decode_to_writer(br_base64_encoding enc, br_bytes_view src, br_writer w) {
  br__base64_decode_shape shape;
  usize err_off = 0u;
  usize i = 0u;
  usize total = 0u;
  u8 buffer[384]; /* multiple of 3: whole output triples never split a flush */
  usize buffered = 0u;

  if (!br__base64_decode_shape_of(enc, src, &shape, &err_off)) {
    return br__base64_decode_into_result(0u, err_off, BR_STATUS_INVALID_ENCODING);
  }
  if (shape.decoded == 0u) {
    return br__base64_decode_into_result(0u, 0u, BR_STATUS_OK);
  }

  while (i + 4u <= shape.chars) {
    u8 v0 = br__base64_decode_value(enc.alphabet, src.data[i]);
    u8 v1 = br__base64_decode_value(enc.alphabet, src.data[i + 1u]);
    u8 v2 = br__base64_decode_value(enc.alphabet, src.data[i + 2u]);
    u8 v3 = br__base64_decode_value(enc.alphabet, src.data[i + 3u]);
    u32 n;

    if (v0 == BR__BASE64_INVALID) {
      return br__base64_decode_into_result(total, i, BR_STATUS_INVALID_ENCODING);
    }
    if (v1 == BR__BASE64_INVALID) {
      return br__base64_decode_into_result(total, i + 1u, BR_STATUS_INVALID_ENCODING);
    }
    if (v2 == BR__BASE64_INVALID) {
      return br__base64_decode_into_result(total, i + 2u, BR_STATUS_INVALID_ENCODING);
    }
    if (v3 == BR__BASE64_INVALID) {
      return br__base64_decode_into_result(total, i + 3u, BR_STATUS_INVALID_ENCODING);
    }
    n = ((u32)v0 << 18) | ((u32)v1 << 12) | ((u32)v2 << 6) | (u32)v3;
    buffer[buffered] = (u8)(n >> 16);
    buffer[buffered + 1u] = (u8)(n >> 8);
    buffer[buffered + 2u] = (u8)n;
    buffered += 3u;
    i += 4u;

    if (buffered == sizeof(buffer)) {
      br_io_result written = br_write_full(w, buffer, buffered);
      total += written.count;
      if (written.status != BR_STATUS_OK) {
        return br__base64_decode_into_result(total, 0u, written.status);
      }
      buffered = 0u;
    }
  }

  if (i < shape.chars) {
    usize rem = shape.chars - i;
    u8 v0 = br__base64_decode_value(enc.alphabet, src.data[i]);
    u8 v1 = br__base64_decode_value(enc.alphabet, src.data[i + 1u]);
    u32 n;

    if (v0 == BR__BASE64_INVALID) {
      return br__base64_decode_into_result(total, i, BR_STATUS_INVALID_ENCODING);
    }
    if (v1 == BR__BASE64_INVALID) {
      return br__base64_decode_into_result(total, i + 1u, BR_STATUS_INVALID_ENCODING);
    }
    n = ((u32)v0 << 18) | ((u32)v1 << 12);
    if (rem == 2u) {
      if (enc.strict && (v1 & 0x0Fu) != 0u) {
        return br__base64_decode_into_result(total, i + 1u, BR_STATUS_INVALID_ENCODING);
      }
      buffer[buffered] = (u8)(n >> 16);
      buffered += 1u;
    } else {
      u8 v2 = br__base64_decode_value(enc.alphabet, src.data[i + 2u]);
      if (v2 == BR__BASE64_INVALID) {
        return br__base64_decode_into_result(total, i + 2u, BR_STATUS_INVALID_ENCODING);
      }
      if (enc.strict && (v2 & 0x03u) != 0u) {
        return br__base64_decode_into_result(total, i + 2u, BR_STATUS_INVALID_ENCODING);
      }
      n |= (u32)v2 << 6;
      buffer[buffered] = (u8)(n >> 16);
      buffer[buffered + 1u] = (u8)(n >> 8);
      buffered += 2u;
    }
  }

  if (buffered > 0u) {
    br_io_result written = br_write_full(w, buffer, buffered);
    total += written.count;
    if (written.status != BR_STATUS_OK) {
      return br__base64_decode_into_result(total, 0u, written.status);
    }
  }

  return br__base64_decode_into_result(total, 0u, BR_STATUS_OK);
}
