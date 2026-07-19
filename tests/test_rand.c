#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <bedrock.h>

/*
GOLDEN VECTORS — the differential-lock anchor. These are INDEPENDENT reference
outputs, not produced by this implementation:

- golden_1_2: the 20 expected NewPCG(1,2).Uint64() outputs embedded verbatim in
  Go's own upstream/go/src/math/rand/v2/pcg_test.go (TestPCG). Adopted as-is.
- golden_0_0: the NewPCG(0,0).Uint64() stream, generated offline with a
  throwaway `go run` program against math/rand/v2 (never committed; toolchain
  purity binds the build, not one-time vector generation). It pins the
  zero-value contract against GO's output — runtime-comparing br_rand{0} to
  br_rand_seed(0,0) would be trivially true (identical {0,0} bits) and test
  nothing; only an externally generated constant proves the {0,0} STREAM matches.
*/
static const uint64_t golden_1_2[] = {
  0xc4f5a58656eef510ull, 0x9dcec3ad077dec6cull, 0xc8d04605312f8088ull, 0xcbedc0dcb63ac19aull,
  0x3bf98798cae97950ull, 0x0a8c6d7f8d485abcull, 0x7ffa3780429cd279ull, 0x730ad2626b1c2f8eull,
  0x21ff2330f4a0ad99ull, 0x2f0901a1947094b0ull, 0xa9735a3cfbe36cefull, 0x71ddb0a01a12c84aull,
  0xf0e53e77a78453bbull, 0x1f173e9663be1e9dull, 0x657651da3ac4115eull, 0xc8987376b65a157bull,
  0xbb17008f5fca28e7ull, 0x8232bd645f29ed22ull, 0x12be8f07ad14c539ull, 0x54908a48e8e4736eull};

static const uint64_t golden_0_0[] = {0x38ffff682123e08aull,
                                      0xacfc572dc29cb1fdull,
                                      0x57ec35105c35c2dbull,
                                      0x70cf668abad6ac57ull,
                                      0x664e36a97266a5fbull,
                                      0x13b0ac0baf9607d2ull,
                                      0xba9c5b00f75963a1ull,
                                      0x7b35a9996fa10ee2ull};

static void test_golden_stream(void) {
  br_rand r;
  usize i;

  br_rand_seed(&r, 1u, 2u);
  for (i = 0u; i < sizeof(golden_1_2) / sizeof(golden_1_2[0]); ++i) {
    assert(br_rand_u64(&r) == golden_1_2[i]);
  }
}

static void test_zero_value_stream(void) {
  br_rand zeroed;
  br_rand seeded;
  usize i;

  /* A zero-initialized generator must produce Go's NewPCG(0,0) stream, and
     br_rand_seed(r,0,0) must produce the identical stream. */
  memset(&zeroed, 0, sizeof(zeroed));
  br_rand_seed(&seeded, 0u, 0u);
  for (i = 0u; i < sizeof(golden_0_0) / sizeof(golden_0_0[0]); ++i) {
    uint64_t z = br_rand_u64(&zeroed);
    uint64_t s = br_rand_u64(&seeded);
    assert(z == golden_0_0[i]);
    assert(s == golden_0_0[i]);
  }
}

static void test_u32_is_top_half(void) {
  /* u32 is the top 32 bits of a u64 draw from an equivalent generator. */
  br_rand a;
  br_rand b;
  int i;

  br_rand_seed(&a, 42u, 7u);
  br_rand_seed(&b, 42u, 7u);
  for (i = 0; i < 100; ++i) {
    assert(br_rand_u32(&a) == (uint32_t)(br_rand_u64(&b) >> 32));
  }
}

static void test_u64_below_bounds_and_zero(void) {
  br_rand r;
  int i;

  br_rand_seed(&r, 99u, 100u);
  assert(br_rand_u64_below(&r, 0u) == 0u); /* bound 0 -> 0 */

  /* Every draw strictly below the bound, across assorted bounds including a
     power of two (mask path) and a value near 2^64 (large reject zone). */
  for (i = 0; i < 100000; ++i) {
    assert(br_rand_u64_below(&r, 1000u) < 1000u);
    assert(br_rand_u64_below(&r, 1024u) < 1024u); /* power of two */
    assert(br_rand_u64_below(&r, 0xfffffffffffffffbull) < 0xfffffffffffffffbull);
    assert(br_rand_u64_below(&r, 1u) == 0u); /* bound 1 -> only 0 */
  }
}

static void test_u64_below_uniformity(void) {
  /* Coarse bucket check: draws below k=7 (not a power of two, exercises reject)
     should land in every bucket with roughly equal frequency. */
  br_rand r;
  enum { K = 7, DRAWS = 700000 };
  usize counts[K];
  int i;

  br_rand_seed(&r, 0xdeadbeefu, 0xcafef00du);
  for (i = 0; i < K; ++i) {
    counts[i] = 0u;
  }
  for (i = 0; i < DRAWS; ++i) {
    uint64_t v = br_rand_u64_below(&r, (uint64_t)K);
    assert(v < (uint64_t)K);
    counts[v] += 1u;
  }
  /* Expected DRAWS/K = 100000 each; allow a generous +-15% band. */
  for (i = 0; i < K; ++i) {
    assert(counts[i] > 85000u && counts[i] < 115000u);
  }
}

static void test_i64_between(void) {
  br_rand r;
  int i;

  br_rand_seed(&r, 5u, 6u);

  /* Empty / inverted range returns lo, no UB. */
  assert(br_rand_i64_between(&r, 5, 5) == 5);
  assert(br_rand_i64_between(&r, 10, 3) == 10);

  /* Normal range. */
  for (i = 0; i < 100000; ++i) {
    int64_t v = br_rand_i64_between(&r, -50, 50);
    assert(v >= -50 && v < 50);
  }

  /* Negative-only range. */
  for (i = 0; i < 10000; ++i) {
    int64_t v = br_rand_i64_between(&r, -100, -90);
    assert(v >= -100 && v < -90);
  }
}

static void test_i64_between_extremes(void) {
  /* The UB guard: lo=INT64_MIN with a near-2^64 span would overflow a naive
     signed `lo + draw`. Done in unsigned, it is defined; UBSan must stay quiet
     and every draw must be in range. */
  br_rand r;
  int i;

  br_rand_seed(&r, 0x0123456789abcdefull, 0xfedcba9876543210ull);
  for (i = 0; i < 200000; ++i) {
    int64_t v = br_rand_i64_between(&r, INT64_MIN, INT64_MAX);
    assert(v >= INT64_MIN && v < INT64_MAX);
  }
  /* A span that is exactly a power of two straddling zero. */
  for (i = 0; i < 10000; ++i) {
    int64_t v = br_rand_i64_between(&r, INT64_MIN, 0);
    assert(v >= INT64_MIN && v < 0);
  }
}

static void test_floats_in_range(void) {
  br_rand r;
  int i;

  br_rand_seed(&r, 7u, 8u);
  for (i = 0; i < 500000; ++i) {
    double d = br_rand_f64(&r);
    float f = br_rand_f32(&r);
    assert(d >= 0.0 && d < 1.0);
    assert(f >= 0.0f && f < 1.0f);
  }
}

static void test_float_buckets(void) {
  /* Coarse uniformity: f64 over 10 buckets. */
  br_rand r;
  enum { B = 10, DRAWS = 1000000 };
  usize counts[B];
  int i;

  br_rand_seed(&r, 11u, 13u);
  for (i = 0; i < B; ++i) {
    counts[i] = 0u;
  }
  for (i = 0; i < DRAWS; ++i) {
    double d = br_rand_f64(&r);
    usize bucket = (usize)(d * (double)B);
    if (bucket >= (usize)B) {
      bucket = (usize)B - 1u; /* guard the d just under 1.0 case */
    }
    counts[bucket] += 1u;
  }
  for (i = 0; i < B; ++i) {
    assert(counts[i] > 90000u && counts[i] < 110000u); /* expect 100000 +-10% */
  }
}

/* Shuffle test: track the permutation of an index array via the swap callback. */
typedef struct shuffle_ctx {
  size_t *elems;
} shuffle_ctx;

static void shuffle_swap(void *ctx, size_t i, size_t j) {
  shuffle_ctx *s = (shuffle_ctx *)ctx;
  size_t tmp = s->elems[i];
  s->elems[i] = s->elems[j];
  s->elems[j] = tmp;
}

static void test_shuffle_is_permutation(void) {
  br_rand r;
  enum { N = 50 };
  size_t elems[N];
  bool seen[N];
  shuffle_ctx ctx;
  int i;

  br_rand_seed(&r, 123u, 456u);
  for (i = 0; i < N; ++i) {
    elems[i] = (size_t)i;
  }
  ctx.elems = elems;
  br_rand_shuffle(&r, (size_t)N, shuffle_swap, &ctx);

  /* Result must be a permutation: every original index present exactly once. */
  memset(seen, 0, sizeof(seen));
  for (i = 0; i < N; ++i) {
    assert(elems[i] < (size_t)N);
    assert(!seen[elems[i]]);
    seen[elems[i]] = true;
  }

  /* count < 2 and NULL swap are no-ops. */
  br_rand_shuffle(&r, 1u, shuffle_swap, &ctx);
  br_rand_shuffle(&r, 0u, shuffle_swap, &ctx);
  br_rand_shuffle(&r, (size_t)N, NULL, &ctx);
}

static void test_shuffle_small_reaches_all_permutations(void) {
  /* For n=3 there are 6 permutations; a correct Fisher-Yates reaches all of
     them. Encode each result as a base-3 id and assert all 6 appear. */
  br_rand r;
  bool perm_seen[6];
  int trial;
  int i;

  br_rand_seed(&r, 2024u, 1u);
  memset(perm_seen, 0, sizeof(perm_seen));

  for (trial = 0; trial < 20000; ++trial) {
    size_t elems[3] = {0u, 1u, 2u};
    shuffle_ctx ctx;
    int id;
    ctx.elems = elems;
    br_rand_shuffle(&r, 3u, shuffle_swap, &ctx);
    id = (int)(elems[0] * 6u + elems[1] * 2u + elems[2]); /* unique per perm */
    /* Map the 6 valid encodings to 0..5. */
    switch (id) {
      case 0 * 6 + 1 * 2 + 2:
        perm_seen[0] = true;
        break;
      case 0 * 6 + 2 * 2 + 1:
        perm_seen[1] = true;
        break;
      case 1 * 6 + 0 * 2 + 2:
        perm_seen[2] = true;
        break;
      case 1 * 6 + 2 * 2 + 0:
        perm_seen[3] = true;
        break;
      case 2 * 6 + 0 * 2 + 1:
        perm_seen[4] = true;
        break;
      case 2 * 6 + 1 * 2 + 0:
        perm_seen[5] = true;
        break;
      default:
        assert(0); /* not a valid permutation of {0,1,2} */
    }
  }
  for (i = 0; i < 6; ++i) {
    assert(perm_seen[i]);
  }
}

static void test_entropy_fill(void) {
  uint8_t a[64];
  uint8_t b[64];
  br_status sa;
  br_status sb;

  /* len 0 always succeeds. */
  assert(br_rand_entropy_fill(NULL, 0u) == BR_STATUS_OK);

  sa = br_rand_entropy_fill(a, sizeof(a));
  sb = br_rand_entropy_fill(b, sizeof(b));
  /* On a supported platform, two fills should differ (probability of a 64-byte
     collision is negligible). On an unsupported platform, both report the
     status and we skip the difference check. */
  if (sa == BR_STATUS_OK && sb == BR_STATUS_OK) {
    assert(memcmp(a, b, sizeof(a)) != 0);
  } else {
    assert(sa == BR_STATUS_NOT_SUPPORTED);
  }
}

static void test_seed_entropy(void) {
  br_rand r;
  br_status status = br_rand_seed_entropy(&r);
  /* Either it seeded (OK) or the platform has no entropy source. */
  assert(status == BR_STATUS_OK || status == BR_STATUS_NOT_SUPPORTED);
  if (status == BR_STATUS_OK) {
    /* Seeded generator produces draws without crashing. */
    (void)br_rand_u64(&r);
  }
}

/* Each thread owns a PRIVATE br_rand on its own stack and shares nothing. This
   is the anti-rand() property: no global state, so no lock and no data race.
   Under TSan this must be clean. */
static int rand_thread_worker(void *arg) {
  uint64_t seed = *(uint64_t *)arg;
  br_rand local; /* private to this thread */
  uint64_t acc = 0u;
  int i;

  br_rand_seed(&local, seed, seed ^ 0x5555555555555555ull);
  for (i = 0; i < 10000; ++i) {
    acc ^= br_rand_u64(&local);
    acc ^= br_rand_u64_below(&local, 1000u);
    (void)br_rand_f64(&local);
  }
  return (int)(acc & 0x7fffffffu);
}

static void test_private_generators_no_race(void) {
  enum { N = 8 };
  br_thread threads[N];
  uint64_t seeds[N];
  int i;

  for (i = 0; i < N; ++i) {
    seeds[i] = (uint64_t)(i + 1);
    assert(br_thread_create(&threads[i], rand_thread_worker, &seeds[i]) == BR_STATUS_OK);
  }
  for (i = 0; i < N; ++i) {
    assert(br_thread_join(&threads[i], NULL) == BR_STATUS_OK);
  }
}

int main(void) {
  test_golden_stream();
  test_zero_value_stream();
  test_u32_is_top_half();
  test_u64_below_bounds_and_zero();
  test_u64_below_uniformity();
  test_i64_between();
  test_i64_between_extremes();
  test_floats_in_range();
  test_float_buckets();
  test_shuffle_is_permutation();
  test_shuffle_small_reaches_all_permutations();
  test_entropy_fill();
  test_seed_entropy();
  test_private_generators_no_race();
  return 0;
}
