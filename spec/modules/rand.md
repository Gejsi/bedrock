# Rand

Explicit-state, non-cryptographic pseudo-random number generation: PCG64 with
the DXSM output permutation, matching Go v2's `math/rand/v2` generator so the
output stream is differential-locked against a machine-checkable reference. No
global state, no hidden seeding — every generator is a caller-held value.

NOT cryptographically secure: PCG is fast and statistically excellent but
predictable from observed output. For keys/tokens/nonces draw from the OS
entropy source (`br_rand_entropy_fill`), never from the PRNG stream.

## Scope

| Facility | Surface | Status |
| --- | --- | --- |
| generator state + seeding | `br_rand`, `br_rand_seed`/`_seed_entropy` | planned |
| raw draws | `br_rand_u64`/`_u32` | planned |
| bounded draws (unbiased) | `br_rand_u64_below`/`_i64_between` | planned |
| unit float | `br_rand_f64`/`_f32` (`[0,1)`) | planned |
| shuffle | `br_rand_shuffle` (callback swap) | planned |
| OS entropy | `br_rand_entropy_fill` (seed source; future uuid dep) | planned |

Deferred: non-uniform distributions (normal/exponential/perm). Skipped: the
distribution zoo (gamma/beta/pareto/weibull/von_mises/cauchy/laplace/zipf/...);
Go itself ships only Normal/Exp/Perm/Shuffle, and Bedrock matches that line one
notch tighter. No global convenience generator (no hidden state). f16 excluded.

Header `include/bedrock/rand/rand.h`; impl `src/rand/rand.c` (portable
generator) + `src/rand/entropy_*.c` (per-OS, mirroring the sync futex split).
Umbrella `include/bedrock/rand.h`.

## Conventions

- Every call takes an explicit `br_rand *` — no global generator, no ambient
  state (deviation from C `rand()` and Odin's context-carried generator). This
  makes rand thread-safe BY CONSTRUCTION: one `br_rand` per thread (or per
  use), never shared, needs no lock — the property C's global `rand()` cannot
  offer. Sharing one `br_rand` across threads without external synchronization
  is a data race, same as any other mutable value.
- Draw calls return the number directly (no `br_status`) — generation is a hot
  path and cannot fail. Only entropy seeding returns a status.
- Standard C types in the signatures; short aliases stay internal.

## Generator: PCG64 DXSM

A 128-bit-state PCG with the DXSM (double-xorshift-multiply) output permutation,
ported byte-for-byte from Go v2's `pcg.go` so identical seeds produce identical
streams (the correctness anchor — see Testing). The state is two `uint64_t`
with a FIXED 128-bit increment constant baked into the step; classic PCG's
settable-stream increment is intentionally dropped (see Deviations).

```c
typedef struct br_rand { uint64_t hi; uint64_t lo; } br_rand;
```

Step (`state = state*MUL + INC`, mod 2^128) over the landed bits primitives —
`br_bits_mul_u64` (hi/lo) and `br_bits_add_u64` (carry):
```
mulHi=2549297995355413924  mulLo=4865540595714422341
incHi=6364136223846793005  incLo=1442695040888963407
{hi,lo} = br_bits_mul_u64(r->lo, mulLo)
hi += r->hi*mulLo + r->lo*mulHi
{lo,c} = br_bits_add_u64(lo, incLo, 0)
{hi,_} = br_bits_add_u64(hi, incHi, c)
r->hi = hi; r->lo = lo
```
DXSM output (on the step's returned hi/lo):
```
const cheapMul = 0xda942042e4dd58b5
hi ^= hi >> 32;  hi *= cheapMul;  hi ^= hi >> 48;  hi *= (lo | 1);  return hi
```
DXSM is xorshift+multiply on the high half — it needs NO rotate, so the bits
dependency is `mul_u64` + `add_u64` only (no `rotate_*`).

## Seeding

```c
void      br_rand_seed(br_rand *r, uint64_t seed1, uint64_t seed2); /* direct 128-bit state (Go NewPCG); infallible */
br_status br_rand_seed_entropy(br_rand *r);                         /* fill both halves from OS entropy */
```

- `br_rand_seed` assigns `{hi=seed1, lo=seed2}` directly, mirroring Go's
  `NewPCG(seed1, seed2)` — the reproducible form; same pair → same stream
  forever. The two-word seed IS the API: there is deliberately no single-u64
  convenience (see Deviations).
- `br_rand_seed_entropy` fills both halves from `br_rand_entropy_fill`; it is the
  default for "I just want randomness" and the ONLY seed that can fail (returns
  the entropy status). The deterministic seed is pure assignment and cannot
  fail, so it is `void`.

## Zero-value contract

A zero-initialized `br_rand` is a VALID generator, equivalent to
`br_rand_seed(r, 0, 0)`: it produces a well-defined deterministic stream,
exactly matching Go's documented "a zero PCG is equivalent to NewPCG(0, 0)".
Unlike `br_thread`, `br_rand` IS zero-value-ready — PCG's fixed nonzero
increment moves the state off zero on the first step, so the all-zero state is
not a fixed point and the DXSM `(lo|1)` never collapses. No init call is
required; seed only to choose a different stream.

## Draw API (status-free — hot path)

```c
uint64_t br_rand_u64(br_rand *r);   /* one PCG64 DXSM step */
uint32_t br_rand_u32(br_rand *r);   /* top 32 bits of a u64 draw */

uint64_t br_rand_u64_below(br_rand *r, uint64_t bound);          /* [0,bound); bound==0 -> 0 */
int64_t  br_rand_i64_between(br_rand *r, int64_t lo, int64_t hi); /* [lo,hi); lo>=hi -> lo */

double br_rand_f64(br_rand *r);  /* [0,1): top 53 bits * 2^-53 */
float  br_rand_f32(br_rand *r);  /* [0,1): top 24 bits * 2^-24 */
```

Bounded draws use Lemire multiply-shift-reject, matching Go v2's `uint64n`:
power-of-two `bound` takes a mask fast path; otherwise `{hi,lo} =
br_bits_mul_u64(draw, bound)`, return `hi`, and reject (redraw) only when
`lo < (-bound % bound)` — the expensive modulo is computed only inside the rare
`lo < bound` case, so most draws skip it. Exactly uniform, no modulo bias.
`u64_below(r,0)` returns 0; `i64_between` with `lo >= hi` returns `lo` (defined,
never UB — C's no-panic rule; Go panics here, we don't). Unit floats divide top
mantissa bits by a power of two, never `x % k`.

## Shuffle (callback swap)

```c
typedef void (*br_rand_swap_fn)(void *ctx, size_t i, size_t j);
void br_rand_shuffle(br_rand *r, size_t count, br_rand_swap_fn swap, void *ctx);
```

Fisher-Yates over `count` elements via a caller swap callback — the C form of
Go v2's `Shuffle(n, swap)`, with no element size, no memcpy, no allocation. Each
index comes from `br_rand_u64_below` (unbiased). This callback-over-element-size
shape is the pattern the future sort/container redesign should follow.

## OS entropy (seed source + future uuid dependency)

```c
br_status br_rand_entropy_fill(void *dst, size_t len);
```

Fills `dst` with `len` cryptographically-strong OS random bytes: `getrandom` on
Linux (loop on `EINTR` and on short returns), `getentropy` on macOS/BSD (256-byte
per-call cap — loop for larger; `arc4random_buf` is an acceptable can't-fail
alternative there), `BCryptGenRandom` on Windows (runtime-resolved via
`GetProcAddress` like the futex backend, so consumers need no import lib).
Unsupported targets return `BR_STATUS_NOT_SUPPORTED` so the caller MUST seed
explicitly — never a silent `time()`-style weak seed. Fills the WHOLE buffer or
returns an error (never reports partial fill as success); never blocks
indefinitely. This is the ONLY OS-entropy surface in Bedrock;
`br_rand_seed_entropy` wraps it, and a future `uuid` module draws its v4/v7
randomness from here (not from the PCG stream — PCG is not a CSPRNG).

## Deviations from Odin / notes

- PCG64 DXSM, ported from Go v2 (Odin ships pcg + xoshiro256; Bedrock ships one
  generator). DXSM, not classic XSL-RR — Go v2 (and NumPy) adopted DXSM; matching
  it gives a machine-checkable reference stream.
- Fixed increment, no settable streams: classic PCG offers independent substreams
  via a per-generator odd increment, but Go v2 uses a fixed increment and so does
  Bedrock, so the output is differential-locked against the pinned reference.
  Independent streams today = seed each generator from entropy. Settable streams
  return only if a concrete consumer needs them.
- No single-u64 seed convenience: Go v2's PCG offers only the two-word
  `NewPCG(seed1, seed2)`, and so does Bedrock — a third seeding spelling adds
  choice without capability. `br_rand_seed(r, 42, 0)` is the reproducible test
  idiom.
- No global generator / no ambient context (deviation from C `rand()` and Odin's
  context generator). Explicit `br_rand *` everywhere.
- Generation is status-free; only entropy seeding returns `br_status`.
- Non-crypto by contract, stated loudly; the CSPRNG need is served by
  `br_rand_entropy_fill`, not the PRNG.
- `i64_between(lo>=hi)`/`u64_below(0)` return a defined value, where Go panics
  (no-panic rule).
- xoshiro256++ is the no-multiply fallback if ever wanted; ChaCha8 (Go v2's
  crypto-grade generator) is deferred with the excluded crypto surface.

## Testing

- GOLDEN VECTORS (the anchor): generate reference outputs from the pinned Go v2
  `math/rand/v2` PCG for fixed `(seed1, seed2)` pairs, lock them into the suite,
  assert `br_rand_u64` reproduces them bit-identically. Because the port is
  byte-for-byte DXSM, this must match forever and across every platform/endianness
  (pure u64 arithmetic) — a portability + correctness regression guard in one.
- Zero-value: assert `br_rand{0}` reproduces the `br_rand_seed(r, 0, 0)` golden
  stream (not a constant) — pins the zero-value contract.
- Bounded uniformity: chi-square / bucket counts over many `u64_below(k)` for
  assorted k incl. k near 2^64 (exercises the reject zone) and powers of two (the
  mask path); assert no draw >= bound.
- Float range: `f64`/`f32` always in `[0,1)` over a large sweep; bucket
  uniformity.
- Shuffle: all permutations reachable for small n (catches a Fisher-Yates
  off-by-one); no index out of range (ASan).
- entropy_fill: distinct buffers across calls, len 0 handled, whole-buffer-or-error;
  per-OS path compiles and runs on each target (Linux local, others via CI).
- TSan: N threads each owning a private `br_rand`, no shared state — the
  anti-`rand()` no-race property.
