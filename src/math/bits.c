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
static const uint8_t br__bits_width_table[256] = {
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
static int br__bits_popcount64_portable(uint64_t x) {
  /* Parallel bit count (SWAR). */
  x = x - ((x >> 1) & 0x5555555555555555ull);
  x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
  x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0full;
  return (int)((x * 0x0101010101010101ull) >> 56);
}
#endif

static int br__bits_bit_width64_portable(uint64_t x) {
  int n = 0;
  if (x >= (uint64_t)1 << 32) {
    x >>= 32;
    n = 32;
  }
  if (x >= (uint64_t)1 << 16) {
    x >>= 16;
    n += 16;
  }
  if (x >= (uint64_t)1 << 8) {
    x >>= 8;
    n += 8;
  }
  return n + (int)br__bits_width_table[x];
}

#if defined(BR__BITS_NEED_PORTABLE_BYTESWAP)
static uint64_t br__bits_byteswap64_portable(uint64_t x) {
  x = ((x & 0x00ff00ff00ff00ffull) << 8) | ((x >> 8) & 0x00ff00ff00ff00ffull);
  x = ((x & 0x0000ffff0000ffffull) << 16) | ((x >> 16) & 0x0000ffff0000ffffull);
  return (x << 32) | (x >> 32);
}
#endif

/* ---- Core primitives, dispatched by tier. Each takes a value already known to
   be handled for the x==0 case by the caller where noted. ---- */

static int br__bits_popcount64(uint64_t x) {
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
static int br__bits_clz64_nonzero(uint64_t x) {
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
static int br__bits_ctz64_nonzero(uint64_t x) {
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
     Negate in unsigned arithmetic: -(int64_t)x overflows for x with bit 63 set
     (UB), whereas the unsigned 0 - x is well-defined two's-complement. */
  return br__bits_popcount64_portable((x & (~x + 1u)) - 1u);
#endif
}

/* ---- count_ones ---- */

int br_bits_count_ones_u8(uint8_t x) {
  return br__bits_popcount64(x);
}
int br_bits_count_ones_u16(uint16_t x) {
  return br__bits_popcount64(x);
}
int br_bits_count_ones_u32(uint32_t x) {
  return br__bits_popcount64(x);
}
int br_bits_count_ones_u64(uint64_t x) {
  return br__bits_popcount64(x);
}

/* ---- count_zeros = width - count_ones ---- */

int br_bits_count_zeros_u8(uint8_t x) {
  return 8 - br__bits_popcount64(x);
}
int br_bits_count_zeros_u16(uint16_t x) {
  return 16 - br__bits_popcount64(x);
}
int br_bits_count_zeros_u32(uint32_t x) {
  return 32 - br__bits_popcount64(x);
}
int br_bits_count_zeros_u64(uint64_t x) {
  return 64 - br__bits_popcount64(x);
}

/* ---- clz (leading zeros); returns width at 0 ---- */

int br_bits_clz_u8(uint8_t x) {
  if (x == 0u) {
    return 8;
  }
  return br__bits_clz64_nonzero(x) - 56;
}
int br_bits_clz_u16(uint16_t x) {
  if (x == 0u) {
    return 16;
  }
  return br__bits_clz64_nonzero(x) - 48;
}
int br_bits_clz_u32(uint32_t x) {
  if (x == 0u) {
    return 32;
  }
  return br__bits_clz64_nonzero(x) - 32;
}
int br_bits_clz_u64(uint64_t x) {
  if (x == 0u) {
    return 64;
  }
  return br__bits_clz64_nonzero(x);
}

/* ---- ctz (trailing zeros); returns width at 0 ---- */

int br_bits_ctz_u8(uint8_t x) {
  if (x == 0u) {
    return 8;
  }
  return br__bits_ctz64_nonzero(x);
}
int br_bits_ctz_u16(uint16_t x) {
  if (x == 0u) {
    return 16;
  }
  return br__bits_ctz64_nonzero(x);
}
int br_bits_ctz_u32(uint32_t x) {
  if (x == 0u) {
    return 32;
  }
  return br__bits_ctz64_nonzero(x);
}
int br_bits_ctz_u64(uint64_t x) {
  if (x == 0u) {
    return 64;
  }
  return br__bits_ctz64_nonzero(x);
}

/* ---- bit_width; returns 0 at 0 (matches C23 stdc_bit_width) ---- */

int br_bits_bit_width_u8(uint8_t x) {
  return (int)br__bits_width_table[x];
}
int br_bits_bit_width_u16(uint16_t x) {
  return br__bits_bit_width64_portable(x);
}
int br_bits_bit_width_u32(uint32_t x) {
  return br__bits_bit_width64_portable(x);
}
int br_bits_bit_width_u64(uint64_t x) {
  return br__bits_bit_width64_portable(x);
}

/* ---- reverse_bits (portable; no widespread intrinsic) ---- */

uint8_t br_bits_reverse_u8(uint8_t x) {
  x = (uint8_t)(((x & 0xf0u) >> 4) | ((x & 0x0fu) << 4));
  x = (uint8_t)(((x & 0xccu) >> 2) | ((x & 0x33u) << 2));
  x = (uint8_t)(((x & 0xaau) >> 1) | ((x & 0x55u) << 1));
  return x;
}
uint16_t br_bits_reverse_u16(uint16_t x) {
  x = (uint16_t)((x >> 8) | (x << 8));
  x = (uint16_t)(((x & 0xf0f0u) >> 4) | ((x & 0x0f0fu) << 4));
  x = (uint16_t)(((x & 0xccccu) >> 2) | ((x & 0x3333u) << 2));
  x = (uint16_t)(((x & 0xaaaau) >> 1) | ((x & 0x5555u) << 1));
  return x;
}
uint32_t br_bits_reverse_u32(uint32_t x) {
  x = (x >> 16) | (x << 16);
  x = ((x & 0xff00ff00u) >> 8) | ((x & 0x00ff00ffu) << 8);
  x = ((x & 0xf0f0f0f0u) >> 4) | ((x & 0x0f0f0f0fu) << 4);
  x = ((x & 0xccccccccu) >> 2) | ((x & 0x33333333u) << 2);
  x = ((x & 0xaaaaaaaau) >> 1) | ((x & 0x55555555u) << 1);
  return x;
}
uint64_t br_bits_reverse_u64(uint64_t x) {
  x = (x >> 32) | (x << 32);
  x = ((x & 0xffff0000ffff0000ull) >> 16) | ((x & 0x0000ffff0000ffffull) << 16);
  x = ((x & 0xff00ff00ff00ff00ull) >> 8) | ((x & 0x00ff00ff00ff00ffull) << 8);
  x = ((x & 0xf0f0f0f0f0f0f0f0ull) >> 4) | ((x & 0x0f0f0f0f0f0f0f0full) << 4);
  x = ((x & 0xccccccccccccccccull) >> 2) | ((x & 0x3333333333333333ull) << 2);
  x = ((x & 0xaaaaaaaaaaaaaaaaull) >> 1) | ((x & 0x5555555555555555ull) << 1);
  return x;
}

/* ---- byteswap ---- */

uint16_t br_bits_byteswap_u16(uint16_t x) {
#if defined(BR__BITS_HAVE_GNU)
  return __builtin_bswap16(x);
#elif defined(BR__BITS_HAVE_MSVC)
  return _byteswap_ushort(x);
#else
  return (uint16_t)((x >> 8) | (x << 8));
#endif
}
uint32_t br_bits_byteswap_u32(uint32_t x) {
#if defined(BR__BITS_HAVE_GNU)
  return __builtin_bswap32(x);
#elif defined(BR__BITS_HAVE_MSVC)
  return _byteswap_ulong(x);
#else
  return (uint32_t)(br__bits_byteswap64_portable(x) >> 32);
#endif
}
uint64_t br_bits_byteswap_u64(uint64_t x) {
#if defined(BR__BITS_HAVE_GNU)
  return __builtin_bswap64(x);
#elif defined(BR__BITS_HAVE_MSVC)
  return _byteswap_uint64(x);
#else
  return br__bits_byteswap64_portable(x);
#endif
}

/* ---- rotations (k mod width; the mask keeps the shift well-defined) ---- */

uint8_t br_bits_rotate_left_u8(uint8_t x, unsigned k) {
  unsigned s = k & 7u;
  if (s == 0u) {
    return x;
  }
  return (uint8_t)((x << s) | (x >> (8u - s)));
}
uint16_t br_bits_rotate_left_u16(uint16_t x, unsigned k) {
  unsigned s = k & 15u;
  if (s == 0u) {
    return x;
  }
  return (uint16_t)((x << s) | (x >> (16u - s)));
}
uint32_t br_bits_rotate_left_u32(uint32_t x, unsigned k) {
  unsigned s = k & 31u;
  if (s == 0u) {
    return x;
  }
  return (x << s) | (x >> (32u - s));
}
uint64_t br_bits_rotate_left_u64(uint64_t x, unsigned k) {
  unsigned s = k & 63u;
  if (s == 0u) {
    return x;
  }
  return (x << s) | (x >> (64u - s));
}

uint8_t br_bits_rotate_right_u8(uint8_t x, unsigned k) {
  return br_bits_rotate_left_u8(x, 8u - (k & 7u));
}
uint16_t br_bits_rotate_right_u16(uint16_t x, unsigned k) {
  return br_bits_rotate_left_u16(x, 16u - (k & 15u));
}
uint32_t br_bits_rotate_right_u32(uint32_t x, unsigned k) {
  return br_bits_rotate_left_u32(x, 32u - (k & 31u));
}
uint64_t br_bits_rotate_right_u64(uint64_t x, unsigned k) {
  return br_bits_rotate_left_u64(x, 64u - (k & 63u));
}

/* ---- is_power_of_two ---- */

bool br_bits_is_power_of_two_u8(uint8_t x) {
  return x != 0u && (x & (uint8_t)(x - 1u)) == 0u;
}
bool br_bits_is_power_of_two_u16(uint16_t x) {
  return x != 0u && (x & (uint16_t)(x - 1u)) == 0u;
}
bool br_bits_is_power_of_two_u32(uint32_t x) {
  return x != 0u && (x & (x - 1u)) == 0u;
}
bool br_bits_is_power_of_two_u64(uint64_t x) {
  return x != 0u && (x & (x - 1u)) == 0u;
}
bool br_bits_is_power_of_two_i8(int8_t x) {
  return x > 0 && (x & (int8_t)(x - 1)) == 0;
}
bool br_bits_is_power_of_two_i16(int16_t x) {
  return x > 0 && (x & (int16_t)(x - 1)) == 0;
}
bool br_bits_is_power_of_two_i32(int32_t x) {
  return x > 0 && (x & (x - 1)) == 0;
}
bool br_bits_is_power_of_two_i64(int64_t x) {
  return x > 0 && (x & (x - 1)) == 0;
}

/* ---- add / sub with carry / borrow ---- */

br_bits_add_u32_result br_bits_add_u32(uint32_t x, uint32_t y, bool carry_in) {
  br_bits_add_u32_result result;
  uint64_t sum = (uint64_t)x + (uint64_t)y + (uint64_t)(carry_in ? 1u : 0u);

  result.sum = (uint32_t)sum;
  result.carry_out = (sum >> 32) != 0u;
  return result;
}

br_bits_add_u64_result br_bits_add_u64(uint64_t x, uint64_t y, bool carry_in) {
  br_bits_add_u64_result result;
  uint64_t sum = x + y + (carry_in ? 1u : 0u);

  result.sum = sum;
  /* Carry out per Go's bits.Add64: a bit is carried where both inputs set it,
     or where either input set it and the sum did not. */
  result.carry_out = (((x & y) | ((x | y) & ~sum)) >> 63) != 0u;
  return result;
}

br_bits_sub_u32_result br_bits_sub_u32(uint32_t x, uint32_t y, bool borrow_in) {
  br_bits_sub_u32_result result;
  uint64_t diff = (uint64_t)x - (uint64_t)y - (uint64_t)(borrow_in ? 1u : 0u);

  result.diff = (uint32_t)diff;
  result.borrow_out = (diff >> 32) != 0u;
  return result;
}

br_bits_sub_u64_result br_bits_sub_u64(uint64_t x, uint64_t y, bool borrow_in) {
  br_bits_sub_u64_result result;
  uint64_t b = borrow_in ? 1u : 0u;
  uint64_t diff = x - y - b;

  result.diff = diff;
  /* Borrow out per Go's bits.Sub64. */
  result.borrow_out = (((~x & y) | (~(x ^ y) & diff)) >> 63) != 0u;
  return result;
}

/* ---- full-width multiply (hi, lo) ---- */

br_bits_mul_u32_result br_bits_mul_u32(uint32_t x, uint32_t y) {
  br_bits_mul_u32_result result;
  uint64_t product = (uint64_t)x * (uint64_t)y;

  result.hi = (uint32_t)(product >> 32);
  result.lo = (uint32_t)product;
  return result;
}

br_bits_mul_u64_result br_bits_mul_u64(uint64_t x, uint64_t y) {
  br_bits_mul_u64_result result;
#if defined(__SIZEOF_INT128__)
  unsigned __int128 product = (unsigned __int128)x * (unsigned __int128)y;
  result.hi = (uint64_t)(product >> 64);
  result.lo = (uint64_t)product;
#else
  /* Portable dual-word multiply (Go's bits.Mul64). */
  const uint64_t mask32 = 0xffffffffull;
  uint64_t x0 = x & mask32;
  uint64_t x1 = x >> 32;
  uint64_t y0 = y & mask32;
  uint64_t y1 = y >> 32;

  uint64_t w0 = x0 * y0;
  uint64_t t = x1 * y0 + (w0 >> 32);
  uint64_t w1 = t & mask32;
  uint64_t w2 = t >> 32;

  w1 += x0 * y1;

  result.hi = x1 * y1 + w2 + (w1 >> 32);
  result.lo = x * y;
#endif
  return result;
}

/* ---- divide (double-width / single) returning status, never panicking ---- */

br_bits_div_u32_result br_bits_div_u32(uint32_t hi, uint32_t lo, uint32_t y) {
  br_bits_div_u32_result result;
  uint64_t z;

  if (y == 0u || y <= hi) {
    result.quo = 0u;
    result.rem = 0u;
    result.status = BR_STATUS_INVALID_ARGUMENT;
    return result;
  }

  z = ((uint64_t)hi << 32) | (uint64_t)lo;
  result.quo = (uint32_t)(z / (uint64_t)y);
  result.rem = (uint32_t)(z % (uint64_t)y);
  result.status = BR_STATUS_OK;
  return result;
}

br_bits_div_u64_result br_bits_div_u64(uint64_t hi, uint64_t lo, uint64_t y) {
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
    result.quo = (uint64_t)(z / y);
    result.rem = (uint64_t)(z % y);
  }
#else
  {
    /* Knuth Algorithm D for a 128-by-64 divide (Go's bits.Div64). */
    const uint64_t two32 = (uint64_t)1 << 32;
    const uint64_t mask32 = two32 - 1u;
    unsigned s = (unsigned)br_bits_clz_u64(y);
    uint64_t yn1, yn0, un32, un10, un1, un0, q1, q0, rhat, un21;

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

uint8_t br_bits_field_extract_u8(uint8_t value, unsigned offset, unsigned bits) {
  return (uint8_t)((value >> offset) & BR__BITS_UMASK(uint8_t, bits, 8u));
}
uint16_t br_bits_field_extract_u16(uint16_t value, unsigned offset, unsigned bits) {
  return (uint16_t)((value >> offset) & BR__BITS_UMASK(uint16_t, bits, 16u));
}
uint32_t br_bits_field_extract_u32(uint32_t value, unsigned offset, unsigned bits) {
  return (value >> offset) & BR__BITS_UMASK(uint32_t, bits, 32u);
}
uint64_t br_bits_field_extract_u64(uint64_t value, unsigned offset, unsigned bits) {
  return (value >> offset) & BR__BITS_UMASK(uint64_t, bits, 64u);
}

int8_t br_bits_field_extract_i8(int8_t value, unsigned offset, unsigned bits) {
  uint8_t field = br_bits_field_extract_u8((uint8_t)value, offset, bits);
  uint8_t sign = (uint8_t)(bits == 0u ? 0u : (uint8_t)1 << (bits - 1u));
  return (int8_t)((uint8_t)(field ^ sign) - sign);
}
int16_t br_bits_field_extract_i16(int16_t value, unsigned offset, unsigned bits) {
  uint16_t field = br_bits_field_extract_u16((uint16_t)value, offset, bits);
  uint16_t sign = (uint16_t)(bits == 0u ? 0u : (uint16_t)1 << (bits - 1u));
  return (int16_t)((uint16_t)(field ^ sign) - sign);
}
int32_t br_bits_field_extract_i32(int32_t value, unsigned offset, unsigned bits) {
  uint32_t field = br_bits_field_extract_u32((uint32_t)value, offset, bits);
  uint32_t sign = bits == 0u ? 0u : (uint32_t)1 << (bits - 1u);
  return (int32_t)((field ^ sign) - sign);
}
int64_t br_bits_field_extract_i64(int64_t value, unsigned offset, unsigned bits) {
  uint64_t field = br_bits_field_extract_u64((uint64_t)value, offset, bits);
  uint64_t sign = bits == 0u ? 0u : (uint64_t)1 << (bits - 1u);
  return (int64_t)((field ^ sign) - sign);
}

uint8_t br_bits_field_insert_u8(uint8_t base, uint8_t insert, unsigned offset, unsigned bits) {
  uint8_t mask = (uint8_t)(BR__BITS_UMASK(uint8_t, bits, 8u) << offset);
  return (uint8_t)((base & (uint8_t)~mask) | (((uint8_t)(insert << offset)) & mask));
}
uint16_t br_bits_field_insert_u16(uint16_t base, uint16_t insert, unsigned offset, unsigned bits) {
  uint16_t mask = (uint16_t)(BR__BITS_UMASK(uint16_t, bits, 16u) << offset);
  return (uint16_t)((base & (uint16_t)~mask) | (((uint16_t)(insert << offset)) & mask));
}
uint32_t br_bits_field_insert_u32(uint32_t base, uint32_t insert, unsigned offset, unsigned bits) {
  uint32_t mask = BR__BITS_UMASK(uint32_t, bits, 32u) << offset;
  return (base & ~mask) | ((insert << offset) & mask);
}
uint64_t br_bits_field_insert_u64(uint64_t base, uint64_t insert, unsigned offset, unsigned bits) {
  uint64_t mask = BR__BITS_UMASK(uint64_t, bits, 64u) << offset;
  return (base & ~mask) | ((insert << offset) & mask);
}

int8_t br_bits_field_insert_i8(int8_t base, int8_t insert, unsigned offset, unsigned bits) {
  return (int8_t)br_bits_field_insert_u8((uint8_t)base, (uint8_t)insert, offset, bits);
}
int16_t br_bits_field_insert_i16(int16_t base, int16_t insert, unsigned offset, unsigned bits) {
  return (int16_t)br_bits_field_insert_u16((uint16_t)base, (uint16_t)insert, offset, bits);
}
int32_t br_bits_field_insert_i32(int32_t base, int32_t insert, unsigned offset, unsigned bits) {
  return (int32_t)br_bits_field_insert_u32((uint32_t)base, (uint32_t)insert, offset, bits);
}
int64_t br_bits_field_insert_i64(int64_t base, int64_t insert, unsigned offset, unsigned bits) {
  return (int64_t)br_bits_field_insert_u64((uint64_t)base, (uint64_t)insert, offset, bits);
}
