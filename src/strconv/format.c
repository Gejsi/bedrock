#include "internal.h"

#include <math.h>

static br_io_result br__format_result(usize count, br_status status) {
  br_io_result r;
  r.count = count;
  r.status = status;
  return r;
}

/*
Emit `src` (already fully computed) into `dst` atomically: if it does not fit,
nothing is written and the caller sees SHORT_BUFFER. `scratch` holds the digits
built in a bounded engine/int buffer; this is the single place that touches dst.
*/
static br_io_result br__emit(const u8 *src, usize n, u8 *dst, usize dst_cap) {
  if (dst == NULL || dst_cap < n) {
    return br__format_result(0u, BR_STATUS_SHORT_BUFFER);
  }
  memcpy(dst, src, n);
  return br__format_result(n, BR_STATUS_OK);
}

/* -------- integers -------- */

/* Worst case is base 2: 64 digits + sign. */
#define BR__FORMAT_INT_SCRATCH 66u

static br_io_result br__format_u64_base(u64 value, int base, bool neg, u8 *dst, usize dst_cap) {
  static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  u8 tmp[BR__FORMAT_INT_SCRATCH];
  u8 out[BR__FORMAT_INT_SCRATCH];
  usize n = 0u;
  usize i;
  u64 b = (u64)base;

  if (base < 2 || base > 36) {
    return br__format_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  if (value == 0u) {
    tmp[n] = '0';
    n += 1u;
  } else {
    while (value > 0u) {
      u64 q = value / b;
      tmp[n] = (u8)digits[value - q * b];
      n += 1u;
      value = q;
    }
  }
  if (neg) {
    tmp[n] = '-';
    n += 1u;
  }

  /* tmp holds the number reversed; reverse into a bounded buffer. */
  for (i = 0u; i < n; i += 1u) {
    out[i] = tmp[n - 1u - i];
  }
  return br__emit(out, n, dst, dst_cap);
}

br_io_result br_format_i64(int64_t value, int base, uint8_t *dst, size_t dst_cap) {
  if (value < 0) {
    /* Negate as unsigned so INT64_MIN does not overflow. */
    u64 mag = (u64)(-(value + 1)) + 1u;
    return br__format_u64_base(mag, base, true, dst, dst_cap);
  }
  return br__format_u64_base((u64)value, base, false, dst, dst_cap);
}

br_io_result br_format_u64(uint64_t value, int base, uint8_t *dst, size_t dst_cap) {
  return br__format_u64_base(value, base, false, dst, dst_cap);
}

br_io_result br_format_bool(bool value, uint8_t *dst, size_t dst_cap) {
  if (value) {
    return br__emit((const u8 *)"true", 4u, dst, dst_cap);
  }
  return br__emit((const u8 *)"false", 5u, dst, dst_cap);
}

/* -------- float bounds -------- */

/*
Maximum decimal integral digits for a finite value of each width: f64 tops out
near 1.8e308 (309 digits), f32 near 3.4e38 (39 digits).
*/
#define BR__F64_MAX_INT_DIGITS 309u
#define BR__F32_MAX_INT_DIGITS 39u
/* A clamp well under SIZE_MAX so a pathological prec cannot overflow the sum. */
#define BR__FORMAT_BOUND_CLAMP ((size_t)1u << 20)

static size_t br__format_float_bound(br_float_format fmt, int prec, unsigned max_int_digits) {
  size_t p;

  if (fmt == BR_FLOAT_SHORTEST) {
    return max_int_digits == BR__F64_MAX_INT_DIGITS ? BR_FORMAT_F64_SHORTEST_MAX
                                                    : BR_FORMAT_F32_SHORTEST_MAX;
  }
  /* NaN/Inf are "NaN"/"+Inf"/"-Inf" — dwarfed by the digit bounds below. */
  p = prec < 0 ? 0u : (size_t)prec;
  if (p > BR__FORMAT_BOUND_CLAMP) {
    p = BR__FORMAT_BOUND_CLAMP;
  }

  switch (fmt) {
    case BR_FLOAT_DECIMAL:
      /* sign + integer digits + '.' + prec */
      return 1u + max_int_digits + 1u + p;
    case BR_FLOAT_EXPONENT:
      /* sign + leading digit + '.' + prec + 'e' + exp sign + up to 3 exp digits */
      return 1u + 1u + 1u + p + 1u + 1u + 3u;
    case BR_FLOAT_GENERAL:
    default:
      /* The larger of the decimal and exponent shapes. */
      return 1u + max_int_digits + 1u + p;
  }
}

size_t br_format_f64_bound(br_float_format fmt, int prec) {
  return br__format_float_bound(fmt, prec, BR__F64_MAX_INT_DIGITS);
}

size_t br_format_f32_bound(br_float_format fmt, int prec) {
  return br__format_float_bound(fmt, prec, BR__F32_MAX_INT_DIGITS);
}

/* -------- float formatting -------- */

/*
Streams digits straight into `dst`, bounds-checked at every append so a full
buffer aborts before any partial write is observed by the caller (the caller
discards the result on SHORT_BUFFER). `ok` latches false on the first overflow.
*/
typedef struct br__sink {
  u8 *dst;
  usize cap;
  usize n;
  bool ok;
} br__sink;

static void br__sink_byte(br__sink *s, u8 c) {
  if (!s->ok) {
    return;
  }
  if (s->n >= s->cap) {
    s->ok = false;
    return;
  }
  s->dst[s->n] = c;
  s->n += 1u;
}

static void br__sink_zeros(br__sink *s, i32 count) {
  i32 i;
  for (i = 0; i < count; i += 1) {
    br__sink_byte(s, '0');
  }
}

/*
Assemble the rounded decimal `d` into `sink` for the effective (fmt, prec).
Mirrors the digit-placement rules of the shared engine's formatter. `fmt` here
is the resolved 'f'/'e' shape (GENERAL is resolved to one of them before call).
*/
static void br__format_digits(br__sink *sink, bool neg, const br__decimal *d, i32 prec, char fmt) {
  if (neg) {
    br__sink_byte(sink, '-');
  }

  if (fmt == 'f') {
    i32 i;

    if (d->decimal_point > 0) {
      i32 m = d->count < d->decimal_point ? d->count : d->decimal_point;
      for (i = 0; i < m; i += 1) {
        br__sink_byte(sink, d->digits[i]);
      }
      br__sink_zeros(sink, d->decimal_point - m);
    } else {
      br__sink_byte(sink, '0');
    }

    if (prec > 0) {
      br__sink_byte(sink, '.');
      for (i = 0; i < prec; i += 1) {
        i32 j = d->decimal_point + i;
        u8 c = (j >= 0 && j < d->count) ? d->digits[j] : (u8)'0';
        br__sink_byte(sink, c);
      }
    }
    return;
  }

  /* 'e' */
  {
    u8 lead = d->count != 0 ? d->digits[0] : (u8)'0';
    i32 exp;
    i32 i;

    br__sink_byte(sink, lead);
    if (prec > 0) {
      i32 m;
      br__sink_byte(sink, '.');
      i = 1;
      m = d->count < prec + 1 ? d->count : prec + 1;
      for (; i < m; i += 1) {
        br__sink_byte(sink, d->digits[i]);
      }
      for (; i <= prec; i += 1) {
        br__sink_byte(sink, '0');
      }
    }

    br__sink_byte(sink, 'e');
    exp = d->count == 0 ? 0 : d->decimal_point - 1;
    if (exp < 0) {
      br__sink_byte(sink, '-');
      exp = -exp;
    } else {
      br__sink_byte(sink, '+');
    }
    if (exp < 10) {
      br__sink_byte(sink, '0');
      br__sink_byte(sink, (u8)('0' + exp));
    } else if (exp < 100) {
      br__sink_byte(sink, (u8)('0' + exp / 10));
      br__sink_byte(sink, (u8)('0' + exp % 10));
    } else {
      br__sink_byte(sink, (u8)('0' + exp / 100));
      br__sink_byte(sink, (u8)('0' + (exp / 10) % 10));
      br__sink_byte(sink, (u8)('0' + exp % 10));
    }
  }
}

static br_io_result br__format_float(
  u64 bits, const br__float_info *info, br_float_format fmt, int prec, u8 *dst, usize dst_cap) {
  bool neg = (bits >> (info->expbits + info->mantbits)) != 0u;
  i32 exp = (i32)((bits >> info->mantbits) & (((u64)1u << info->expbits) - 1u));
  u64 mant = bits & (((u64)1u << info->mantbits) - 1u);
  br__decimal d;
  i32 eprec;
  char shape;
  br__sink sink;

  if (fmt != BR_FLOAT_SHORTEST && prec < 0) {
    return br__format_result(0u, BR_STATUS_INVALID_ARGUMENT);
  }

  /* Inf / NaN. */
  if (exp == (1 << info->expbits) - 1) {
    const char *s = mant != 0u ? "NaN" : (neg ? "-Inf" : "+Inf");
    return br__emit((const u8 *)s, strlen(s), dst, dst_cap);
  }

  if (exp == 0) {
    exp += 1; /* denormal */
  } else {
    mant |= (u64)1u << info->mantbits;
  }
  exp += info->bias;

  d = (br__decimal){0};
  br__decimal_assign(&d, mant);
  br__decimal_shift(&d, exp - (i32)info->mantbits);
  d.neg = neg;

  /*
  Round the decimal to the requested precision under the ORIGINAL mode, then
  resolve the display shape. SHORTEST rounds to the minimal round-tripping
  digits; the fixed modes round to `prec` fractional (f/e) or significant (g)
  digits; only after rounding is GENERAL resolved to an 'f' or 'e' shape.
  */
  if (fmt == BR_FLOAT_SHORTEST) {
    br__decimal_round_shortest(&d, mant, exp, info);
  } else if (fmt == BR_FLOAT_EXPONENT) {
    br__decimal_round(&d, prec + 1);
  } else if (fmt == BR_FLOAT_DECIMAL) {
    br__decimal_round(&d, d.decimal_point + prec);
  } else { /* GENERAL */
    br__decimal_round(&d, prec == 0 ? 1 : prec);
  }

  if (fmt == BR_FLOAT_SHORTEST) {
    exp = d.count == 0 ? 0 : d.decimal_point - 1;
    if (exp < -4 || exp >= 21) {
      shape = 'e';
      prec = d.count > 0 ? d.count - 1 : 0;
    } else {
      shape = 'f';
      prec = d.count - d.decimal_point;
      if (prec < 0) {
        prec = 0;
      }
    }
  } else if (fmt == BR_FLOAT_GENERAL) {
    eprec = prec;
    if (eprec > d.count && d.count >= d.decimal_point) {
      eprec = d.count;
    }
    exp = d.decimal_point - 1;
    if (exp < -4 || exp >= eprec) {
      shape = 'e';
      prec = (prec > d.count ? d.count : prec);
      prec = prec > 0 ? prec - 1 : 0;
    } else {
      shape = 'f';
      if (prec > d.decimal_point) {
        prec = d.count;
      }
      prec = prec - d.decimal_point;
      if (prec < 0) {
        prec = 0;
      }
    }
  } else {
    shape = fmt == BR_FLOAT_EXPONENT ? 'e' : 'f';
  }

  sink.dst = dst;
  sink.cap = dst_cap;
  sink.n = 0u;
  sink.ok = dst != NULL;
  br__format_digits(&sink, neg, &d, prec, shape);

  if (!sink.ok) {
    return br__format_result(0u, BR_STATUS_SHORT_BUFFER);
  }
  return br__format_result(sink.n, BR_STATUS_OK);
}

br_io_result
br_format_f64(double value, br_float_format fmt, int prec, uint8_t *dst, size_t dst_cap) {
  u64 bits;
  memcpy(&bits, &value, sizeof(bits));
  return br__format_float(bits, &br__f64_info, fmt, prec, dst, dst_cap);
}

br_io_result
br_format_f32(float value, br_float_format fmt, int prec, uint8_t *dst, size_t dst_cap) {
  u32 bits;
  memcpy(&bits, &value, sizeof(bits));
  return br__format_float((u64)bits, &br__f32_info, fmt, prec, dst, dst_cap);
}
