#include "internal.h"

#include <math.h>

/*
Result plumbing. The public result structs differ only in `value` type, so each
constructor is tiny; keeping them explicit avoids a macro that would obscure the
value types in the ABI.
*/
static br_parse_u64_result br__parse_u64_make(u64 value, usize consumed, br_status status) {
  br_parse_u64_result r;
  r.value = value;
  r.consumed = consumed;
  r.status = status;
  return r;
}

static br_parse_i64_result br__parse_i64_make(i64 value, usize consumed, br_status status) {
  br_parse_i64_result r;
  r.value = value;
  r.consumed = consumed;
  r.status = status;
  return r;
}

static br_parse_f64_result br__parse_f64_make(double value, usize consumed, br_status status) {
  br_parse_f64_result r;
  r.value = value;
  r.consumed = consumed;
  r.status = status;
  return r;
}

static br_parse_f32_result br__parse_f32_make(float value, usize consumed, br_status status) {
  br_parse_f32_result r;
  r.value = value;
  r.consumed = consumed;
  r.status = status;
  return r;
}

static bool br__parse_digit_value(u8 c, u32 *out) {
  u32 v;

  if (c >= '0' && c <= '9') {
    v = (u32)(c - '0');
  } else if (c >= 'a' && c <= 'z') {
    v = (u32)(c - 'a') + 10u;
  } else if (c >= 'A' && c <= 'Z') {
    v = (u32)(c - 'A') + 10u;
  } else {
    return false;
  }
  *out = v;
  return true;
}

/*
Unsigned magnitude parse shared by the signed and unsigned entry points. Reads
an optional base-0 prefix, then digits in `base`. `saturated` reports that the
magnitude exceeded `limit` (the caller maps that to OUT_OF_RANGE and keeps the
clamp). No sign is consumed here.
*/
static br_status br__parse_u64_magnitude(
  br_string_view s, int base, u64 limit, u64 *out_value, usize *out_consumed) {
  const u8 *p = (const u8 *)s.data;
  usize len = s.len;
  usize i = 0u;
  u64 value = 0u;
  u64 base_u;
  bool saturated = false;
  bool any_digit = false;

  if (base == 0) {
    base = 10;
    if (i + 1u < len && p[i] == '0') {
      u8 c = p[i + 1u];
      if (c == 'x' || c == 'X') {
        base = 16;
        i += 2u;
      } else if (c == 'o' || c == 'O') {
        base = 8;
        i += 2u;
      } else if (c == 'b' || c == 'B') {
        base = 2;
        i += 2u;
      }
    }
  }

  base_u = (u64)base;
  for (; i < len; i += 1u) {
    u32 dv;

    if (!br__parse_digit_value(p[i], &dv) || dv >= base_u) {
      break;
    }
    any_digit = true;
    if (!saturated) {
      if (value > (limit - dv) / base_u) {
        saturated = true;
        value = limit;
      } else {
        value = value * base_u + dv;
      }
    }
  }

  *out_consumed = i;
  if (!any_digit) {
    *out_value = 0u;
    return BR_STATUS_INVALID_ENCODING;
  }
  *out_value = value;
  return saturated ? BR_STATUS_OUT_OF_RANGE : BR_STATUS_OK;
}

static br_parse_u64_result br__parse_u64_impl(br_string_view s, int base, bool strict) {
  u64 value;
  usize consumed;
  br_status status;

  if (base != 0 && (base < 2 || base > 36)) {
    return br__parse_u64_make(0u, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (s.data == NULL || s.len == 0u) {
    return br__parse_u64_make(0u, 0u, BR_STATUS_INVALID_ENCODING);
  }
  /* A leading sign is not a valid unsigned digit. */
  if (s.data[0] == '-' || s.data[0] == '+') {
    return br__parse_u64_make(0u, 0u, BR_STATUS_INVALID_ENCODING);
  }

  status = br__parse_u64_magnitude(s, base, 0xFFFFFFFFFFFFFFFFuLL, &value, &consumed);
  if (status == BR_STATUS_INVALID_ENCODING) {
    return br__parse_u64_make(0u, consumed, status);
  }
  if (strict && consumed != s.len) {
    return br__parse_u64_make(0u, consumed, BR_STATUS_INVALID_ENCODING);
  }
  return br__parse_u64_make(value, consumed, status);
}

static br_parse_i64_result br__parse_i64_impl(br_string_view s, int base, bool strict) {
  const u8 *p = (const u8 *)s.data;
  usize off = 0u;
  bool neg = false;
  u64 limit;
  u64 mag;
  usize consumed;
  br_string_view rest;
  br_status status;

  if (base != 0 && (base < 2 || base > 36)) {
    return br__parse_i64_make(0, 0u, BR_STATUS_INVALID_ARGUMENT);
  }
  if (s.data == NULL || s.len == 0u) {
    return br__parse_i64_make(0, 0u, BR_STATUS_INVALID_ENCODING);
  }

  if (p[0] == '+') {
    off = 1u;
  } else if (p[0] == '-') {
    off = 1u;
    neg = true;
  }

  limit = neg ? 0x8000000000000000uLL : 0x7FFFFFFFFFFFFFFFuLL;
  rest.data = s.data + off;
  rest.len = s.len - off;

  status = br__parse_u64_magnitude(rest, base, limit, &mag, &consumed);
  if (status == BR_STATUS_INVALID_ENCODING) {
    return br__parse_i64_make(0, off == 0u ? 0u : consumed, BR_STATUS_INVALID_ENCODING);
  }
  consumed += off;
  if (strict && consumed != s.len) {
    return br__parse_i64_make(0, consumed, BR_STATUS_INVALID_ENCODING);
  }

  if (neg) {
    /* mag <= 2^63 here; -(2^63) is INT64_MIN and representable. */
    return br__parse_i64_make(
      mag == 0x8000000000000000uLL ? INT64_MIN : -(i64)mag, consumed, status);
  }
  return br__parse_i64_make((i64)mag, consumed, status);
}

br_parse_i64_result br_parse_i64(br_string_view s, int base) {
  return br__parse_i64_impl(s, base, true);
}

br_parse_u64_result br_parse_u64(br_string_view s, int base) {
  return br__parse_u64_impl(s, base, true);
}

br_parse_i64_result br_parse_i64_prefix(br_string_view s, int base) {
  return br__parse_i64_impl(s, base, false);
}

br_parse_u64_result br_parse_u64_prefix(br_string_view s, int base) {
  return br__parse_u64_impl(s, base, false);
}

static br_parse_i32_result br__narrow_i32(br_parse_i64_result r) {
  br_parse_i32_result out;
  out.consumed = r.consumed;
  if (r.status == BR_STATUS_OK && (r.value < INT32_MIN || r.value > INT32_MAX)) {
    out.value = r.value < 0 ? INT32_MIN : INT32_MAX;
    out.status = BR_STATUS_OUT_OF_RANGE;
    return out;
  }
  if (r.status == BR_STATUS_OUT_OF_RANGE) {
    out.value = r.value < 0 ? INT32_MIN : INT32_MAX;
    out.status = BR_STATUS_OUT_OF_RANGE;
    return out;
  }
  out.value = (i32)r.value;
  out.status = r.status;
  return out;
}

static br_parse_u32_result br__narrow_u32(br_parse_u64_result r) {
  br_parse_u32_result out;
  out.consumed = r.consumed;
  if (r.status == BR_STATUS_OK && r.value > UINT32_MAX) {
    out.value = UINT32_MAX;
    out.status = BR_STATUS_OUT_OF_RANGE;
    return out;
  }
  if (r.status == BR_STATUS_OUT_OF_RANGE) {
    out.value = UINT32_MAX;
    out.status = BR_STATUS_OUT_OF_RANGE;
    return out;
  }
  out.value = (u32)r.value;
  out.status = r.status;
  return out;
}

br_parse_i32_result br_parse_i32(br_string_view s, int base) {
  return br__narrow_i32(br_parse_i64(s, base));
}

br_parse_u32_result br_parse_u32(br_string_view s, int base) {
  return br__narrow_u32(br_parse_u64(s, base));
}

br_parse_i32_result br_parse_i32_prefix(br_string_view s, int base) {
  return br__narrow_i32(br_parse_i64_prefix(s, base));
}

br_parse_u32_result br_parse_u32_prefix(br_string_view s, int base) {
  return br__narrow_u32(br_parse_u64_prefix(s, base));
}

/*
Case-insensitive match of `word` at the front of `p`; returns its length on a
match, else 0.
*/
static usize br__match_word_ci(const u8 *p, usize len, const char *word) {
  usize i;

  for (i = 0u; word[i] != '\0'; i += 1u) {
    u8 lc;

    if (i >= len) {
      return 0u;
    }
    lc = (u8)(p[i] | 0x20u);
    if (lc != (u8)word[i]) {
      return 0u;
    }
  }
  return i;
}

/*
Scan the special float words (case-insensitive, matching Go): inf, infinity,
nan, with an optional leading sign. Returns the consumed length (0 if none) and
sets `*value`.
*/
static usize br__parse_float_special(const u8 *p, usize len, double *value) {
  usize off = 0u;
  double sign = 1.0;
  usize n;

  if (len == 0u) {
    return 0u;
  }
  if (p[0] == '+') {
    off = 1u;
  } else if (p[0] == '-') {
    off = 1u;
    sign = -1.0;
  }

  n = br__match_word_ci(p + off, len - off, "infinity");
  if (n != 0u) {
    *value = sign * (double)INFINITY;
    return off + n;
  }
  n = br__match_word_ci(p + off, len - off, "inf");
  if (n != 0u) {
    *value = sign * (double)INFINITY;
    return off + n;
  }
  n = br__match_word_ci(p + off, len - off, "nan");
  if (n != 0u) {
    *value = (double)NAN;
    return off + n;
  }
  return 0u;
}

/*
Scan a decimal float syntax: optional sign, integer/fraction digits, optional
`e`/`E` exponent. Returns consumed length (0 if the leading text is not a
number). Fills the multiprecision `d` (its sign/digits/decimal_point).
*/
static usize br__scan_float(const u8 *p, usize len, br__decimal *d) {
  usize i = 0u;
  bool saw_dot = false;
  bool saw_digits = false;

  *d = (br__decimal){0};

  if (len == 0u) {
    return 0u;
  }
  if (p[0] == '+') {
    i += 1u;
  } else if (p[0] == '-') {
    i += 1u;
    d->neg = true;
  }

  for (; i < len; i += 1u) {
    u8 c = p[i];

    if (c == '.') {
      if (saw_dot) {
        break;
      }
      saw_dot = true;
      d->decimal_point = d->count;
      continue;
    }
    if (c >= '0' && c <= '9') {
      saw_digits = true;
      if (c == '0' && d->count == 0) {
        d->decimal_point -= 1;
        continue;
      }
      if (d->count < BR__DECIMAL_MAX_DIGITS) {
        d->digits[d->count] = c;
        d->count += 1;
      } else if (c != '0') {
        d->trunc = true;
      }
      continue;
    }
    break;
  }

  if (!saw_digits) {
    return 0u;
  }
  if (!saw_dot) {
    d->decimal_point = d->count;
  }

  if (i < len && (p[i] | 0x20u) == 'e') {
    usize j = i + 1u;
    i32 exp_sign = 1;
    i32 e = 0;
    bool saw_exp_digit = false;

    if (j < len && (p[j] == '+' || p[j] == '-')) {
      if (p[j] == '-') {
        exp_sign = -1;
      }
      j += 1u;
    }
    if (j >= len || p[j] < '0' || p[j] > '9') {
      /* No exponent digits: the 'e' is trailing junk, not part of the number. */
      return i;
    }
    for (; j < len && p[j] >= '0' && p[j] <= '9'; j += 1u) {
      saw_exp_digit = true;
      if (e < 10000) {
        e = e * 10 + (i32)(p[j] - '0');
      }
    }
    if (saw_exp_digit) {
      d->decimal_point += e * exp_sign;
      i = j;
    }
  }

  return i;
}

static br_parse_f64_result br__parse_f64_impl(br_string_view s, bool strict) {
  const u8 *p = (const u8 *)s.data;
  br__decimal d;
  usize consumed;
  double special;
  u64 bits;
  bool overflow;
  double value;

  if (s.data == NULL || s.len == 0u) {
    return br__parse_f64_make(0.0, 0u, BR_STATUS_INVALID_ENCODING);
  }

  consumed = br__parse_float_special(p, s.len, &special);
  if (consumed != 0u) {
    if (strict && consumed != s.len) {
      return br__parse_f64_make(0.0, consumed, BR_STATUS_INVALID_ENCODING);
    }
    return br__parse_f64_make(special, consumed, BR_STATUS_OK);
  }

  consumed = br__scan_float(p, s.len, &d);
  if (consumed == 0u) {
    return br__parse_f64_make(0.0, 0u, BR_STATUS_INVALID_ENCODING);
  }
  if (strict && consumed != s.len) {
    return br__parse_f64_make(0.0, consumed, BR_STATUS_INVALID_ENCODING);
  }

  bits = br__decimal_to_float_bits(&d, &br__f64_info, &overflow);
  memcpy(&value, &bits, sizeof(value));
  return br__parse_f64_make(value, consumed, overflow ? BR_STATUS_OUT_OF_RANGE : BR_STATUS_OK);
}

static br_parse_f32_result br__parse_f32_impl(br_string_view s, bool strict) {
  const u8 *p = (const u8 *)s.data;
  br__decimal d;
  usize consumed;
  double special;
  u64 bits;
  u32 bits32;
  bool overflow;
  float value;

  if (s.data == NULL || s.len == 0u) {
    return br__parse_f32_make(0.0f, 0u, BR_STATUS_INVALID_ENCODING);
  }

  consumed = br__parse_float_special(p, s.len, &special);
  if (consumed != 0u) {
    if (strict && consumed != s.len) {
      return br__parse_f32_make(0.0f, consumed, BR_STATUS_INVALID_ENCODING);
    }
    return br__parse_f32_make((float)special, consumed, BR_STATUS_OK);
  }

  consumed = br__scan_float(p, s.len, &d);
  if (consumed == 0u) {
    return br__parse_f32_make(0.0f, 0u, BR_STATUS_INVALID_ENCODING);
  }
  if (strict && consumed != s.len) {
    return br__parse_f32_make(0.0f, consumed, BR_STATUS_INVALID_ENCODING);
  }

  /* Round decimal -> f32 directly (never through f64) to avoid double-rounding. */
  bits = br__decimal_to_float_bits(&d, &br__f32_info, &overflow);
  bits32 = (u32)bits;
  memcpy(&value, &bits32, sizeof(value));
  return br__parse_f32_make(value, consumed, overflow ? BR_STATUS_OUT_OF_RANGE : BR_STATUS_OK);
}

br_parse_f64_result br_parse_f64(br_string_view s) {
  return br__parse_f64_impl(s, true);
}

br_parse_f32_result br_parse_f32(br_string_view s) {
  return br__parse_f32_impl(s, true);
}

br_parse_f64_result br_parse_f64_prefix(br_string_view s) {
  return br__parse_f64_impl(s, false);
}

br_parse_f32_result br_parse_f32_prefix(br_string_view s) {
  return br__parse_f32_impl(s, false);
}

br_parse_bool_result br_parse_bool(br_string_view s) {
  br_parse_bool_result r;

  r.value = false;
  r.consumed = 0u;
  r.status = BR_STATUS_INVALID_ENCODING;

  if (s.data == NULL) {
    return r;
  }
  if (br_string_equal(s, br_string_view_from_cstr("1")) ||
      br_string_equal(s, br_string_view_from_cstr("t")) ||
      br_string_equal(s, br_string_view_from_cstr("true"))) {
    r.value = true;
    r.consumed = s.len;
    r.status = BR_STATUS_OK;
  } else if (br_string_equal(s, br_string_view_from_cstr("0")) ||
             br_string_equal(s, br_string_view_from_cstr("f")) ||
             br_string_equal(s, br_string_view_from_cstr("false"))) {
    r.value = false;
    r.consumed = s.len;
    r.status = BR_STATUS_OK;
  }
  return r;
}
