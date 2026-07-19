#include <assert.h>
#include <stdint.h>

#include <bedrock/math/bits.h>

/*
Differential test harness. The library object linked into this binary provides
the native tier (whatever the host offers: C23 stdbit, GNU/MSVC builtins, or the
portable fallback). To compare the portable fallback against that native tier in
ONE binary, this TU compiles src/math/bits.c a SECOND time with
BR_BITS_FORCE_FALLBACK, under a private `brfb_` prefix so its symbols do not
clash with the linked-in native ones.

This double-compile lives entirely in the test; the production header and source
carry nothing to support it. The prefix macros are #undef-safe because bits.c
only defines the br_bits_* / br__bits_* names we remap here plus its own file
statics (which BR_BITS_FORCE_FALLBACK keeps self-contained).
*/

/* Remap every public entry point (and the tables/helpers are file-static, so
   they don't need remapping — only the external br_bits_* symbols would clash).
   We rename by textual substitution of the `br_bits_` prefix. */
#define br_bits_count_ones_u8 brfb_count_ones_u8
#define br_bits_count_ones_u16 brfb_count_ones_u16
#define br_bits_count_ones_u32 brfb_count_ones_u32
#define br_bits_count_ones_u64 brfb_count_ones_u64
#define br_bits_count_zeros_u8 brfb_count_zeros_u8
#define br_bits_count_zeros_u16 brfb_count_zeros_u16
#define br_bits_count_zeros_u32 brfb_count_zeros_u32
#define br_bits_count_zeros_u64 brfb_count_zeros_u64
#define br_bits_clz_u8 brfb_clz_u8
#define br_bits_clz_u16 brfb_clz_u16
#define br_bits_clz_u32 brfb_clz_u32
#define br_bits_clz_u64 brfb_clz_u64
#define br_bits_ctz_u8 brfb_ctz_u8
#define br_bits_ctz_u16 brfb_ctz_u16
#define br_bits_ctz_u32 brfb_ctz_u32
#define br_bits_ctz_u64 brfb_ctz_u64
#define br_bits_bit_width_u8 brfb_bit_width_u8
#define br_bits_bit_width_u16 brfb_bit_width_u16
#define br_bits_bit_width_u32 brfb_bit_width_u32
#define br_bits_bit_width_u64 brfb_bit_width_u64
#define br_bits_reverse_u8 brfb_reverse_u8
#define br_bits_reverse_u16 brfb_reverse_u16
#define br_bits_reverse_u32 brfb_reverse_u32
#define br_bits_reverse_u64 brfb_reverse_u64
#define br_bits_byteswap_u16 brfb_byteswap_u16
#define br_bits_byteswap_u32 brfb_byteswap_u32
#define br_bits_byteswap_u64 brfb_byteswap_u64
#define br_bits_rotate_left_u8 brfb_rotate_left_u8
#define br_bits_rotate_left_u16 brfb_rotate_left_u16
#define br_bits_rotate_left_u32 brfb_rotate_left_u32
#define br_bits_rotate_left_u64 brfb_rotate_left_u64
#define br_bits_rotate_right_u8 brfb_rotate_right_u8
#define br_bits_rotate_right_u16 brfb_rotate_right_u16
#define br_bits_rotate_right_u32 brfb_rotate_right_u32
#define br_bits_rotate_right_u64 brfb_rotate_right_u64
#define br_bits_is_power_of_two_u8 brfb_is_power_of_two_u8
#define br_bits_is_power_of_two_u16 brfb_is_power_of_two_u16
#define br_bits_is_power_of_two_u32 brfb_is_power_of_two_u32
#define br_bits_is_power_of_two_u64 brfb_is_power_of_two_u64
#define br_bits_is_power_of_two_i8 brfb_is_power_of_two_i8
#define br_bits_is_power_of_two_i16 brfb_is_power_of_two_i16
#define br_bits_is_power_of_two_i32 brfb_is_power_of_two_i32
#define br_bits_is_power_of_two_i64 brfb_is_power_of_two_i64
#define br_bits_add_u32 brfb_add_u32
#define br_bits_add_u64 brfb_add_u64
#define br_bits_sub_u32 brfb_sub_u32
#define br_bits_sub_u64 brfb_sub_u64
#define br_bits_mul_u32 brfb_mul_u32
#define br_bits_mul_u64 brfb_mul_u64
#define br_bits_div_u32 brfb_div_u32
#define br_bits_div_u64 brfb_div_u64
#define br_bits_field_extract_u8 brfb_field_extract_u8
#define br_bits_field_extract_u16 brfb_field_extract_u16
#define br_bits_field_extract_u32 brfb_field_extract_u32
#define br_bits_field_extract_u64 brfb_field_extract_u64
#define br_bits_field_extract_i8 brfb_field_extract_i8
#define br_bits_field_extract_i16 brfb_field_extract_i16
#define br_bits_field_extract_i32 brfb_field_extract_i32
#define br_bits_field_extract_i64 brfb_field_extract_i64
#define br_bits_field_insert_u8 brfb_field_insert_u8
#define br_bits_field_insert_u16 brfb_field_insert_u16
#define br_bits_field_insert_u32 brfb_field_insert_u32
#define br_bits_field_insert_u64 brfb_field_insert_u64
#define br_bits_field_insert_i8 brfb_field_insert_i8
#define br_bits_field_insert_i16 brfb_field_insert_i16
#define br_bits_field_insert_i32 brfb_field_insert_i32
#define br_bits_field_insert_i64 brfb_field_insert_i64

/* Rename the result-struct constructors' file statics are not external; only
   the typedef names are shared (identical layout, defined once by the header
   which we include normally above). Force the fallback tier and pull the impl
   in with the renamed public symbols. */
#define BR_BITS_FORCE_FALLBACK 1
#include "../src/math/bits.c"

/* Undo the remap so the rest of the test refers to the real (native) symbols. */
#undef br_bits_count_ones_u8
#undef br_bits_count_ones_u16
#undef br_bits_count_ones_u32
#undef br_bits_count_ones_u64
#undef br_bits_count_zeros_u8
#undef br_bits_count_zeros_u16
#undef br_bits_count_zeros_u32
#undef br_bits_count_zeros_u64
#undef br_bits_clz_u8
#undef br_bits_clz_u16
#undef br_bits_clz_u32
#undef br_bits_clz_u64
#undef br_bits_ctz_u8
#undef br_bits_ctz_u16
#undef br_bits_ctz_u32
#undef br_bits_ctz_u64
#undef br_bits_bit_width_u8
#undef br_bits_bit_width_u16
#undef br_bits_bit_width_u32
#undef br_bits_bit_width_u64
#undef br_bits_reverse_u8
#undef br_bits_reverse_u16
#undef br_bits_reverse_u32
#undef br_bits_reverse_u64
#undef br_bits_byteswap_u16
#undef br_bits_byteswap_u32
#undef br_bits_byteswap_u64
#undef br_bits_rotate_left_u8
#undef br_bits_rotate_left_u16
#undef br_bits_rotate_left_u32
#undef br_bits_rotate_left_u64
#undef br_bits_rotate_right_u8
#undef br_bits_rotate_right_u16
#undef br_bits_rotate_right_u32
#undef br_bits_rotate_right_u64
#undef br_bits_is_power_of_two_u8
#undef br_bits_is_power_of_two_u16
#undef br_bits_is_power_of_two_u32
#undef br_bits_is_power_of_two_u64
#undef br_bits_is_power_of_two_i8
#undef br_bits_is_power_of_two_i16
#undef br_bits_is_power_of_two_i32
#undef br_bits_is_power_of_two_i64
#undef br_bits_add_u32
#undef br_bits_add_u64
#undef br_bits_sub_u32
#undef br_bits_sub_u64
#undef br_bits_mul_u32
#undef br_bits_mul_u64
#undef br_bits_div_u32
#undef br_bits_div_u64
#undef br_bits_field_extract_u8
#undef br_bits_field_extract_u16
#undef br_bits_field_extract_u32
#undef br_bits_field_extract_u64
#undef br_bits_field_extract_i8
#undef br_bits_field_extract_i16
#undef br_bits_field_extract_i32
#undef br_bits_field_extract_i64
#undef br_bits_field_insert_u8
#undef br_bits_field_insert_u16
#undef br_bits_field_insert_u32
#undef br_bits_field_insert_u64
#undef br_bits_field_insert_i8
#undef br_bits_field_insert_i16
#undef br_bits_field_insert_i32
#undef br_bits_field_insert_i64

/* Small deterministic PRNG (xorshift64*) for randomized u32/u64 sweeps. */
static u64 rng_state = 0x9e3779b97f4a7c15ull;
static u64 next_rand(void) {
  u64 x = rng_state;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  rng_state = x;
  return x * 0x2545f4914f6cdd1dull;
}

/* Differential: native tier vs forced fallback, exhaustive over u8 and u16. */
static void test_diff_u8_u16(void) {
  for (int i = 0; i < 256; ++i) {
    u8 x = (u8)i;
    assert(br_bits_count_ones_u8(x) == brfb_count_ones_u8(x));
    assert(br_bits_count_zeros_u8(x) == brfb_count_zeros_u8(x));
    assert(br_bits_clz_u8(x) == brfb_clz_u8(x));
    assert(br_bits_ctz_u8(x) == brfb_ctz_u8(x));
    assert(br_bits_bit_width_u8(x) == brfb_bit_width_u8(x));
    assert(br_bits_reverse_u8(x) == brfb_reverse_u8(x));
  }
  for (int i = 0; i < 65536; ++i) {
    u16 x = (u16)i;
    assert(br_bits_count_ones_u16(x) == brfb_count_ones_u16(x));
    assert(br_bits_count_zeros_u16(x) == brfb_count_zeros_u16(x));
    assert(br_bits_clz_u16(x) == brfb_clz_u16(x));
    assert(br_bits_ctz_u16(x) == brfb_ctz_u16(x));
    assert(br_bits_bit_width_u16(x) == brfb_bit_width_u16(x));
    assert(br_bits_reverse_u16(x) == brfb_reverse_u16(x));
    assert(br_bits_byteswap_u16(x) == brfb_byteswap_u16(x));
  }
}

/* Differential: native vs fallback over random u32/u64, plus 0 and all-ones. */
static void test_diff_u32_u64(void) {
  u32 seeds32[] = {0u, 1u, 0xffffffffu, 0x80000000u, 0x00000001u, 0xdeadbeefu};
  u64 seeds64[] = {0u, 1u, ~(u64)0, (u64)1 << 63, 0x0123456789abcdefull};
  usize i;

  for (i = 0; i < sizeof(seeds32) / sizeof(seeds32[0]); ++i) {
    u32 x = seeds32[i];
    assert(br_bits_count_ones_u32(x) == brfb_count_ones_u32(x));
    assert(br_bits_clz_u32(x) == brfb_clz_u32(x));
    assert(br_bits_ctz_u32(x) == brfb_ctz_u32(x));
    assert(br_bits_bit_width_u32(x) == brfb_bit_width_u32(x));
    assert(br_bits_reverse_u32(x) == brfb_reverse_u32(x));
    assert(br_bits_byteswap_u32(x) == brfb_byteswap_u32(x));
  }
  for (i = 0; i < sizeof(seeds64) / sizeof(seeds64[0]); ++i) {
    u64 x = seeds64[i];
    assert(br_bits_count_ones_u64(x) == brfb_count_ones_u64(x));
    assert(br_bits_clz_u64(x) == brfb_clz_u64(x));
    assert(br_bits_ctz_u64(x) == brfb_ctz_u64(x));
    assert(br_bits_bit_width_u64(x) == brfb_bit_width_u64(x));
    assert(br_bits_reverse_u64(x) == brfb_reverse_u64(x));
    assert(br_bits_byteswap_u64(x) == brfb_byteswap_u64(x));
  }
  for (i = 0; i < 200000; ++i) {
    u64 r = next_rand();
    u32 x32 = (u32)r;
    u64 x64 = r;
    assert(br_bits_count_ones_u32(x32) == brfb_count_ones_u32(x32));
    assert(br_bits_clz_u32(x32) == brfb_clz_u32(x32));
    assert(br_bits_ctz_u32(x32) == brfb_ctz_u32(x32));
    assert(br_bits_bit_width_u32(x32) == brfb_bit_width_u32(x32));
    assert(br_bits_reverse_u32(x32) == brfb_reverse_u32(x32));
    assert(br_bits_byteswap_u32(x32) == brfb_byteswap_u32(x32));
    assert(br_bits_count_ones_u64(x64) == brfb_count_ones_u64(x64));
    assert(br_bits_clz_u64(x64) == brfb_clz_u64(x64));
    assert(br_bits_ctz_u64(x64) == brfb_ctz_u64(x64));
    assert(br_bits_bit_width_u64(x64) == brfb_bit_width_u64(x64));
    assert(br_bits_reverse_u64(x64) == brfb_reverse_u64(x64));
    assert(br_bits_byteswap_u64(x64) == brfb_byteswap_u64(x64));
  }
}

/* Known-answer checks for the zero-input contract and basic values. */
static void test_known_values(void) {
  assert(br_bits_clz_u32(0u) == 32);
  assert(br_bits_ctz_u32(0u) == 32);
  assert(br_bits_clz_u8(0u) == 8);
  assert(br_bits_ctz_u8(0u) == 8);
  assert(br_bits_clz_u64(0u) == 64);
  assert(br_bits_ctz_u64(0u) == 64);
  assert(br_bits_bit_width_u32(0u) == 0);
  assert(br_bits_count_ones_u32(0u) == 0);
  assert(br_bits_count_zeros_u32(0u) == 32);

  assert(br_bits_clz_u32(1u) == 31);
  assert(br_bits_ctz_u32(1u) == 0);
  assert(br_bits_bit_width_u32(1u) == 1);
  assert(br_bits_bit_width_u32(0xffffffffu) == 32);
  assert(br_bits_clz_u32(0x80000000u) == 0);
  assert(br_bits_ctz_u32(0x80000000u) == 31);
  assert(br_bits_count_ones_u32(0xffffffffu) == 32);

  assert(br_bits_byteswap_u32(0x11223344u) == 0x44332211u);
  assert(br_bits_reverse_u8(0x01u) == 0x80u);
}

/* Property tests over random inputs. */
static void test_properties(void) {
  usize i;
  for (i = 0; i < 200000; ++i) {
    u64 r = next_rand();
    u32 x = (u32)r;
    u64 y = r;
    unsigned k = (unsigned)(r >> 40);

    /* popcount + count_zeros == width */
    assert(br_bits_count_ones_u32(x) + br_bits_count_zeros_u32(x) == 32);
    assert(br_bits_count_ones_u64(y) + br_bits_count_zeros_u64(y) == 64);

    /* rotate_left(rotate_right(x,k),k) == x, and vice versa */
    assert(br_bits_rotate_left_u32(br_bits_rotate_right_u32(x, k), k) == x);
    assert(br_bits_rotate_right_u64(br_bits_rotate_left_u64(y, k), k) == y);

    /* reverse(reverse(x)) == x */
    assert(br_bits_reverse_u32(br_bits_reverse_u32(x)) == x);
    assert(br_bits_reverse_u64(br_bits_reverse_u64(y)) == y);

    /* bit_width(x) - 1 == index of highest set bit, for x > 0 */
    if (x != 0u) {
      assert(br_bits_bit_width_u32(x) - 1 == 31 - br_bits_clz_u32(x));
    }
  }
}

/* Rotation edge: k mod width, and k == 0 / k == width are identity. */
static void test_rotate_edges(void) {
  assert(br_bits_rotate_left_u32(0x12345678u, 0u) == 0x12345678u);
  assert(br_bits_rotate_left_u32(0x12345678u, 32u) == 0x12345678u);
  assert(br_bits_rotate_left_u32(0x12345678u, 36u) == br_bits_rotate_left_u32(0x12345678u, 4u));
  assert(br_bits_rotate_left_u8(0x01u, 1u) == 0x02u);
  assert(br_bits_rotate_right_u8(0x01u, 1u) == 0x80u);
}

/* add / sub carry / borrow chains, checked against a wider type. */
static void test_add_sub(void) {
  usize i;
  br_bits_add_u32_result a;
  br_bits_sub_u32_result s;

  a = br_bits_add_u32(0xffffffffu, 1u, false);
  assert(a.sum == 0u && a.carry_out);
  a = br_bits_add_u32(0xffffffffu, 0u, true);
  assert(a.sum == 0u && a.carry_out);
  a = br_bits_add_u32(1u, 2u, false);
  assert(a.sum == 3u && !a.carry_out);

  s = br_bits_sub_u32(0u, 1u, false);
  assert(s.diff == 0xffffffffu && s.borrow_out);
  s = br_bits_sub_u32(5u, 3u, false);
  assert(s.diff == 2u && !s.borrow_out);

  for (i = 0; i < 100000; ++i) {
    u64 r = next_rand();
    u32 x = (u32)r;
    u32 y = (u32)(r >> 32);
    bool cin = (r & 1u) != 0u;
    u64 wide_add = (u64)x + (u64)y + (u64)(cin ? 1u : 0u);
    br_bits_add_u32_result ar = br_bits_add_u32(x, y, cin);
    assert(ar.sum == (u32)wide_add);
    assert(ar.carry_out == ((wide_add >> 32) != 0u));

    {
      br_bits_add_u64_result ar64 = br_bits_add_u64(x, y, cin);
      assert(ar64.sum == (u64)x + (u64)y + (cin ? 1u : 0u));
      assert(!ar64.carry_out); /* 32-bit inputs never overflow u64 */
    }
  }
}

/* mul (hi,lo): check u32 against u64, u64 against __int128 where available. */
static void test_mul(void) {
  usize i;
  br_bits_mul_u32_result m32;

  m32 = br_bits_mul_u32(0xffffffffu, 0xffffffffu);
  assert(((u64)m32.hi << 32 | m32.lo) == 0xfffffffe00000001ull);

  for (i = 0; i < 200000; ++i) {
    u64 r1 = next_rand();
    u64 r2 = next_rand();
    u32 x = (u32)r1;
    u32 y = (u32)r2;
    br_bits_mul_u32_result m = br_bits_mul_u32(x, y);
    u64 expect = (u64)x * (u64)y;
    assert(((u64)m.hi << 32 | m.lo) == expect);

    {
      br_bits_mul_u64_result m64 = br_bits_mul_u64(r1, r2);
#if defined(__SIZEOF_INT128__)
      unsigned __int128 e = (unsigned __int128)r1 * (unsigned __int128)r2;
      assert(m64.hi == (u64)(e >> 64));
      assert(m64.lo == (u64)e);
#else
      /* Without 128-bit: check the low half at least. */
      assert(m64.lo == r1 * r2);
#endif
    }
  }
}

/* div: status contract and reconstruction quo*y + rem == (hi:lo). */
static void test_div(void) {
  usize i;
  br_bits_div_u32_result d32;
  br_bits_div_u64_result d64;

  /* divide error and overflow -> INVALID_ARGUMENT, zeroed. */
  d32 = br_bits_div_u32(0u, 10u, 0u);
  assert(d32.status == BR_STATUS_INVALID_ARGUMENT && d32.quo == 0u && d32.rem == 0u);
  d32 = br_bits_div_u32(5u, 0u, 5u); /* y <= hi */
  assert(d32.status == BR_STATUS_INVALID_ARGUMENT);
  d64 = br_bits_div_u64(0u, 100u, 0u);
  assert(d64.status == BR_STATUS_INVALID_ARGUMENT);
  d64 = br_bits_div_u64(5u, 0u, 5u);
  assert(d64.status == BR_STATUS_INVALID_ARGUMENT);

  /* Simple exact division. */
  d32 = br_bits_div_u32(0u, 100u, 7u);
  assert(d32.status == BR_STATUS_OK && d32.quo == 14u && d32.rem == 2u);

  for (i = 0; i < 200000; ++i) {
    u64 r = next_rand();
    u32 hi = (u32)(r >> 40); /* keep hi small so hi < y is likely */
    u32 lo = (u32)r;
    u32 y = (u32)(next_rand() | 1u); /* nonzero */
    if (y <= hi) {
      continue;
    }
    d32 = br_bits_div_u32(hi, lo, y);
    assert(d32.status == BR_STATUS_OK);
    {
      u64 z = ((u64)hi << 32) | lo;
      assert((u64)d32.quo * (u64)y + (u64)d32.rem == z);
      assert(d32.rem < y);
    }
  }

#if defined(__SIZEOF_INT128__)
  for (i = 0; i < 200000; ++i) {
    u64 hi = next_rand() >> 40;
    u64 lo = next_rand();
    u64 y = next_rand() | 1u;
    if (y <= hi) {
      continue;
    }
    d64 = br_bits_div_u64(hi, lo, y);
    assert(d64.status == BR_STATUS_OK);
    {
      unsigned __int128 z = ((unsigned __int128)hi << 64) | lo;
      assert((unsigned __int128)d64.quo * y + d64.rem == z);
      assert(d64.rem < y);
    }
  }
#endif
}

static void test_is_power_of_two(void) {
  assert(br_bits_is_power_of_two_u32(1u));
  assert(br_bits_is_power_of_two_u32(2u));
  assert(br_bits_is_power_of_two_u32(0x80000000u));
  assert(!br_bits_is_power_of_two_u32(0u));
  assert(!br_bits_is_power_of_two_u32(3u));
  assert(!br_bits_is_power_of_two_i32(0));
  assert(!br_bits_is_power_of_two_i32(-8)); /* negatives are not powers of two */
  assert(br_bits_is_power_of_two_i32(16));
  assert(br_bits_is_power_of_two_i8((i8)64));
  assert(!br_bits_is_power_of_two_i8((i8)-128));
}

static void test_bitfields(void) {
  /* extract: pull bits [offset, offset+bits). */
  assert(br_bits_field_extract_u32(0xabcd1234u, 0u, 8u) == 0x34u);
  assert(br_bits_field_extract_u32(0xabcd1234u, 8u, 8u) == 0x12u);
  assert(br_bits_field_extract_u32(0xabcd1234u, 0u, 32u) == 0xabcd1234u);
  assert(br_bits_field_extract_u32(0xabcd1234u, 4u, 0u) == 0u);

  /* signed extract sign-extends. Field 0xF in 4 bits == -1. */
  assert(br_bits_field_extract_i32((i32)0x000000f0, 4u, 4u) == -1);
  assert(br_bits_field_extract_i32((i32)0x00000070, 4u, 4u) == 7);
  assert(br_bits_field_extract_i8((i8)0xf0, 4u, 4u) == -1);

  /* insert: replace bits [offset, offset+bits) with insert's low bits. */
  assert(br_bits_field_insert_u32(0x00000000u, 0xffu, 8u, 8u) == 0x0000ff00u);
  assert(br_bits_field_insert_u32(0xffffffffu, 0x00u, 8u, 8u) == 0xffff00ffu);
  assert(br_bits_field_insert_u32(0x12345678u, 0xabu, 0u, 8u) == 0x123456abu);
  assert(br_bits_field_insert_u32(0x12345678u, 0x99u, 0u, 32u) == 0x00000099u);

  /* round-trip: insert then extract returns the low `bits` of the inserted. */
  {
    u32 base = 0xdeadbeefu;
    u32 v = br_bits_field_insert_u32(base, 0x2au, 5u, 6u);
    assert(br_bits_field_extract_u32(v, 5u, 6u) == (0x2au & 0x3fu));
  }
}

int main(void) {
  test_diff_u8_u16();
  test_diff_u32_u64();
  test_known_values();
  test_properties();
  test_rotate_edges();
  test_add_sub();
  test_mul();
  test_div();
  test_is_power_of_two();
  test_bitfields();
  return 0;
}
