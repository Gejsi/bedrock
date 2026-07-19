#include <bedrock/math/bits.h>

/*
Three-tier implementation of the count/scan/byteswap primitives:

  Tier 1: C23 <stdbit.h>          (stdc_leading_zeros, stdc_count_ones, ...)
  Tier 2: compiler intrinsics     (gcc/clang __builtin_*, MSVC _BitScan / __popcnt)
  Tier 3: portable C fallback     (de Bruijn, bit tricks, the 256-byte table)

Define BR_BITS_FORCE_FALLBACK to compile only Tier 3 regardless of platform;
the test suite uses this to differentially check the fallback against whatever
native tier the host actually provides.

Invariant across all tiers: x == 0 is guarded FIRST, because the compiler
builtins for clz/ctz are undefined at 0. The public counts/scans are total.
*/

#if !defined(BR_BITS_FORCE_FALLBACK)
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L) && defined(__has_include)
#if __has_include(<stdbit.h>)
#define BR__BITS_HAVE_STDBIT 1
#include <stdbit.h>
#endif
#endif
#endif

#if !defined(BR_BITS_FORCE_FALLBACK) && !defined(BR__BITS_HAVE_STDBIT)
#if defined(__GNUC__) || defined(__clang__)
#define BR__BITS_HAVE_GNU 1
#elif defined(_MSC_VER)
#define BR__BITS_HAVE_MSVC 1
#include <intrin.h>
#endif
#endif

/* Gate the portable helpers so they are not dead code under a native tier
   (-Werror would reject an unused static). Two independent needs:
   - popcount/scan portable path: used only when NO count/scan tier exists
     (no stdbit, no GNU, no MSVC).
   - byteswap portable path: byteswap has only GNU/MSVC intrinsics (stdbit has
     no byteswap), so it is used whenever neither of those is present.
   The bit_width table helper is always used, so it is never gated. */
#if !defined(BR__BITS_HAVE_STDBIT) && !defined(BR__BITS_HAVE_GNU) && !defined(BR__BITS_HAVE_MSVC)
#define BR__BITS_NEED_PORTABLE_COUNTS 1
#endif
#if !defined(BR__BITS_HAVE_GNU) && !defined(BR__BITS_HAVE_MSVC)
#define BR__BITS_NEED_PORTABLE_BYTESWAP 1
#endif

/*
The 256-entry bit-width table: table[x] is the minimum number of bits needed to
represent the byte x (0 -> 0, 1 -> 1, 255 -> 8). Used by the portable bit_width
fallback, byte at a time. Ported from Odin/Go's len8tab.
*/
static const u8 br__bits_width_table[256] = {
  0x00, 0x01, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
  0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08};

/* ---- Portable primitives. The popcount/byteswap helpers back the fallback
   tier only (gated so they are not dead code under a native tier); the
   bit_width helper is always used. ---- */

#if defined(BR__BITS_NEED_PORTABLE_COUNTS)
static int br__bits_popcount64_portable(u64 x) {
  /* Parallel bit count (SWAR). */
  x = x - ((x >> 1) & 0x5555555555555555ull);
  x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
  x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0full;
  return (int)((x * 0x0101010101010101ull) >> 56);
}
#endif

static int br__bits_bit_width64_portable(u64 x) {
  int n = 0;
  if (x >= (u64)1 << 32) {
    x >>= 32;
    n = 32;
  }
  if (x >= (u64)1 << 16) {
    x >>= 16;
    n += 16;
  }
  if (x >= (u64)1 << 8) {
    x >>= 8;
    n += 8;
  }
  return n + (int)br__bits_width_table[x];
}

#if defined(BR__BITS_NEED_PORTABLE_BYTESWAP)
static u64 br__bits_byteswap64_portable(u64 x) {
  x = ((x & 0x00ff00ff00ff00ffull) << 8) | ((x >> 8) & 0x00ff00ff00ff00ffull);
  x = ((x & 0x0000ffff0000ffffull) << 16) | ((x >> 16) & 0x0000ffff0000ffffull);
  return (x << 32) | (x >> 32);
}
#endif

/* ---- Core primitives, dispatched by tier. Each takes a value already known to
   be handled for the x==0 case by the caller where noted. ---- */

static int br__bits_popcount64(u64 x) {
#if defined(BR__BITS_HAVE_STDBIT)
  return (int)stdc_count_ones_ull(x);
#elif defined(BR__BITS_HAVE_GNU)
  return __builtin_popcountll(x);
#elif defined(BR__BITS_HAVE_MSVC)
  return (int)__popcnt64(x);
#else
  return br__bits_popcount64_portable(x);
#endif
}

/* Count leading zeros of x within a 64-bit word. Caller guarantees x != 0. */
static int br__bits_clz64_nonzero(u64 x) {
#if defined(BR__BITS_HAVE_STDBIT)
  return (int)stdc_leading_zeros_ull(x);
#elif defined(BR__BITS_HAVE_GNU)
  return __builtin_clzll(x);
#elif defined(BR__BITS_HAVE_MSVC)
  unsigned long index;
  _BitScanReverse64(&index, x);
  return 63 - (int)index;
#else
  return 64 - br__bits_bit_width64_portable(x);
#endif
}

/* Count trailing zeros of x within a 64-bit word. Caller guarantees x != 0. */
static int br__bits_ctz64_nonzero(u64 x) {
#if defined(BR__BITS_HAVE_STDBIT)
  return (int)stdc_trailing_zeros_ull(x);
#elif defined(BR__BITS_HAVE_GNU)
  return __builtin_ctzll(x);
#elif defined(BR__BITS_HAVE_MSVC)
  unsigned long index;
  _BitScanForward64(&index, x);
  return (int)index;
#else
  /* Isolate lowest set bit, then its bit position via popcount of (bit - 1).
     Negate in unsigned arithmetic: -(i64)x overflows for x with bit 63 set
     (UB), whereas the unsigned 0 - x is well-defined two's-complement. */
  return br__bits_popcount64_portable((x & (~x + 1u)) - 1u);
#endif
}

/* ---- count_ones ---- */

int br_bits_count_ones_u8(u8 x) {
  return br__bits_popcount64(x);
}
int br_bits_count_ones_u16(u16 x) {
  return br__bits_popcount64(x);
}
int br_bits_count_ones_u32(u32 x) {
  return br__bits_popcount64(x);
}
int br_bits_count_ones_u64(u64 x) {
  return br__bits_popcount64(x);
}

/* ---- count_zeros = width - count_ones ---- */

int br_bits_count_zeros_u8(u8 x) {
  return 8 - br__bits_popcount64(x);
}
int br_bits_count_zeros_u16(u16 x) {
  return 16 - br__bits_popcount64(x);
}
int br_bits_count_zeros_u32(u32 x) {
  return 32 - br__bits_popcount64(x);
}
int br_bits_count_zeros_u64(u64 x) {
  return 64 - br__bits_popcount64(x);
}

/* ---- clz (leading zeros); returns width at 0 ---- */

int br_bits_clz_u8(u8 x) {
  if (x == 0u) {
    return 8;
  }
  return br__bits_clz64_nonzero(x) - 56;
}
int br_bits_clz_u16(u16 x) {
  if (x == 0u) {
    return 16;
  }
  return br__bits_clz64_nonzero(x) - 48;
}
int br_bits_clz_u32(u32 x) {
  if (x == 0u) {
    return 32;
  }
  return br__bits_clz64_nonzero(x) - 32;
}
int br_bits_clz_u64(u64 x) {
  if (x == 0u) {
    return 64;
  }
  return br__bits_clz64_nonzero(x);
}

/* ---- ctz (trailing zeros); returns width at 0 ---- */

int br_bits_ctz_u8(u8 x) {
  if (x == 0u) {
    return 8;
  }
  return br__bits_ctz64_nonzero(x);
}
int br_bits_ctz_u16(u16 x) {
  if (x == 0u) {
    return 16;
  }
  return br__bits_ctz64_nonzero(x);
}
int br_bits_ctz_u32(u32 x) {
  if (x == 0u) {
    return 32;
  }
  return br__bits_ctz64_nonzero(x);
}
int br_bits_ctz_u64(u64 x) {
  if (x == 0u) {
    return 64;
  }
  return br__bits_ctz64_nonzero(x);
}

/* ---- bit_width; returns 0 at 0 (matches C23 stdc_bit_width) ---- */

int br_bits_bit_width_u8(u8 x) {
  return (int)br__bits_width_table[x];
}
int br_bits_bit_width_u16(u16 x) {
  return br__bits_bit_width64_portable(x);
}
int br_bits_bit_width_u32(u32 x) {
  return br__bits_bit_width64_portable(x);
}
int br_bits_bit_width_u64(u64 x) {
  return br__bits_bit_width64_portable(x);
}

/* ---- reverse_bits (portable; no widespread intrinsic) ---- */

u8 br_bits_reverse_u8(u8 x) {
  x = (u8)(((x & 0xf0u) >> 4) | ((x & 0x0fu) << 4));
  x = (u8)(((x & 0xccu) >> 2) | ((x & 0x33u) << 2));
  x = (u8)(((x & 0xaau) >> 1) | ((x & 0x55u) << 1));
  return x;
}
u16 br_bits_reverse_u16(u16 x) {
  x = (u16)((x >> 8) | (x << 8));
  x = (u16)(((x & 0xf0f0u) >> 4) | ((x & 0x0f0fu) << 4));
  x = (u16)(((x & 0xccccu) >> 2) | ((x & 0x3333u) << 2));
  x = (u16)(((x & 0xaaaau) >> 1) | ((x & 0x5555u) << 1));
  return x;
}
u32 br_bits_reverse_u32(u32 x) {
  x = (x >> 16) | (x << 16);
  x = ((x & 0xff00ff00u) >> 8) | ((x & 0x00ff00ffu) << 8);
  x = ((x & 0xf0f0f0f0u) >> 4) | ((x & 0x0f0f0f0fu) << 4);
  x = ((x & 0xccccccccu) >> 2) | ((x & 0x33333333u) << 2);
  x = ((x & 0xaaaaaaaau) >> 1) | ((x & 0x55555555u) << 1);
  return x;
}
u64 br_bits_reverse_u64(u64 x) {
  x = (x >> 32) | (x << 32);
  x = ((x & 0xffff0000ffff0000ull) >> 16) | ((x & 0x0000ffff0000ffffull) << 16);
  x = ((x & 0xff00ff00ff00ff00ull) >> 8) | ((x & 0x00ff00ff00ff00ffull) << 8);
  x = ((x & 0xf0f0f0f0f0f0f0f0ull) >> 4) | ((x & 0x0f0f0f0f0f0f0f0full) << 4);
  x = ((x & 0xccccccccccccccccull) >> 2) | ((x & 0x3333333333333333ull) << 2);
  x = ((x & 0xaaaaaaaaaaaaaaaaull) >> 1) | ((x & 0x5555555555555555ull) << 1);
  return x;
}

/* ---- byteswap ---- */

u16 br_bits_byteswap_u16(u16 x) {
#if defined(BR__BITS_HAVE_GNU)
  return __builtin_bswap16(x);
#elif defined(BR__BITS_HAVE_MSVC)
  return _byteswap_ushort(x);
#else
  return (u16)((x >> 8) | (x << 8));
#endif
}
u32 br_bits_byteswap_u32(u32 x) {
#if defined(BR__BITS_HAVE_GNU)
  return __builtin_bswap32(x);
#elif defined(BR__BITS_HAVE_MSVC)
  return _byteswap_ulong(x);
#else
  return (u32)(br__bits_byteswap64_portable(x) >> 32);
#endif
}
u64 br_bits_byteswap_u64(u64 x) {
#if defined(BR__BITS_HAVE_GNU)
  return __builtin_bswap64(x);
#elif defined(BR__BITS_HAVE_MSVC)
  return _byteswap_uint64(x);
#else
  return br__bits_byteswap64_portable(x);
#endif
}

/* ---- rotations (k mod width; the mask keeps the shift well-defined) ---- */

u8 br_bits_rotate_left_u8(u8 x, unsigned k) {
  unsigned s = k & 7u;
  if (s == 0u) {
    return x;
  }
  return (u8)((x << s) | (x >> (8u - s)));
}
u16 br_bits_rotate_left_u16(u16 x, unsigned k) {
  unsigned s = k & 15u;
  if (s == 0u) {
    return x;
  }
  return (u16)((x << s) | (x >> (16u - s)));
}
u32 br_bits_rotate_left_u32(u32 x, unsigned k) {
  unsigned s = k & 31u;
  if (s == 0u) {
    return x;
  }
  return (x << s) | (x >> (32u - s));
}
u64 br_bits_rotate_left_u64(u64 x, unsigned k) {
  unsigned s = k & 63u;
  if (s == 0u) {
    return x;
  }
  return (x << s) | (x >> (64u - s));
}

u8 br_bits_rotate_right_u8(u8 x, unsigned k) {
  return br_bits_rotate_left_u8(x, 8u - (k & 7u));
}
u16 br_bits_rotate_right_u16(u16 x, unsigned k) {
  return br_bits_rotate_left_u16(x, 16u - (k & 15u));
}
u32 br_bits_rotate_right_u32(u32 x, unsigned k) {
  return br_bits_rotate_left_u32(x, 32u - (k & 31u));
}
u64 br_bits_rotate_right_u64(u64 x, unsigned k) {
  return br_bits_rotate_left_u64(x, 64u - (k & 63u));
}

/* ---- is_power_of_two ---- */

bool br_bits_is_power_of_two_u8(u8 x) {
  return x != 0u && (x & (u8)(x - 1u)) == 0u;
}
bool br_bits_is_power_of_two_u16(u16 x) {
  return x != 0u && (x & (u16)(x - 1u)) == 0u;
}
bool br_bits_is_power_of_two_u32(u32 x) {
  return x != 0u && (x & (x - 1u)) == 0u;
}
bool br_bits_is_power_of_two_u64(u64 x) {
  return x != 0u && (x & (x - 1u)) == 0u;
}
bool br_bits_is_power_of_two_i8(i8 x) {
  return x > 0 && (x & (i8)(x - 1)) == 0;
}
bool br_bits_is_power_of_two_i16(i16 x) {
  return x > 0 && (x & (i16)(x - 1)) == 0;
}
bool br_bits_is_power_of_two_i32(i32 x) {
  return x > 0 && (x & (x - 1)) == 0;
}
bool br_bits_is_power_of_two_i64(i64 x) {
  return x > 0 && (x & (x - 1)) == 0;
}

/* ---- add / sub with carry / borrow ---- */

br_bits_add_u32_result br_bits_add_u32(u32 x, u32 y, bool carry_in) {
  br_bits_add_u32_result result;
  u64 sum = (u64)x + (u64)y + (u64)(carry_in ? 1u : 0u);

  result.sum = (u32)sum;
  result.carry_out = (sum >> 32) != 0u;
  return result;
}

br_bits_add_u64_result br_bits_add_u64(u64 x, u64 y, bool carry_in) {
  br_bits_add_u64_result result;
  u64 sum = x + y + (carry_in ? 1u : 0u);

  result.sum = sum;
  /* Carry out per Go's bits.Add64: a bit is carried where both inputs set it,
     or where either input set it and the sum did not. */
  result.carry_out = (((x & y) | ((x | y) & ~sum)) >> 63) != 0u;
  return result;
}

br_bits_sub_u32_result br_bits_sub_u32(u32 x, u32 y, bool borrow_in) {
  br_bits_sub_u32_result result;
  u64 diff = (u64)x - (u64)y - (u64)(borrow_in ? 1u : 0u);

  result.diff = (u32)diff;
  result.borrow_out = (diff >> 32) != 0u;
  return result;
}

br_bits_sub_u64_result br_bits_sub_u64(u64 x, u64 y, bool borrow_in) {
  br_bits_sub_u64_result result;
  u64 b = borrow_in ? 1u : 0u;
  u64 diff = x - y - b;

  result.diff = diff;
  /* Borrow out per Go's bits.Sub64. */
  result.borrow_out = (((~x & y) | (~(x ^ y) & diff)) >> 63) != 0u;
  return result;
}

/* ---- full-width multiply (hi, lo) ---- */

br_bits_mul_u32_result br_bits_mul_u32(u32 x, u32 y) {
  br_bits_mul_u32_result result;
  u64 product = (u64)x * (u64)y;

  result.hi = (u32)(product >> 32);
  result.lo = (u32)product;
  return result;
}

br_bits_mul_u64_result br_bits_mul_u64(u64 x, u64 y) {
  br_bits_mul_u64_result result;
#if defined(__SIZEOF_INT128__)
  unsigned __int128 product = (unsigned __int128)x * (unsigned __int128)y;
  result.hi = (u64)(product >> 64);
  result.lo = (u64)product;
#else
  /* Portable dual-word multiply (Go's bits.Mul64). */
  const u64 mask32 = 0xffffffffull;
  u64 x0 = x & mask32;
  u64 x1 = x >> 32;
  u64 y0 = y & mask32;
  u64 y1 = y >> 32;

  u64 w0 = x0 * y0;
  u64 t = x1 * y0 + (w0 >> 32);
  u64 w1 = t & mask32;
  u64 w2 = t >> 32;

  w1 += x0 * y1;

  result.hi = x1 * y1 + w2 + (w1 >> 32);
  result.lo = x * y;
#endif
  return result;
}

/* ---- divide (double-width / single) returning status, never panicking ---- */

br_bits_div_u32_result br_bits_div_u32(u32 hi, u32 lo, u32 y) {
  br_bits_div_u32_result result;
  u64 z;

  if (y == 0u || y <= hi) {
    result.quo = 0u;
    result.rem = 0u;
    result.status = BR_STATUS_INVALID_ARGUMENT;
    return result;
  }

  z = ((u64)hi << 32) | (u64)lo;
  result.quo = (u32)(z / (u64)y);
  result.rem = (u32)(z % (u64)y);
  result.status = BR_STATUS_OK;
  return result;
}

br_bits_div_u64_result br_bits_div_u64(u64 hi, u64 lo, u64 y) {
  br_bits_div_u64_result result;

  if (y == 0u || y <= hi) {
    result.quo = 0u;
    result.rem = 0u;
    result.status = BR_STATUS_INVALID_ARGUMENT;
    return result;
  }

#if defined(__SIZEOF_INT128__)
  {
    unsigned __int128 z = ((unsigned __int128)hi << 64) | (unsigned __int128)lo;
    result.quo = (u64)(z / y);
    result.rem = (u64)(z % y);
  }
#else
  {
    /* Knuth Algorithm D for a 128-by-64 divide (Go's bits.Div64). */
    const u64 two32 = (u64)1 << 32;
    const u64 mask32 = two32 - 1u;
    unsigned s = (unsigned)br_bits_clz_u64(y);
    u64 yn1, yn0, un32, un10, un1, un0, q1, q0, rhat, un21;

    y <<= s;
    yn1 = y >> 32;
    yn0 = y & mask32;
    un32 = (hi << s) | (lo >> (64u - s));
    un10 = lo << s;
    un1 = un10 >> 32;
    un0 = un10 & mask32;
    q1 = un32 / yn1;
    rhat = un32 - q1 * yn1;

    while (q1 >= two32 || q1 * yn0 > two32 * rhat + un1) {
      q1 -= 1u;
      rhat += yn1;
      if (rhat >= two32) {
        break;
      }
    }

    un21 = un32 * two32 + un1 - q1 * y;
    q0 = un21 / yn1;
    rhat = un21 - q0 * yn1;

    while (q0 >= two32 || q0 * yn0 > two32 * rhat + un0) {
      q0 -= 1u;
      rhat += yn1;
      if (rhat >= two32) {
        break;
      }
    }

    result.quo = q1 * two32 + q0;
    result.rem = (un21 * two32 + un0 - q0 * y) >> s;
  }
#endif
  result.status = BR_STATUS_OK;
  return result;
}

/* ---- bitfields ----
   mask(bits) built as ((1<<bits)-1) but guarding bits==width, where the shift
   would be undefined. field_extract shifts down then masks; the signed variants
   sign-extend with the (v ^ m) - m trick. field_insert clears then deposits. */

#define BR__BITS_UMASK(TYPE, BITS, WIDTH)                                                          \
  ((BITS) >= (WIDTH) ? (TYPE) ~(TYPE)0 : (TYPE)(((TYPE)1 << (BITS)) - (TYPE)1))

u8 br_bits_field_extract_u8(u8 value, unsigned offset, unsigned bits) {
  return (u8)((value >> offset) & BR__BITS_UMASK(u8, bits, 8u));
}
u16 br_bits_field_extract_u16(u16 value, unsigned offset, unsigned bits) {
  return (u16)((value >> offset) & BR__BITS_UMASK(u16, bits, 16u));
}
u32 br_bits_field_extract_u32(u32 value, unsigned offset, unsigned bits) {
  return (value >> offset) & BR__BITS_UMASK(u32, bits, 32u);
}
u64 br_bits_field_extract_u64(u64 value, unsigned offset, unsigned bits) {
  return (value >> offset) & BR__BITS_UMASK(u64, bits, 64u);
}

i8 br_bits_field_extract_i8(i8 value, unsigned offset, unsigned bits) {
  u8 field = br_bits_field_extract_u8((u8)value, offset, bits);
  u8 sign = (u8)(bits == 0u ? 0u : (u8)1 << (bits - 1u));
  return (i8)((u8)(field ^ sign) - sign);
}
i16 br_bits_field_extract_i16(i16 value, unsigned offset, unsigned bits) {
  u16 field = br_bits_field_extract_u16((u16)value, offset, bits);
  u16 sign = (u16)(bits == 0u ? 0u : (u16)1 << (bits - 1u));
  return (i16)((u16)(field ^ sign) - sign);
}
i32 br_bits_field_extract_i32(i32 value, unsigned offset, unsigned bits) {
  u32 field = br_bits_field_extract_u32((u32)value, offset, bits);
  u32 sign = bits == 0u ? 0u : (u32)1 << (bits - 1u);
  return (i32)((field ^ sign) - sign);
}
i64 br_bits_field_extract_i64(i64 value, unsigned offset, unsigned bits) {
  u64 field = br_bits_field_extract_u64((u64)value, offset, bits);
  u64 sign = bits == 0u ? 0u : (u64)1 << (bits - 1u);
  return (i64)((field ^ sign) - sign);
}

u8 br_bits_field_insert_u8(u8 base, u8 insert, unsigned offset, unsigned bits) {
  u8 mask = (u8)(BR__BITS_UMASK(u8, bits, 8u) << offset);
  return (u8)((base & (u8)~mask) | (((u8)(insert << offset)) & mask));
}
u16 br_bits_field_insert_u16(u16 base, u16 insert, unsigned offset, unsigned bits) {
  u16 mask = (u16)(BR__BITS_UMASK(u16, bits, 16u) << offset);
  return (u16)((base & (u16)~mask) | (((u16)(insert << offset)) & mask));
}
u32 br_bits_field_insert_u32(u32 base, u32 insert, unsigned offset, unsigned bits) {
  u32 mask = BR__BITS_UMASK(u32, bits, 32u) << offset;
  return (base & ~mask) | ((insert << offset) & mask);
}
u64 br_bits_field_insert_u64(u64 base, u64 insert, unsigned offset, unsigned bits) {
  u64 mask = BR__BITS_UMASK(u64, bits, 64u) << offset;
  return (base & ~mask) | ((insert << offset) & mask);
}

i8 br_bits_field_insert_i8(i8 base, i8 insert, unsigned offset, unsigned bits) {
  return (i8)br_bits_field_insert_u8((u8)base, (u8)insert, offset, bits);
}
i16 br_bits_field_insert_i16(i16 base, i16 insert, unsigned offset, unsigned bits) {
  return (i16)br_bits_field_insert_u16((u16)base, (u16)insert, offset, bits);
}
i32 br_bits_field_insert_i32(i32 base, i32 insert, unsigned offset, unsigned bits) {
  return (i32)br_bits_field_insert_u32((u32)base, (u32)insert, offset, bits);
}
i64 br_bits_field_insert_i64(i64 base, i64 insert, unsigned offset, unsigned bits) {
  return (i64)br_bits_field_insert_u64((u64)base, (u64)insert, offset, bits);
}
