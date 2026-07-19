#include "internal.h"

const br__float_info br__f32_info = {23u, 8u, -127};
const br__float_info br__f64_info = {52u, 11u, -1023};

void br__decimal_trim(br__decimal *d) {
  while (d->count > 0 && d->digits[d->count - 1] == '0') {
    d->count -= 1;
  }
  if (d->count == 0) {
    d->decimal_point = 0;
  }
}

void br__decimal_assign(br__decimal *d, u64 value) {
  u8 buf[24];
  i32 n = 0;
  u64 v = value;

  while (v > 0u) {
    u64 q = v / 10u;
    buf[n] = (u8)('0' + (v - 10u * q));
    n += 1;
    v = q;
  }

  d->count = 0;
  for (n -= 1; n >= 0; n -= 1) {
    d->digits[d->count] = buf[n];
    d->count += 1;
  }
  d->decimal_point = d->count;
  br__decimal_trim(d);
}

static void br__decimal_shift_right(br__decimal *d, u32 k) {
  i32 r = 0;
  i32 w = 0;
  u64 n = 0u;
  u64 mask;

  for (; (n >> k) == 0u; r += 1) {
    if (r >= d->count) {
      if (n == 0u) {
        d->count = 0;
        return;
      }
      while ((n >> k) == 0u) {
        n *= 10u;
        r += 1;
      }
      break;
    }
    n = n * 10u + (u64)(d->digits[r] - '0');
  }
  d->decimal_point -= r - 1;

  mask = ((u64)1u << k) - 1u;

  for (; r < d->count; r += 1) {
    u64 c = (u64)(d->digits[r] - '0');
    u64 dig = n >> k;
    n &= mask;
    d->digits[w] = (u8)('0' + dig);
    w += 1;
    n = n * 10u + c;
  }

  while (n > 0u) {
    u64 dig = n >> k;
    n &= mask;
    if (w < BR__DECIMAL_MAX_DIGITS) {
      d->digits[w] = (u8)('0' + dig);
      w += 1;
    } else if (dig > 0u) {
      d->trunc = true;
    }
    n *= 10u;
  }

  d->count = w;
  br__decimal_trim(d);
}

typedef struct br__shift_left_offset {
  i32 delta;
  const char *cutoff;
} br__shift_left_offset;

static const br__shift_left_offset br__shift_left_offsets[61] = {
  {0, ""},
  {1, "5"},
  {1, "25"},
  {1, "125"},
  {2, "625"},
  {2, "3125"},
  {2, "15625"},
  {3, "78125"},
  {3, "390625"},
  {3, "1953125"},
  {4, "9765625"},
  {4, "48828125"},
  {4, "244140625"},
  {4, "1220703125"},
  {5, "6103515625"},
  {5, "30517578125"},
  {5, "152587890625"},
  {6, "762939453125"},
  {6, "3814697265625"},
  {6, "19073486328125"},
  {7, "95367431640625"},
  {7, "476837158203125"},
  {7, "2384185791015625"},
  {7, "11920928955078125"},
  {8, "59604644775390625"},
  {8, "298023223876953125"},
  {8, "1490116119384765625"},
  {9, "7450580596923828125"},
  {9, "37252902984619140625"},
  {9, "186264514923095703125"},
  {10, "931322574615478515625"},
  {10, "4656612873077392578125"},
  {10, "23283064365386962890625"},
  {10, "116415321826934814453125"},
  {11, "582076609134674072265625"},
  {11, "2910383045673370361328125"},
  {11, "14551915228366851806640625"},
  {12, "72759576141834259033203125"},
  {12, "363797880709171295166015625"},
  {12, "1818989403545856475830078125"},
  {13, "9094947017729282379150390625"},
  {13, "45474735088646411895751953125"},
  {13, "227373675443232059478759765625"},
  {13, "1136868377216160297393798828125"},
  {14, "5684341886080801486968994140625"},
  {14, "28421709430404007434844970703125"},
  {14, "142108547152020037174224853515625"},
  {15, "710542735760100185871124267578125"},
  {15, "3552713678800500929355621337890625"},
  {15, "17763568394002504646778106689453125"},
  {16, "88817841970012523233890533447265625"},
  {16, "444089209850062616169452667236328125"},
  {16, "2220446049250313080847263336181640625"},
  {16, "11102230246251565404236316680908203125"},
  {17, "55511151231257827021181583404541015625"},
  {17, "277555756156289135105907917022705078125"},
  {17, "1387778780781445675529539585113525390625"},
  {18, "6938893903907228377647697925567626953125"},
  {18, "34694469519536141888238489627838134765625"},
  {18, "173472347597680709441192448139190673828125"},
  {19, "867361737988403547205962240695953369140625"},
};

static bool br__decimal_prefix_less(const u8 *b, i32 count, const char *s) {
  i32 i;

  for (i = 0; s[i] != '\0'; i += 1) {
    if (i >= count) {
      return true;
    }
    if (b[i] != (u8)s[i]) {
      return b[i] < (u8)s[i];
    }
  }
  return false;
}

static void br__decimal_shift_left(br__decimal *d, u32 k) {
  i32 delta = br__shift_left_offsets[k].delta;
  const char *cutoff = br__shift_left_offsets[k].cutoff;
  i32 read_index = d->count;
  i32 write_index;
  u64 n = 0u;

  if (br__decimal_prefix_less(d->digits, d->count, cutoff)) {
    delta -= 1;
  }

  write_index = d->count + delta;

  for (read_index -= 1; read_index >= 0; read_index -= 1) {
    u64 quo;
    u64 rem;

    n += ((u64)(d->digits[read_index] - '0')) << k;
    quo = n / 10u;
    rem = n - 10u * quo;
    write_index -= 1;
    if (write_index < BR__DECIMAL_MAX_DIGITS) {
      d->digits[write_index] = (u8)('0' + rem);
    } else if (rem != 0u) {
      d->trunc = true;
    }
    n = quo;
  }

  while (n > 0u) {
    u64 quo = n / 10u;
    u64 rem = n - 10u * quo;
    write_index -= 1;
    if (write_index < BR__DECIMAL_MAX_DIGITS) {
      d->digits[write_index] = (u8)('0' + rem);
    } else if (rem != 0u) {
      d->trunc = true;
    }
    n = quo;
  }

  d->decimal_point += delta;
  d->count += delta;
  if (d->count < 0) {
    d->count = 0;
  } else if (d->count > BR__DECIMAL_MAX_DIGITS) {
    d->count = BR__DECIMAL_MAX_DIGITS;
  }
  br__decimal_trim(d);
}

void br__decimal_shift(br__decimal *d, i32 shift) {
  const u32 max_shift = (u32)(8 * (i32)sizeof(u32)) - 4u; /* 28 */

  if (d->count == 0) {
    return;
  }
  if (shift > 0) {
    while ((u32)shift > max_shift) {
      br__decimal_shift_left(d, max_shift);
      shift -= (i32)max_shift;
    }
    br__decimal_shift_left(d, (u32)shift);
  } else if (shift < 0) {
    while (shift < -(i32)max_shift) {
      br__decimal_shift_right(d, max_shift);
      shift += (i32)max_shift;
    }
    br__decimal_shift_right(d, (u32)(-shift));
  }
}

bool br__decimal_can_round_up(const br__decimal *d, i32 nd) {
  if (nd < 0 || nd >= d->count) {
    return false;
  }
  if (d->digits[nd] == '5' && nd + 1 == d->count) {
    if (d->trunc) {
      return true;
    }
    return nd > 0 && ((d->digits[nd - 1] - '0') % 2) != 0;
  }
  return d->digits[nd] >= '5';
}

void br__decimal_round_up(br__decimal *d, i32 nd) {
  i32 i;

  if (nd < 0 || nd >= d->count) {
    return;
  }
  for (i = nd - 1; i >= 0; i -= 1) {
    if (d->digits[i] < '9') {
      d->digits[i] += 1;
      d->count = i + 1;
      return;
    }
  }
  d->digits[0] = '1';
  d->count = 1;
  d->decimal_point += 1;
}

void br__decimal_round_down(br__decimal *d, i32 nd) {
  if (nd < 0 || nd >= d->count) {
    return;
  }
  d->count = nd;
  br__decimal_trim(d);
}

void br__decimal_round(br__decimal *d, i32 nd) {
  if (nd < 0 || nd >= d->count) {
    return;
  }
  if (br__decimal_can_round_up(d, nd)) {
    br__decimal_round_up(d, nd);
  } else {
    br__decimal_round_down(d, nd);
  }
}

u64 br__decimal_rounded_integer(const br__decimal *d) {
  i32 i = 0;
  u64 n = 0u;
  i32 m;

  if (d->decimal_point > 20) {
    return 0xFFFFFFFFFFFFFFFFuLL;
  }
  m = d->decimal_point < d->count ? d->decimal_point : d->count;
  for (; i < m; i += 1) {
    n = n * 10u + (u64)(d->digits[i] - '0');
  }
  for (; i < d->decimal_point; i += 1) {
    n *= 10u;
  }
  if (br__decimal_can_round_up(d, d->decimal_point)) {
    n += 1u;
  }
  return n;
}

void br__decimal_round_shortest(br__decimal *d, u64 mant, i32 exp, const br__float_info *info) {
  i32 minexp = info->bias + 1;
  br__decimal upper;
  br__decimal lower;
  u64 mantlo;
  i32 explo;
  bool inclusive;
  i32 i;

  if (mant == 0u) {
    d->count = 0;
    return;
  }

  if (exp > minexp && 332 * (d->decimal_point - d->count) >= 100 * (exp - (i32)info->mantbits)) {
    return;
  }

  upper = (br__decimal){0};
  br__decimal_assign(&upper, 2u * mant + 1u);
  br__decimal_shift(&upper, exp - (i32)info->mantbits - 1);

  if (mant > ((u64)1u << info->mantbits) || exp == minexp) {
    mantlo = mant - 1u;
    explo = exp;
  } else {
    mantlo = 2u * mant - 1u;
    explo = exp - 1;
  }
  lower = (br__decimal){0};
  br__decimal_assign(&lower, 2u * mantlo + 1u);
  br__decimal_shift(&lower, explo - (i32)info->mantbits - 1);

  inclusive = (mant % 2u) == 0u;

  for (i = 0; i < d->count; i += 1) {
    u8 l = i < lower.count ? lower.digits[i] : (u8)'0';
    u8 m = d->digits[i];
    u8 u = i < upper.count ? upper.digits[i] : (u8)'0';
    bool ok_round_down = l != m || (inclusive && i + 1 == lower.count);
    bool ok_round_up = m != u && (inclusive || m + 1 < u || i + 1 < upper.count);

    if (ok_round_down && ok_round_up) {
      br__decimal_round(d, i + 1);
      return;
    }
    if (ok_round_down) {
      br__decimal_round_down(d, i + 1);
      return;
    }
    if (ok_round_up) {
      br__decimal_round_up(d, i + 1);
      return;
    }
  }
}

static u64
br__decimal_float_end(const br__decimal *d, u64 mant, i32 exp, const br__float_info *info) {
  u64 bits = mant & (((u64)1u << info->mantbits) - 1u);

  bits |= (u64)((exp - info->bias) & ((1 << info->expbits) - 1)) << info->mantbits;
  if (d->neg) {
    bits |= (u64)1u << info->mantbits << info->expbits;
  }
  return bits;
}

static u64 br__decimal_float_overflow(const br__decimal *d, const br__float_info *info) {
  i32 exp = (1 << info->expbits) - 1 + info->bias;

  return br__decimal_float_end(d, 0u, exp, info);
}

u64 br__decimal_to_float_bits(br__decimal *d, const br__float_info *info, bool *overflow) {
  static const i32 power_table[9] = {1, 3, 6, 9, 13, 16, 19, 23, 26};
  i32 exp = 0;
  u64 mant;

  *overflow = false;

  if (d->count == 0) {
    return br__decimal_float_end(d, 0u, info->bias, info);
  }

  if (d->decimal_point > 310) {
    *overflow = true;
    return br__decimal_float_overflow(d, info);
  }
  if (d->decimal_point < -330) {
    return br__decimal_float_end(d, 0u, info->bias, info);
  }

  while (d->decimal_point > 0) {
    i32 n = d->decimal_point >= 9 ? 27 : power_table[d->decimal_point];
    br__decimal_shift(d, -n);
    exp += n;
  }
  while (d->decimal_point < 0 || (d->decimal_point == 0 && d->digits[0] < '5')) {
    i32 n = (-d->decimal_point) >= 9 ? 27 : power_table[-d->decimal_point];
    br__decimal_shift(d, n);
    exp -= n;
  }

  exp -= 1;

  if (exp < info->bias + 1) {
    i32 n = info->bias + 1 - exp;
    br__decimal_shift(d, -n);
    exp += n;
  }

  if ((exp - info->bias) >= ((1 << info->expbits) - 1)) {
    *overflow = true;
    return br__decimal_float_overflow(d, info);
  }

  br__decimal_shift(d, (i32)(1u + info->mantbits));
  mant = br__decimal_rounded_integer(d);

  if (mant == ((u64)2u << info->mantbits)) {
    mant >>= 1;
    exp += 1;
    if ((exp - info->bias) >= ((1 << info->expbits) - 1)) {
      *overflow = true;
      return br__decimal_float_overflow(d, info);
    }
  }

  if ((mant & ((u64)1u << info->mantbits)) == 0u) {
    exp = info->bias;
  }

  return br__decimal_float_end(d, mant, exp, info);
}
