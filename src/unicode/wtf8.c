#include <bedrock/unicode/wtf8.h>

/*
WTF-8 codec.

The lead-byte classification and the mask/shift byte-layout arithmetic below are
a DELIBERATE local copy of the strict codec in src/unicode/utf8.c, not shared
code. The one intentional difference: the 0xE0..0xEF (3-byte) lead's continuation
range is NOT narrowed for 0xED -- strict UTF-8 caps 0xED's first continuation at
0x9F to ban the surrogate range U+D800..U+DFFF, whereas WTF-8 leaves it at 0xBF
so a lone surrogate decodes as its 3-byte sequence. That single relaxation is
the entire point of the copy; everything else matches utf8.c byte for byte. The
duplication is safe because UTF-8's byte layout is frozen by specification, and a
differential test (tests/test_wtf8.c) pins that strictly-valid UTF-8 decodes
identically through both surfaces. Keeping the copy here leaves utf8.c untouched,
so the strict codec's contract and test suite are unaffected by construction.
*/

#define BR__WTF8_LOCB ((u8)0x80)
#define BR__WTF8_HICB ((u8)0xbf)
#define BR__WTF8_MASKX ((u8)0x3f)
#define BR__WTF8_MASK2 ((u8)0x1f)
#define BR__WTF8_MASK3 ((u8)0x0f)
#define BR__WTF8_MASK4 ((u8)0x07)

#define BR__WTF8_SURROGATE_MIN 0xd800u
#define BR__WTF8_SURROGATE_MAX 0xdfffu
#define BR__WTF8_HIGH_SURROGATE_MAX 0xdbffu
#define BR__WTF8_LOW_SURROGATE_MIN 0xdc00u
#define BR__WTF8_ASTRAL_MIN 0x10000u
#define BR__WTF8_MAX_RUNE 0x10ffffu

typedef struct br__wtf8_lead_info {
  usize width;
  u8 lo;
  u8 hi;
  bool ascii;
  bool invalid;
} br__wtf8_lead_info;

/* Classify a lead byte. Identical to utf8.c's br__utf8_classify_lead EXCEPT the
   0xED case keeps hi = 0xBF (utf8 uses 0x9F to reject surrogates). */
static br__wtf8_lead_info br__wtf8_classify_lead(u8 lead) {
  br__wtf8_lead_info info;

  info.width = 1u;
  info.lo = BR__WTF8_LOCB;
  info.hi = BR__WTF8_HICB;
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
  /* 0xE1..0xEF all use the full 0x80..0xBF continuation range here; strict
     UTF-8 special-cases 0xED to 0x9F to exclude surrogates, WTF-8 does not. */
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

static bool br__wtf8_is_continuation(u8 byte_value) {
  return byte_value >= BR__WTF8_LOCB && byte_value <= BR__WTF8_HICB;
}

/* Decode the first code point of `s`, allowing surrogates. On success sets
   *out_cp and *out_width (bytes consumed) and returns true. On any malformed
   prefix returns false (out params untouched). */
static bool br__wtf8_decode(br_bytes_view s, u32 *out_cp, usize *out_width) {
  br__wtf8_lead_info info;
  u8 b0;
  u8 b1;
  u8 b2;
  u8 b3;

  if (s.len == 0u) {
    return false;
  }

  b0 = s.data[0];
  info = br__wtf8_classify_lead(b0);
  if (info.ascii) {
    *out_cp = (u32)b0;
    *out_width = 1u;
    return true;
  }
  if (info.invalid || s.len < info.width) {
    return false;
  }

  b1 = s.data[1];
  if (b1 < info.lo || b1 > info.hi) {
    return false;
  }
  if (info.width == 2u) {
    *out_cp = ((u32)(b0 & BR__WTF8_MASK2) << 6) | (u32)(b1 & BR__WTF8_MASKX);
    *out_width = 2u;
    return true;
  }

  b2 = s.data[2];
  if (!br__wtf8_is_continuation(b2)) {
    return false;
  }
  if (info.width == 3u) {
    *out_cp = ((u32)(b0 & BR__WTF8_MASK3) << 12) | ((u32)(b1 & BR__WTF8_MASKX) << 6) |
              (u32)(b2 & BR__WTF8_MASKX);
    *out_width = 3u;
    return true;
  }

  b3 = s.data[3];
  if (!br__wtf8_is_continuation(b3)) {
    return false;
  }
  *out_cp = ((u32)(b0 & BR__WTF8_MASK4) << 18) | ((u32)(b1 & BR__WTF8_MASKX) << 12) |
            ((u32)(b2 & BR__WTF8_MASKX) << 6) | (u32)(b3 & BR__WTF8_MASKX);
  *out_width = 4u;
  return true;
}

/* Encode a code point (surrogates allowed, <= U+10FFFF) as WTF-8 into `dst`,
   returning the byte count. Caller guarantees room (<= 4). */
static usize br__wtf8_encode(u32 cp, u8 *dst) {
  if (cp <= 0x7fu) {
    dst[0] = (u8)cp;
    return 1u;
  }
  if (cp <= 0x7ffu) {
    dst[0] = (u8)(0xc0u | (cp >> 6));
    dst[1] = (u8)(0x80u | (cp & BR__WTF8_MASKX));
    return 2u;
  }
  if (cp <= 0xffffu) {
    dst[0] = (u8)(0xe0u | (cp >> 12));
    dst[1] = (u8)(0x80u | ((cp >> 6) & BR__WTF8_MASKX));
    dst[2] = (u8)(0x80u | (cp & BR__WTF8_MASKX));
    return 3u;
  }
  dst[0] = (u8)(0xf0u | (cp >> 18));
  dst[1] = (u8)(0x80u | ((cp >> 12) & BR__WTF8_MASKX));
  dst[2] = (u8)(0x80u | ((cp >> 6) & BR__WTF8_MASKX));
  dst[3] = (u8)(0x80u | (cp & BR__WTF8_MASKX));
  return 4u;
}

static bool br__wtf8_is_high_surrogate(u32 cp) {
  return cp >= BR__WTF8_SURROGATE_MIN && cp <= BR__WTF8_HIGH_SURROGATE_MAX;
}

static bool br__wtf8_is_low_surrogate(u32 cp) {
  return cp >= BR__WTF8_LOW_SURROGATE_MIN && cp <= BR__WTF8_SURROGATE_MAX;
}

/* Combine a high+low surrogate pair into its astral scalar value. */
static u32 br__wtf8_combine_surrogates(u32 high, u32 low) {
  return BR__WTF8_ASTRAL_MIN + ((high - BR__WTF8_SURROGATE_MIN) << 10) +
         (low - BR__WTF8_LOW_SURROGATE_MIN);
}

bool br_wtf8_valid(br_bytes_view s) {
  usize i = 0u;
  bool prev_was_high_surrogate = false;

  while (i < s.len) {
    u32 cp;
    usize width;

    /* ASCII fast path: scan runs of < 0x80 bytes without rune assembly. */
    if (s.data[i] < 0x80u) {
      i += 1u;
      prev_was_high_surrogate = false;
      continue;
    }

    if (!br__wtf8_decode(br_bytes_view_make(s.data + i, s.len - i), &cp, &width)) {
      return false;
    }
    if (cp > BR__WTF8_MAX_RUNE) {
      return false;
    }
    /* Well-formed rule: a high surrogate immediately followed by a low
       surrogate (both as 3-byte sequences) is a pair that must have been the
       4-byte astral form instead. */
    if (prev_was_high_surrogate && br__wtf8_is_low_surrogate(cp)) {
      return false;
    }
    prev_was_high_surrogate = br__wtf8_is_high_surrogate(cp);
    i += width;
  }

  return true;
}

size_t br_wtf8_from_wtf16_len(br_wtf16_view s) {
  usize total = 0u;
  usize i = 0u;

  while (i < s.len) {
    u32 unit = s.data[i];

    if (br__wtf8_is_high_surrogate(unit) && i + 1u < s.len &&
        br__wtf8_is_low_surrogate(s.data[i + 1u])) {
      total += 4u; /* astral pair */
      i += 2u;
    } else if (unit <= 0x7fu) {
      total += 1u;
      i += 1u;
    } else if (unit <= 0x7ffu) {
      total += 2u;
      i += 1u;
    } else {
      total += 3u; /* BMP scalar or lone surrogate */
      i += 1u;
    }
  }

  return total;
}

static br_io_result br__wtf8_io_result(usize count, br_status status) {
  br_io_result result;

  result.count = count;
  result.status = status;
  return result;
}

br_io_result br_wtf8_from_wtf16(br_wtf16_view s, uint8_t *dst, size_t dst_cap) {
  usize needed = br_wtf8_from_wtf16_len(s);
  usize pos = 0u;
  usize i = 0u;

  if (needed > 0u && dst == NULL) {
    return br__wtf8_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_cap < needed) {
    return br__wtf8_io_result(0u, BR_STATUS_SHORT_BUFFER);
  }

  while (i < s.len) {
    u32 unit = s.data[i];

    if (br__wtf8_is_high_surrogate(unit) && i + 1u < s.len &&
        br__wtf8_is_low_surrogate(s.data[i + 1u])) {
      pos += br__wtf8_encode(br__wtf8_combine_surrogates(unit, s.data[i + 1u]), dst + pos);
      i += 2u;
    } else {
      pos += br__wtf8_encode(unit, dst + pos);
      i += 1u;
    }
  }

  return br__wtf8_io_result(pos, BR_STATUS_OK);
}

size_t br_wtf16_from_wtf8_len(br_bytes_view s) {
  usize total = 0u;
  usize i = 0u;

  while (i < s.len) {
    u32 cp;
    usize width;

    if (s.data[i] < 0x80u) {
      total += 1u;
      i += 1u;
      continue;
    }
    if (!br__wtf8_decode(br_bytes_view_make(s.data + i, s.len - i), &cp, &width)) {
      /* Malformed input: count the offending byte as one unit and resync by
         one, matching the strict decoder's error resync. Callers size against
         well-formed input; this only keeps the sizer total and the transcode
         in lockstep on garbage. */
      total += 1u;
      i += 1u;
      continue;
    }
    total += cp >= BR__WTF8_ASTRAL_MIN ? 2u : 1u;
    i += width;
  }

  return total;
}

br_io_result br_wtf16_from_wtf8(br_bytes_view s, uint16_t *dst, size_t dst_cap) {
  usize needed = br_wtf16_from_wtf8_len(s);
  usize pos = 0u;
  usize i = 0u;

  if (needed > 0u && dst == NULL) {
    return br__wtf8_io_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (dst_cap < needed) {
    return br__wtf8_io_result(0u, BR_STATUS_SHORT_BUFFER);
  }

  while (i < s.len) {
    u32 cp;
    usize width;

    if (s.data[i] < 0x80u) {
      dst[pos] = (uint16_t)s.data[i];
      pos += 1u;
      i += 1u;
      continue;
    }
    if (!br__wtf8_decode(br_bytes_view_make(s.data + i, s.len - i), &cp, &width)) {
      /* Malformed byte: emit U+FFFD and resync by one, staying in lockstep with
         the sizer (which counts it as one unit). Callers transcode well-formed
         input; this only keeps garbage bounded and non-crashing. */
      dst[pos] = 0xfffdu;
      pos += 1u;
      i += 1u;
      continue;
    }
    if (cp >= BR__WTF8_ASTRAL_MIN) {
      u32 adjusted = cp - BR__WTF8_ASTRAL_MIN;
      dst[pos] = (uint16_t)(BR__WTF8_SURROGATE_MIN + (adjusted >> 10));
      dst[pos + 1u] = (uint16_t)(BR__WTF8_LOW_SURROGATE_MIN + (adjusted & 0x3ffu));
      pos += 2u;
    } else {
      dst[pos] = (uint16_t)cp;
      pos += 1u;
    }
    i += width;
  }

  return br__wtf8_io_result(pos, BR_STATUS_OK);
}

/* Byte length of the last WTF-8 sequence in `s`, and its decoded code point, if
   `s` ends in a valid sequence; returns 0 width otherwise. */
static usize br__wtf8_last_width(br_bytes_view s, u32 *out_cp) {
  usize start;
  u32 cp;
  usize width;

  /* A WTF-8 sequence is at most 4 bytes; walk back to the last lead byte. */
  for (start = s.len; start > 0u && start + 4u > s.len; start -= 1u) {
    u8 b = s.data[start - 1u];

    if ((b & 0xc0u) != 0x80u) { /* a lead or ASCII byte */
      if (br__wtf8_decode(
            br_bytes_view_make(s.data + (start - 1u), s.len - (start - 1u)), &cp, &width) &&
          (start - 1u) + width == s.len) {
        *out_cp = cp;
        return width;
      }
      return 0u;
    }
  }
  return 0u;
}

br_bytes_result br_wtf8_concat(br_bytes_view a, br_bytes_view b, br_allocator allocator) {
  br_alloc_result alloc;
  u8 *out;
  u32 a_last_cp = 0u;
  u32 b_first_cp;
  usize b_first_width;
  usize a_tail_width;
  bool splice = false;

  /* Detect the seam: `a` ends in a lone high surrogate and `b` starts with a
     lone low surrogate. If so, drop the two 3-byte halves and emit the single
     4-byte astral scalar in their place. */
  a_tail_width = br__wtf8_last_width(a, &a_last_cp);
  if (a_tail_width == 3u && br__wtf8_is_high_surrogate(a_last_cp) && b.len > 0u &&
      br__wtf8_decode(b, &b_first_cp, &b_first_width) && b_first_width == 3u &&
      br__wtf8_is_low_surrogate(b_first_cp)) {
    splice = true;
  }

  if (splice) {
    /* a without its trailing 3-byte high surrogate + 4-byte pair + b without
       its leading 3-byte low surrogate. */
    usize a_head = a.len - 3u;
    usize b_tail = b.len - 3u;
    usize total = a_head + 4u + b_tail;
    usize pos;

    alloc = br_allocator_alloc_uninit(allocator, total, 1u);
    if (alloc.status != BR_STATUS_OK) {
      return (br_bytes_result){br_bytes_make(NULL, 0u), alloc.status};
    }
    out = (u8 *)alloc.ptr;
    if (a_head > 0u) {
      memcpy(out, a.data, a_head);
    }
    pos = a_head;
    pos += br__wtf8_encode(br__wtf8_combine_surrogates(a_last_cp, b_first_cp), out + pos);
    if (b_tail > 0u) {
      memcpy(out + pos, b.data + 3u, b_tail);
    }
    return (br_bytes_result){br_bytes_make(out, total), BR_STATUS_OK};
  }

  /* Plain byte join. */
  {
    usize total = a.len + b.len;

    if (total == 0u) {
      return (br_bytes_result){br_bytes_make(NULL, 0u), BR_STATUS_OK};
    }
    alloc = br_allocator_alloc_uninit(allocator, total, 1u);
    if (alloc.status != BR_STATUS_OK) {
      return (br_bytes_result){br_bytes_make(NULL, 0u), alloc.status};
    }
    out = (u8 *)alloc.ptr;
    if (a.len > 0u) {
      memcpy(out, a.data, a.len);
    }
    if (b.len > 0u) {
      memcpy(out + a.len, b.data, b.len);
    }
    return (br_bytes_result){br_bytes_make(out, total), BR_STATUS_OK};
  }
}
