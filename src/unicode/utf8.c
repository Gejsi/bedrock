#include <bedrock/unicode/utf8.h>

#define BR__UTF8_LOCB ((u8)0x80)
#define BR__UTF8_HICB ((u8)0xbf)
#define BR__UTF8_MASKX ((u8)0x3f)
#define BR__UTF8_MASK2 ((u8)0x1f)
#define BR__UTF8_MASK3 ((u8)0x0f)
#define BR__UTF8_MASK4 ((u8)0x07)
#define BR__UTF8_SURROGATE_MIN ((br_rune)0xd800)
#define BR__UTF8_SURROGATE_MAX ((br_rune)0xdfff)

typedef struct br__utf8_lead_info {
  usize width;
  u8 lo;
  u8 hi;
  bool ascii;
  bool invalid;
} br__utf8_lead_info;

static br_utf8_decode_result br__utf8_decode_result(br_rune value, usize width) {
  br_utf8_decode_result result;

  result.value = value;
  result.width = width;
  return result;
}

static br_utf8_encode_result br__utf8_encode_result(u8 b0, u8 b1, u8 b2, u8 b3, usize len) {
  br_utf8_encode_result result;

  result.bytes[0] = b0;
  result.bytes[1] = b1;
  result.bytes[2] = b2;
  result.bytes[3] = b3;
  result.len = len;
  return result;
}

static br__utf8_lead_info br__utf8_classify_lead(u8 lead) {
  br__utf8_lead_info info;

  info.width = 1u;
  info.lo = BR__UTF8_LOCB;
  info.hi = BR__UTF8_HICB;
  info.ascii = false;
  info.invalid = false;

  if (lead <= 0x7fu) {
    info.ascii = true;
    return info;
  }
  if (lead < 0xc2u) {
    info.invalid = true;
    return info;
  }
  if (lead <= 0xdfu) {
    info.width = 2u;
    return info;
  }
  if (lead == 0xe0u) {
    info.width = 3u;
    info.lo = 0xa0u;
    return info;
  }
  if (lead <= 0xecu) {
    info.width = 3u;
    return info;
  }
  if (lead == 0xedu) {
    info.width = 3u;
    info.hi = 0x9fu;
    return info;
  }
  if (lead <= 0xefu) {
    info.width = 3u;
    return info;
  }
  if (lead == 0xf0u) {
    info.width = 4u;
    info.lo = 0x90u;
    return info;
  }
  if (lead <= 0xf3u) {
    info.width = 4u;
    return info;
  }
  if (lead == 0xf4u) {
    info.width = 4u;
    info.hi = 0x8fu;
    return info;
  }

  info.invalid = true;
  return info;
}

static bool br__utf8_is_continuation(u8 byte_value) {
  return byte_value >= BR__UTF8_LOCB && byte_value <= BR__UTF8_HICB;
}

br_utf8_encode_result br_utf8_encode(br_rune value) {
  u32 rune_value;

  if (!br_utf8_valid_rune(value)) {
    value = BR_RUNE_ERROR;
  }

  rune_value = (u32)value;
  if (rune_value <= 0x7fu) {
    return br__utf8_encode_result((u8)rune_value, 0u, 0u, 0u, 1u);
  }
  if (rune_value <= 0x7ffu) {
    return br__utf8_encode_result(
      (u8)(0xc0u | (rune_value >> 6)), (u8)(0x80u | (rune_value & BR__UTF8_MASKX)), 0u, 0u, 2u);
  }
  if (rune_value <= 0xffffu) {
    return br__utf8_encode_result((u8)(0xe0u | (rune_value >> 12)),
                                  (u8)(0x80u | ((rune_value >> 6) & BR__UTF8_MASKX)),
                                  (u8)(0x80u | (rune_value & BR__UTF8_MASKX)),
                                  0u,
                                  3u);
  }

  return br__utf8_encode_result((u8)(0xf0u | (rune_value >> 18)),
                                (u8)(0x80u | ((rune_value >> 12) & BR__UTF8_MASKX)),
                                (u8)(0x80u | ((rune_value >> 6) & BR__UTF8_MASKX)),
                                (u8)(0x80u | (rune_value & BR__UTF8_MASKX)),
                                4u);
}

br_utf8_decode_result br_utf8_decode(br_bytes_view s) {
  br__utf8_lead_info info;
  u8 b0;
  u8 b1;
  u8 b2;
  u8 b3;

  if (s.len == 0u) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 0u);
  }

  b0 = s.data[0];
  info = br__utf8_classify_lead(b0);
  if (info.ascii) {
    return br__utf8_decode_result((br_rune)b0, 1u);
  }
  if (info.invalid) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 1u);
  }
  if (s.len < info.width) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 1u);
  }

  b1 = s.data[1];
  if (b1 < info.lo || b1 > info.hi) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 1u);
  }
  if (info.width == 2u) {
    return br__utf8_decode_result(
      (br_rune)(((u32)(b0 & BR__UTF8_MASK2) << 6) | (u32)(b1 & BR__UTF8_MASKX)), 2u);
  }

  b2 = s.data[2];
  if (!br__utf8_is_continuation(b2)) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 1u);
  }
  if (info.width == 3u) {
    return br__utf8_decode_result((br_rune)(((u32)(b0 & BR__UTF8_MASK3) << 12) |
                                            ((u32)(b1 & BR__UTF8_MASKX) << 6) |
                                            (u32)(b2 & BR__UTF8_MASKX)),
                                  3u);
  }

  b3 = s.data[3];
  if (!br__utf8_is_continuation(b3)) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 1u);
  }

  return br__utf8_decode_result(
    (br_rune)(((u32)(b0 & BR__UTF8_MASK4) << 18) | ((u32)(b1 & BR__UTF8_MASKX) << 12) |
              ((u32)(b2 & BR__UTF8_MASKX) << 6) | (u32)(b3 & BR__UTF8_MASKX)),
    4u);
}

br_utf8_decode_result br_utf8_decode_last(br_bytes_view s) {
  usize end;
  isize start;
  isize limit;
  br_utf8_decode_result result;

  end = s.len;
  if (end == 0u) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 0u);
  }
  if (s.data[end - 1u] < (u8)BR_RUNE_SELF) {
    return br__utf8_decode_result((br_rune)s.data[end - 1u], 1u);
  }

  limit = end > (usize)BR_UTF8_MAX ? (isize)(end - (usize)BR_UTF8_MAX) : 0;
  start = (isize)end - 2;
  while (start >= limit) {
    if (br_utf8_rune_start(s.data[(usize)start])) {
      break;
    }
    start -= 1;
  }
  if (start < limit) {
    start = limit;
  }

  result = br_utf8_decode(br_bytes_view_make(s.data + (usize)start, end - (usize)start));
  if ((usize)start + result.width != end) {
    return br__utf8_decode_result(BR_RUNE_ERROR, 1u);
  }
  return result;
}

bool br_utf8_valid_rune(br_rune value) {
  if (value < 0) {
    return false;
  }
  if (value >= BR__UTF8_SURROGATE_MIN && value <= BR__UTF8_SURROGATE_MAX) {
    return false;
  }
  if (value > BR_RUNE_MAX) {
    return false;
  }
  return true;
}

bool br_utf8_valid(br_bytes_view s) {
  usize i = 0u;

  while (i < s.len) {
    br__utf8_lead_info info;
    u8 b1;

    info = br__utf8_classify_lead(s.data[i]);
    if (info.ascii) {
      i += 1u;
      continue;
    }
    if (info.invalid || i + info.width > s.len) {
      return false;
    }

    b1 = s.data[i + 1u];
    if (b1 < info.lo || b1 > info.hi) {
      return false;
    }
    if (info.width > 2u && !br__utf8_is_continuation(s.data[i + 2u])) {
      return false;
    }
    if (info.width > 3u && !br__utf8_is_continuation(s.data[i + 3u])) {
      return false;
    }

    i += info.width;
  }

  return true;
}

bool br_utf8_rune_start(u8 byte_value) {
  return (byte_value & 0xc0u) != 0x80u;
}

usize br_utf8_rune_count(br_bytes_view s) {
  usize count = 0u;
  usize i = 0u;

  while (i < s.len) {
    br__utf8_lead_info info;
    usize width = 1u;

    count += 1u;
    info = br__utf8_classify_lead(s.data[i]);
    if (info.ascii || info.invalid) {
      i += 1u;
      continue;
    }
    if (i + info.width > s.len) {
      i += 1u;
      continue;
    }
    if (s.data[i + 1u] < info.lo || s.data[i + 1u] > info.hi) {
      i += 1u;
      continue;
    }

    width = info.width;
    if (width > 2u && !br__utf8_is_continuation(s.data[i + 2u])) {
      width = 1u;
    } else if (width > 3u && !br__utf8_is_continuation(s.data[i + 3u])) {
      width = 1u;
    }

    i += width;
  }

  return count;
}

i32 br_utf8_rune_size(br_rune value) {
  if (value < 0) {
    return -1;
  }
  if (value <= 0x7f) {
    return 1;
  }
  if (value <= 0x7ff) {
    return 2;
  }
  if (value >= BR__UTF8_SURROGATE_MIN && value <= BR__UTF8_SURROGATE_MAX) {
    return -1;
  }
  if (value <= 0xffff) {
    return 3;
  }
  if (value <= BR_RUNE_MAX) {
    return 4;
  }
  return -1;
}

bool br_utf8_full_rune(br_bytes_view s) {
  br__utf8_lead_info info;

  if (s.len == 0u) {
    return false;
  }

  info = br__utf8_classify_lead(s.data[0]);
  if (info.ascii || info.invalid || s.len >= info.width) {
    return true;
  }
  if (s.len > 1u && (s.data[1] < info.lo || s.data[1] > info.hi)) {
    return true;
  }
  if (s.len > 2u && !br__utf8_is_continuation(s.data[2])) {
    return true;
  }

  return false;
}
