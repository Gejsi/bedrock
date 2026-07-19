#include <assert.h>
#include <string.h>

#include <bedrock.h>

static br_bytes_view bv(const char *s) {
  return br_bytes_view_make(s, strlen(s));
}

/* RFC 4648 section 10 test vectors: input -> standard-alphabet padded output. */
static void test_rfc4648_vectors(void) {
  static const struct {
    const char *decoded;
    const char *encoded;
  } vectors[] = {
    {"", ""},
    {"f", "Zg=="},
    {"fo", "Zm8="},
    {"foo", "Zm9v"},
    {"foob", "Zm9vYg=="},
    {"fooba", "Zm9vYmE="},
    {"foobar", "Zm9vYmFy"},
  };
  size_t i;

  for (i = 0u; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
    br_bytes_view in = bv(vectors[i].decoded);
    uint8_t enc_buf[32];
    uint8_t dec_buf[32];
    br_io_result enc;
    br_decode_into_result dec;

    enc = br_base64_encode_into(br_base64_std(), in, enc_buf, sizeof(enc_buf));
    assert(enc.status == BR_STATUS_OK);
    assert(enc.count == strlen(vectors[i].encoded));
    assert(memcmp(enc_buf, vectors[i].encoded, enc.count) == 0);

    dec = br_base64_decode_into(br_base64_std(), bv(vectors[i].encoded), dec_buf, sizeof(dec_buf));
    assert(dec.status == BR_STATUS_OK);
    assert(dec.count == in.len);
    assert(memcmp(dec_buf, in.data, in.len) == 0);
  }
}

/* Raw (unpadded) standard encoding of the same vectors: no trailing '='. */
static void test_raw_std_vectors(void) {
  static const struct {
    const char *decoded;
    const char *encoded;
  } vectors[] = {
    {"f", "Zg"},
    {"fo", "Zm8"},
    {"foo", "Zm9v"},
    {"foob", "Zm9vYg"},
    {"fooba", "Zm9vYmE"},
  };
  size_t i;

  for (i = 0u; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
    br_bytes_view in = bv(vectors[i].decoded);
    uint8_t enc_buf[32];
    uint8_t dec_buf[32];
    br_io_result enc = br_base64_encode_into(br_base64_raw_std(), in, enc_buf, sizeof(enc_buf));
    br_decode_into_result dec;

    assert(enc.status == BR_STATUS_OK);
    assert(enc.count == strlen(vectors[i].encoded));
    assert(memcmp(enc_buf, vectors[i].encoded, enc.count) == 0);

    dec =
      br_base64_decode_into(br_base64_raw_std(), bv(vectors[i].encoded), dec_buf, sizeof(dec_buf));
    assert(dec.status == BR_STATUS_OK);
    assert(dec.count == in.len && memcmp(dec_buf, in.data, in.len) == 0);
  }
}

/* URL alphabet uses '-' and '_' where STD uses '+' and '/'. */
static void test_url_alphabet(void) {
  /* Bytes chosen so the std encoding contains both '+' and '/'. */
  static const uint8_t raw[] = {0xFB, 0xFF, 0xBF};
  br_bytes_view in = br_bytes_view_make(raw, sizeof(raw));
  uint8_t std_buf[8];
  uint8_t url_buf[8];
  uint8_t dec_buf[8];
  br_io_result se = br_base64_encode_into(br_base64_std(), in, std_buf, sizeof(std_buf));
  br_io_result ue = br_base64_encode_into(br_base64_url(), in, url_buf, sizeof(url_buf));
  br_decode_into_result ud;

  assert(se.status == BR_STATUS_OK && ue.status == BR_STATUS_OK);
  assert(memchr(std_buf, '+', se.count) != NULL || memchr(std_buf, '/', se.count) != NULL);
  assert(memchr(url_buf, '+', ue.count) == NULL && memchr(url_buf, '/', ue.count) == NULL);
  /* URL output should contain the substituted chars. */
  assert(memchr(url_buf, '-', ue.count) != NULL || memchr(url_buf, '_', ue.count) != NULL);

  ud = br_base64_decode_into(
    br_base64_url(), br_bytes_view_make(url_buf, ue.count), dec_buf, sizeof(dec_buf));
  assert(ud.status == BR_STATUS_OK && ud.count == in.len && memcmp(dec_buf, raw, in.len) == 0);
}

/* A byte outside the active alphabet -> INVALID_ENCODING at its index. NO
   whitespace skipping (Go's decoder skips \r\n; Bedrock rejects). */
static void test_bad_byte_and_whitespace(void) {
  uint8_t buf[8];
  br_decode_into_result d;

  /* '\n' at index 2 is not skipped; it is a hard error at that offset. */
  d = br_base64_decode_into(br_base64_std(), bv("Zm\n9v"), buf, sizeof(buf));
  assert(d.status == BR_STATUS_INVALID_ENCODING && d.error_offset == 2u);

  /* '!' mid-stream. */
  d = br_base64_decode_into(br_base64_std(), bv("Zm9!"), buf, sizeof(buf));
  assert(d.status == BR_STATUS_INVALID_ENCODING && d.error_offset == 3u);

  /* URL alphabet must reject '+' and '/' (they belong to STD). */
  d = br_base64_decode_into(br_base64_url(), bv("Zm+v"), buf, sizeof(buf));
  assert(d.status == BR_STATUS_INVALID_ENCODING && d.error_offset == 2u);
}

/* Structural-length impossibility: decoded_len returns 0 AND decode rejects. */
static void test_structural_length(void) {
  uint8_t buf[8];
  br_decode_into_result d;

  /* Padded length not a multiple of 4. */
  assert(br_base64_decoded_len(br_base64_std(), bv("Zm9")) == 0u);
  d = br_base64_decode_into(br_base64_std(), bv("Zm9"), buf, sizeof(buf));
  assert(d.status == BR_STATUS_INVALID_ENCODING && d.error_offset == 2u);

  /* Raw length congruent to 1 mod 4 is impossible. */
  assert(br_base64_decoded_len(br_base64_raw_std(), bv("Zm9vY")) == 0u);
  d = br_base64_decode_into(br_base64_raw_std(), bv("Zm9vY"), buf, sizeof(buf));
  assert(d.status == BR_STATUS_INVALID_ENCODING && d.error_offset == 4u);
}

/* strict rejects a final quantum with non-zero unused bits; lenient masks. */
static void test_strict_vs_lenient(void) {
  uint8_t buf[8];
  br_base64_encoding lenient = br_base64_raw_std();
  br_base64_encoding strict = br_base64_raw_std();
  br_decode_into_result d;

  strict.strict = true;

  /*
  "Zk" is a 2-char quantum decoding to one byte; 'k'=36=0b100100 has non-zero
  low 4 bits, so it is non-canonical. Lenient masks and accepts; strict rejects
  at the offending character (index 1).
  */
  d = br_base64_decode_into(lenient, bv("Zk"), buf, sizeof(buf));
  assert(d.status == BR_STATUS_OK && d.count == 1u);

  d = br_base64_decode_into(strict, bv("Zk"), buf, sizeof(buf));
  assert(d.status == BR_STATUS_INVALID_ENCODING && d.error_offset == 1u);

  /* A canonical 2-char quantum ("Zg" -> 'f') passes strict. */
  d = br_base64_decode_into(strict, bv("Zg"), buf, sizeof(buf));
  assert(d.status == BR_STATUS_OK && d.count == 1u);
}

/* Undersized dst -> SHORT_BUFFER, count 0, nothing written (never truncates). */
static void test_short_buffer(void) {
  uint8_t small[2];
  br_io_result e;
  br_decode_into_result d;

  e = br_base64_encode_into(br_base64_std(), bv("foobar"), small, sizeof(small));
  assert(e.status == BR_STATUS_SHORT_BUFFER && e.count == 0u);

  d = br_base64_decode_into(br_base64_std(), bv("Zm9vYmFy"), small, sizeof(small));
  assert(d.status == BR_STATUS_SHORT_BUFFER && d.count == 0u);
}

/* Allocating encode/decode round-trip, and free-on-error leaves no live alloc. */
static void test_allocating_and_free_on_error(void) {
  br_tracking_allocator tracking;
  br_allocator alloc;
  br_bytes_result enc;
  br_decode_result dec;
  br_decode_result bad;

  memset(&tracking, 0, sizeof(tracking));
  br_tracking_allocator_init(&tracking, br_allocator_heap(), br_allocator_heap());
  alloc = br_tracking_allocator_allocator(&tracking);

  enc = br_base64_encode(br_base64_std(), bv("foobar"), alloc);
  assert(enc.status == BR_STATUS_OK);
  dec = br_base64_decode(br_base64_std(), br_bytes_view_from_bytes(enc.value), alloc);
  assert(dec.status == BR_STATUS_OK && dec.value.len == 6u);
  assert(memcmp(dec.value.data, "foobar", 6u) == 0);

  /* A malformed decode must free its scratch: no live allocation, empty value. */
  bad = br_base64_decode(br_base64_std(), bv("Zm9!"), alloc);
  assert(bad.status == BR_STATUS_INVALID_ENCODING);
  assert(bad.value.data == NULL && bad.value.len == 0u);
  assert(bad.error_offset == 3u);

  (void)br_bytes_free(enc.value, alloc);
  (void)br_bytes_free(dec.value, alloc);
  assert(tracking.stats.live_allocation_count == 0u);
  br_tracking_allocator_destroy(&tracking);
}

int main(void) {
  test_rfc4648_vectors();
  test_raw_std_vectors();
  test_url_alphabet();
  test_bad_byte_and_whitespace();
  test_structural_length();
  test_strict_vs_lenient();
  test_short_buffer();
  test_allocating_and_free_on_error();
  return 0;
}
