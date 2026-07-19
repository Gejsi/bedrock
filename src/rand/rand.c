#include <bedrock/rand/rand.h>

#include <bedrock/math/bits.h>

/*
PCG64 with the DXSM output permutation, ported byte-for-byte from Go v2's
math/rand/v2 pcg.go so identical seeds produce identical streams. That stream
equality is the module's correctness anchor (see tests/test_rand.c).
*/

/* 128-bit multiplier and fixed increment, as two u64 halves each. */
#define BR__RAND_MUL_HI 2549297995355413924ull
#define BR__RAND_MUL_LO 4865540595714422341ull
#define BR__RAND_INC_HI 6364136223846793005ull
#define BR__RAND_INC_LO 1442695040888963407ull

void br_rand_seed(br_rand *r, u64 seed1, u64 seed2) {
  r->hi = seed1;
  r->lo = seed2;
}

/*
Advance the state (state = state * MUL + INC, mod 2^128) and return the pre-DXSM
(hi, lo) of the new state, exactly as pcg.go's next().
*/
static void br__rand_step(br_rand *r, u64 *out_hi, u64 *out_lo) {
  br_bits_mul_u64_result product;
  br_bits_add_u64_result add_lo;
  br_bits_add_u64_result add_hi;
  u64 hi;
  u64 lo;

  /* Full 128-bit multiply of the low word by the low multiplier. */
  product = br_bits_mul_u64(r->lo, BR__RAND_MUL_LO);
  hi = product.hi;
  lo = product.lo;

  /* Cross terms fold into the high word with ORDINARY wrapping adds (mod 2^64,
     no carry tracked) — this matches Go and must not be "fixed" into carried
     adds, which would change the stream. */
  hi += r->hi * BR__RAND_MUL_LO + r->lo * BR__RAND_MUL_HI;

  /* Only the increment addition is carried across the 128-bit boundary. */
  add_lo = br_bits_add_u64(lo, BR__RAND_INC_LO, false);
  lo = add_lo.sum;
  add_hi = br_bits_add_u64(hi, BR__RAND_INC_HI, add_lo.carry_out);
  hi = add_hi.sum;

  r->hi = hi;
  r->lo = lo;
  *out_hi = hi;
  *out_lo = lo;
}

u64 br_rand_u64(br_rand *r) {
  u64 hi;
  u64 lo;
  const u64 cheap_mul = 0xda942042e4dd58b5ull;

  br__rand_step(r, &hi, &lo);

  /* DXSM "double xorshift multiply" on the high half. The (lo | 1) multiply is
     load-bearing (keeps the multiplier odd/invertible) — do not simplify. */
  hi ^= hi >> 32;
  hi *= cheap_mul;
  hi ^= hi >> 48;
  hi *= (lo | 1u);
  return hi;
}

u32 br_rand_u32(br_rand *r) {
  return (u32)(br_rand_u64(r) >> 32);
}

u64 br_rand_u64_below(br_rand *r, u64 bound) {
  br_bits_mul_u64_result wide;
  u64 threshold;

  if (bound == 0u) {
    return 0u;
  }

  /* Power-of-two bound: the low bits are already uniform, so mask. */
  if ((bound & (bound - 1u)) == 0u) {
    return br_rand_u64(r) & (bound - 1u);
  }

  /* Lemire multiply-shift-reject, matching Go v2's uint64n. The high half of
     draw*bound is the result; reject only in the rare low-half zone that would
     bias the distribution, and compute the expensive modulo only there. */
  wide = br_bits_mul_u64(br_rand_u64(r), bound);
  if (wide.lo < bound) {
    threshold = (0u - bound) % bound; /* -bound mod bound, in unsigned */
    while (wide.lo < threshold) {
      wide = br_bits_mul_u64(br_rand_u64(r), bound);
    }
  }
  return wide.hi;
}

int64_t br_rand_i64_between(br_rand *r, int64_t lo, int64_t hi) {
  u64 span;
  u64 draw;

  if (lo >= hi) {
    return lo;
  }

  /* Compute the span and offset in unsigned arithmetic: lo may be INT64_MIN and
     the span may exceed INT64_MAX, so a signed `lo + draw` would overflow (UB).
     Unsigned wrap is defined and the two's-complement cast back is exact. */
  span = (u64)hi - (u64)lo;
  draw = br_rand_u64_below(r, span);
  return (int64_t)((u64)lo + draw);
}

double br_rand_f64(br_rand *r) {
  /* Top 53 bits scaled by 2^-53; division by a power of two is exact. */
  return (double)(br_rand_u64(r) >> 11) * (1.0 / 9007199254740992.0);
}

float br_rand_f32(br_rand *r) {
  /* Top 24 bits scaled by 2^-24. */
  return (float)(br_rand_u64(r) >> 40) * (1.0f / 16777216.0f);
}

void br_rand_shuffle(br_rand *r, usize count, br_rand_swap_fn swap, void *ctx) {
  usize i;

  if (swap == NULL || count < 2u) {
    return;
  }

  /* Fisher-Yates: for each position from the top down, swap with a uniformly
     chosen index in [0, i]. */
  for (i = count - 1u; i > 0u; --i) {
    usize j = (usize)br_rand_u64_below(r, (u64)i + 1u);
    swap(ctx, i, j);
  }
}

br_status br_rand_seed_entropy(br_rand *r) {
  u64 words[2];
  br_status status = br_rand_entropy_fill(words, sizeof(words));

  if (status != BR_STATUS_OK) {
    return status;
  }
  r->hi = words[0];
  r->lo = words[1];
  return BR_STATUS_OK;
}
