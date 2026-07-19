#include <assert.h>
#include <string.h>

#include <bedrock.h>

static br_bytes_view bytes(const uint8_t *p, size_t n) {
  return br_bytes_view_make(p, n);
}

/* Known DWARF/LEB128 encodings for unsigned values. */
static void test_uleb128_known_vectors(void) {
  static const struct {
    uint64_t value;
    uint8_t bytes[10];
    size_t len;
  } vectors[] = {
    {0u, {0x00}, 1u},
    {1u, {0x01}, 1u},
    {127u, {0x7f}, 1u},
    {128u, {0x80, 0x01}, 2u},
    {300u, {0xac, 0x02}, 2u},
    {624485u, {0xe5, 0x8e, 0x26}, 3u},
  };
  size_t i;

  for (i = 0u; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
    uint8_t buf[10];
    br_io_result e = br_uleb128_encode(vectors[i].value, buf, sizeof(buf));
    br_uleb128_result d;

    assert(e.status == BR_STATUS_OK);
    assert(e.count == vectors[i].len);
    assert(memcmp(buf, vectors[i].bytes, e.count) == 0);
    assert(br_uleb128_encoded_len(vectors[i].value) == vectors[i].len);

    d = br_uleb128_decode(bytes(vectors[i].bytes, vectors[i].len));
    assert(d.status == BR_STATUS_OK && d.value == vectors[i].value && d.size == vectors[i].len);
  }
}

/* Known signed (DWARF sign-extended, NOT zig-zag) encodings. */
static void test_ileb128_known_vectors(void) {
  static const struct {
    int64_t value;
    uint8_t bytes[10];
    size_t len;
  } vectors[] = {
    {0, {0x00}, 1u},
    {1, {0x01}, 1u},
    {-1, {0x7f}, 1u},
    {2, {0x02}, 1u},
    {-2, {0x7e}, 1u},
    {63, {0x3f}, 1u},
    {64, {0xc0, 0x00}, 2u},
    {-64, {0x40}, 1u},
    {-127, {0x81, 0x7f}, 2u},
    {-128, {0x80, 0x7f}, 2u},
  };
  size_t i;

  for (i = 0u; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
    uint8_t buf[10];
    br_io_result e = br_ileb128_encode(vectors[i].value, buf, sizeof(buf));
    br_ileb128_result d;

    assert(e.status == BR_STATUS_OK);
    assert(e.count == vectors[i].len);
    assert(memcmp(buf, vectors[i].bytes, e.count) == 0);
    assert(br_ileb128_encoded_len(vectors[i].value) == vectors[i].len);

    d = br_ileb128_decode(bytes(vectors[i].bytes, vectors[i].len));
    assert(d.status == BR_STATUS_OK && d.value == vectors[i].value && d.size == vectors[i].len);
  }
}

static void test_uleb128_roundtrip(void) {
  static const uint64_t values[] = {0u,
                                    1u,
                                    2u,
                                    126u,
                                    127u,
                                    128u,
                                    129u,
                                    16383u,
                                    16384u,
                                    16385u,
                                    2097151u,
                                    2097152u,
                                    0x7FFFFFFFu,
                                    0x80000000u,
                                    0xFFFFFFFFu,
                                    0x7FFFFFFFFFFFFFFFuLL,
                                    0x8000000000000000uLL,
                                    0xFFFFFFFFFFFFFFFFuLL};
  size_t i;
  int s;

  /* Also sweep every 7-bit shift boundary +/- 1. */
  for (s = 7; s < 64; s += 7) {
    uint64_t base = (uint64_t)1u << s;
    uint64_t cases[3];
    size_t k;

    cases[0] = base - 1u;
    cases[1] = base;
    cases[2] = base + 1u;
    for (k = 0u; k < 3u; ++k) {
      uint8_t buf[10];
      br_io_result e = br_uleb128_encode(cases[k], buf, sizeof(buf));
      br_uleb128_result d;

      assert(e.status == BR_STATUS_OK);
      d = br_uleb128_decode(bytes(buf, e.count));
      assert(d.status == BR_STATUS_OK && d.value == cases[k] && d.size == e.count);
    }
  }

  for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i) {
    uint8_t buf[10];
    br_io_result e = br_uleb128_encode(values[i], buf, sizeof(buf));
    br_uleb128_result d;

    assert(e.status == BR_STATUS_OK);
    d = br_uleb128_decode(bytes(buf, e.count));
    assert(d.status == BR_STATUS_OK && d.value == values[i]);
  }
}

static void test_ileb128_roundtrip(void) {
  static const int64_t values[] = {0,
                                   1,
                                   -1,
                                   2,
                                   -2,
                                   63,
                                   -64,
                                   64,
                                   -65,
                                   8191,
                                   -8192,
                                   8192,
                                   0x7FFFFFFF,
                                   -0x80000000LL,
                                   0x7FFFFFFFFFFFFFFFLL,
                                   INT64_MIN,
                                   INT64_MIN + 1};
  size_t i;
  int s;

  for (s = 6; s < 63; s += 7) {
    int64_t base = (int64_t)1 << s;
    int64_t cases[4];
    size_t k;

    cases[0] = base - 1;
    cases[1] = base;
    cases[2] = -base;
    cases[3] = -base - 1;
    for (k = 0u; k < 4u; ++k) {
      uint8_t buf[10];
      br_io_result e = br_ileb128_encode(cases[k], buf, sizeof(buf));
      br_ileb128_result d;

      assert(e.status == BR_STATUS_OK);
      d = br_ileb128_decode(bytes(buf, e.count));
      assert(d.status == BR_STATUS_OK && d.value == cases[k] && d.size == e.count);
    }
  }

  for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i) {
    uint8_t buf[10];
    br_io_result e = br_ileb128_encode(values[i], buf, sizeof(buf));
    br_ileb128_result d;

    assert(e.status == BR_STATUS_OK);
    d = br_ileb128_decode(bytes(buf, e.count));
    assert(d.status == BR_STATUS_OK && d.value == values[i]);
  }
}

/* INT64_MIN legitimately fills all 10 bytes and must be accepted. */
static void test_ileb128_int64_min_full_width(void) {
  uint8_t buf[10];
  br_io_result e = br_ileb128_encode(INT64_MIN, buf, sizeof(buf));
  br_ileb128_result d;

  assert(e.status == BR_STATUS_OK && e.count == 10u);
  assert(buf[9] == 0x7fu); /* final byte carries the sign padding */
  d = br_ileb128_decode(bytes(buf, e.count));
  assert(d.status == BR_STATUS_OK && d.value == INT64_MIN && d.size == 10u);
}

static void test_truncation_and_empty(void) {
  static const uint8_t truncated[] = {0x80, 0x80}; /* continuation, then EOF */
  br_uleb128_result u;
  br_ileb128_result s;

  u = br_uleb128_decode(bytes(truncated, sizeof(truncated)));
  assert(u.status == BR_STATUS_UNEXPECTED_EOF && u.size == 2u);
  s = br_ileb128_decode(bytes(truncated, sizeof(truncated)));
  assert(s.status == BR_STATUS_UNEXPECTED_EOF && s.size == 2u);

  u = br_uleb128_decode(bytes(NULL, 0u));
  assert(u.status == BR_STATUS_UNEXPECTED_EOF && u.size == 0u);
}

static void test_overflow(void) {
  /* 10 continuation bytes then a value byte = an 11th byte: too large. */
  static const uint8_t eleven[] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01};
  /* 10th unsigned byte 0x02 sets a bit past bit 63. */
  static const uint8_t u_big10[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02};
  br_uleb128_result u;
  br_ileb128_result s;

  u = br_uleb128_decode(bytes(eleven, sizeof(eleven)));
  assert(u.status == BR_STATUS_INVALID_ENCODING);
  u = br_uleb128_decode(bytes(u_big10, sizeof(u_big10)));
  assert(u.status == BR_STATUS_INVALID_ENCODING);

  /*
  Signed: INT64_MIN's valid 10th byte is 0x7f. Perturbing its sign-padding bits
  (0x7f -> 0x3f: bit that should match the sign now disagrees) is malformed.
  This is the lead's explicit negative case beyond outright rejection.
  */
  {
    static const uint8_t s_bad_pad[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x3f};
    s = br_ileb128_decode(bytes(s_bad_pad, sizeof(s_bad_pad)));
    assert(s.status == BR_STATUS_INVALID_ENCODING);
  }
  /* An 11th signed byte is also too large. */
  s = br_ileb128_decode(bytes(eleven, sizeof(eleven)));
  assert(s.status == BR_STATUS_INVALID_ENCODING);
}

static void test_encode_short_buffer(void) {
  uint8_t small[1];
  br_io_result e;

  /* 128 needs 2 bytes; a 1-byte dst must fail with nothing written. */
  e = br_uleb128_encode(128u, small, sizeof(small));
  assert(e.status == BR_STATUS_SHORT_BUFFER && e.count == 0u);
  e = br_uleb128_encode(128u, NULL, 0u);
  assert(e.status == BR_STATUS_SHORT_BUFFER && e.count == 0u);
  e = br_ileb128_encode(-128, small, sizeof(small));
  assert(e.status == BR_STATUS_SHORT_BUFFER && e.count == 0u);
}

/* A valid varint followed by trailing bytes consumes only its own bytes. */
static void test_prefix_consumption(void) {
  static const uint8_t buf[] = {0xac, 0x02, 0xff, 0xff}; /* 300, then junk */
  br_uleb128_result d = br_uleb128_decode(bytes(buf, sizeof(buf)));

  assert(d.status == BR_STATUS_OK && d.value == 300u && d.size == 2u);
}

int main(void) {
  test_uleb128_known_vectors();
  test_ileb128_known_vectors();
  test_uleb128_roundtrip();
  test_ileb128_roundtrip();
  test_ileb128_int64_min_full_width();
  test_truncation_and_empty();
  test_overflow();
  test_encode_short_buffer();
  test_prefix_consumption();
  return 0;
}
