#ifndef BEDROCK_RAND_RAND_H
#define BEDROCK_RAND_RAND_H

#include <bedrock/base.h>

BR_EXTERN_C_BEGIN

/*
Explicit-state, non-cryptographic pseudo-random number generation: PCG64 with
the DXSM output permutation. A given seed produces one well-specified stream,
identical on every platform and endianness (the state is pure `uint64_t`
arithmetic), so seeded runs are exactly reproducible.

NOT cryptographically secure: PCG is fast and statistically strong but
predictable from observed output. For keys, tokens, or nonces, draw from the OS
entropy source (`br_rand_entropy_fill`), never from the generator stream.

Every call takes an explicit generator pointer; there is no global or ambient
state. This makes the module thread-safe by construction: give each thread its
own `br_rand` and no locking is needed. Sharing one `br_rand` across threads
without external synchronization is a data race, like any other mutable value.
*/

typedef struct br_rand {
  uint64_t hi;
  uint64_t lo;
} br_rand;

/*
Seed the generator directly with a 128-bit state. The same (seed1, seed2) pair
always produces the same stream. Cannot fail.

A zero-initialized `br_rand` is already a valid generator, equivalent to
`br_rand_seed(r, 0, 0)`; seeding only chooses a different stream.
*/
void br_rand_seed(br_rand *r, uint64_t seed1, uint64_t seed2);

/*
Seed both halves of the state from the OS entropy source. This is the default
for "I just want randomness". Returns the entropy status; it is the only seeding
call that can fail (e.g. BR_STATUS_NOT_SUPPORTED on a platform without an
entropy source).
*/
br_status br_rand_seed_entropy(br_rand *r);

/*
Draw a uniformly distributed value. These are hot-path calls and cannot fail, so
they return the value directly.
*/
uint64_t br_rand_u64(br_rand *r);
uint32_t br_rand_u32(br_rand *r);

/*
Draw a uniformly distributed value in `[0, bound)` with no modulo bias. A
`bound` of 0 returns 0.
*/
uint64_t br_rand_u64_below(br_rand *r, uint64_t bound);

/*
Draw a uniformly distributed value in `[lo, hi)`. If `lo >= hi` the range is
empty and `lo` is returned (defined, never undefined behavior).
*/
int64_t br_rand_i64_between(br_rand *r, int64_t lo, int64_t hi);

/*
Draw a uniformly distributed value in `[0, 1)`.
*/
double br_rand_f64(br_rand *r);
float br_rand_f32(br_rand *r);

/*
Swap callback for `br_rand_shuffle`: exchange the elements at indices `i` and
`j` within the caller's collection.
*/
typedef void (*br_rand_swap_fn)(void *ctx, size_t i, size_t j);

/*
Randomly permute `count` elements using the Fisher-Yates shuffle, exchanging
elements through the caller-provided `swap` callback. No allocation, no element
size — the caller owns the storage and the swap.
*/
void br_rand_shuffle(br_rand *r, size_t count, br_rand_swap_fn swap, void *ctx);

/*
Fill `dst` with `len` cryptographically strong bytes from the OS entropy source.
Fills the whole buffer or returns an error (never a partial fill reported as
success); a `len` of 0 succeeds immediately. Returns BR_STATUS_NOT_SUPPORTED on
a platform with no entropy source, so the caller must seed explicitly rather
than fall back to a weak seed.

This is the module's only OS-entropy surface; `br_rand_seed_entropy` wraps it.
*/
br_status br_rand_entropy_fill(void *dst, size_t len);

BR_EXTERN_C_END

#endif
