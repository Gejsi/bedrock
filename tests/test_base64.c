#include <assert.h>
#include <string.h>

#include <bedrock.h>

static br_bytes_view bv(const char *s) {
  return br_bytes_view_make(s, strlen(s));
}

typedef struct test_base64_writer {
  u8 data[1024];
  usize len;
  usize max_accept;
  usize fail_after;
  usize calls;
  br_error error;
} test_base64_writer;

static test_base64_writer
test_base64_writer_make(usize max_accept, usize fail_after, br_error error) {
  test_base64_writer writer;

  memset(&writer, 0, sizeof(writer));
  writer.max_accept = max_accept;
  writer.fail_after = fail_after;
  writer.error = error;
  return writer;
}

static void test_base64_assert_no_native(br_native_error error) {
  assert(error.domain == BR_ERROR_DOMAIN_NONE);
  assert(error.code == 0u);
}

static br_i64_result test_base64_writer_proc(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence) {
  test_base64_writer *writer = (test_base64_writer *)context;
  usize count;

  BR_UNUSED(offset);
  BR_UNUSED(whence);

  switch (mode) {
    case BR_IO_MODE_WRITE:
      count = br_min_size(data_len, writer->max_accept);
      count = br_min_size(count, BR_ARRAY_COUNT(writer->data) - writer->len);
      count = br_min_size(count, writer->fail_after - writer->len);
      memcpy(writer->data + writer->len, data, count);
      writer->len += count;
      writer->calls += 1u;
      return br_i64_result_make_error(
        (i64)count, writer->len == writer->fail_after ? writer->error : BR_ERROR_OK);
    case BR_IO_MODE_QUERY:
      return br_stream_query_utility(br_io_mode_bit(BR_IO_MODE_WRITE));
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
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
    test_base64_assert_no_native(dec.native_error);
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
    test_base64_assert_no_native(dec.native_error);
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
  test_base64_assert_no_native(ud.native_error);
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

static void test_error_reports_partial_output(void) {
  uint8_t buf[8] = {0xa5u, 0xa5u, 0xa5u, 0xa5u, 0xa5u, 0xa5u, 0xa5u, 0xa5u};
  br_base64_encoding strict = br_base64_raw_std();
  br_decode_into_result d;

  strict.strict = true;
  /* "QUJD" writes "ABC"; the final non-canonical "Zk" then fails. */
  d = br_base64_decode_into(strict, bv("QUJDZk"), buf, sizeof(buf));
  assert(d.status == BR_STATUS_INVALID_ENCODING);
  assert(d.error_offset == 5u);
  assert(d.count == 3u);
  test_base64_assert_no_native(d.native_error);
  assert(memcmp(buf, "ABC", 3u) == 0);
  assert(buf[3] == 0xa5u);
}

static void test_decode_to_writer_standard(void) {
  enum { GROUP_COUNT = 129, ENCODED_LEN = GROUP_COUNT * 4, DECODED_LEN = GROUP_COUNT * 3 };
  u8 encoded[ENCODED_LEN];
  br_byte_buffer sink;
  br_bytes_view decoded;
  br_decode_into_result result;
  usize i;

  /* 129 quanta force one full 384-byte flush followed by a final flush. */
  for (i = 0u; i < GROUP_COUNT; ++i) {
    memcpy(encoded + i * 4u, "QUJD", 4u);
  }

  br_byte_buffer_init(&sink, br_allocator_heap());
  result = br_base64_decode_to_writer(
    br_base64_std(), br_bytes_view_make(encoded, sizeof(encoded)), br_byte_buffer_as_writer(&sink));
  assert(result.status == BR_STATUS_OK);
  assert(result.count == DECODED_LEN);
  assert(result.error_offset == 0u);
  test_base64_assert_no_native(result.native_error);

  decoded = br_byte_buffer_view(&sink);
  assert(decoded.len == DECODED_LEN);
  for (i = 0u; i < decoded.len; i += 3u) {
    assert(memcmp(decoded.data + i, "ABC", 3u) == 0);
  }
  br_byte_buffer_destroy(&sink);
}

static void test_decode_to_writer_raw_url(void) {
  static const u8 expected[] = {0xfbu, 0xffu};
  br_byte_buffer sink;
  br_decode_into_result result;

  br_byte_buffer_init(&sink, br_allocator_heap());
  result =
    br_base64_decode_to_writer(br_base64_raw_url(), bv("-_8"), br_byte_buffer_as_writer(&sink));
  assert(result.status == BR_STATUS_OK);
  assert(result.count == sizeof(expected));
  assert(result.error_offset == 0u);
  test_base64_assert_no_native(result.native_error);
  assert(
    br_bytes_equal(br_byte_buffer_view(&sink), br_bytes_view_make(expected, sizeof(expected))));
  br_byte_buffer_destroy(&sink);
}

static void test_decode_to_writer_malformed_input(void) {
  test_base64_writer sink;
  br_decode_into_result result;

  sink = test_base64_writer_make(SIZE_MAX, SIZE_MAX, BR_ERROR_OK);

  /* The valid first quantum remains buffered when the next quantum fails. */
  result = br_base64_decode_to_writer(
    br_base64_std(), bv("QUJD!A=="), br_stream_make(&sink, test_base64_writer_proc));
  assert(result.status == BR_STATUS_INVALID_ENCODING);
  assert(result.count == 0u);
  assert(result.error_offset == 4u);
  test_base64_assert_no_native(result.native_error);
  assert(sink.calls == 0u);
  assert(sink.len == 0u);
}

static void test_decode_to_writer_short_write(void) {
  test_base64_writer sink;
  br_decode_into_result result;

  sink = test_base64_writer_make(2u, 2u, br_error_make(BR_STATUS_SHORT_WRITE));

  result = br_base64_decode_to_writer(
    br_base64_std(), bv("Zm9vYmFy"), br_stream_make(&sink, test_base64_writer_proc));
  assert(result.status == BR_STATUS_SHORT_WRITE);
  assert(result.count == 2u);
  assert(result.error_offset == 0u);
  test_base64_assert_no_native(result.native_error);
  assert(sink.calls == 1u);
  assert(sink.len == 2u);
  assert(memcmp(sink.data, "fo", 2u) == 0);
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
  test_base64_assert_no_native(d.native_error);
}

static void test_encode_to_writer_native_error(void) {
  static const struct {
    usize src_len;
    usize fail_after;
  } cases[] = {
    {384u, 1u},   /* Failure in the first full flush. */
    {2u, 1u},     /* Failure in a tail-only write. */
    {768u, 514u}, /* A completed flush, then a failing full flush. */
    {386u, 514u}, /* A completed flush, then a failing tail. */
    {1u, 0u},     /* A native failure without any accepted bytes. */
  };
  br_base64_encoding encodings[] = {br_base64_std(), br_base64_raw_url()};
  br_error errors[] = {
    br_error_make_native(BR_STATUS_IO_ERROR, BR_ERROR_DOMAIN_POSIX_ERRNO, 5u),
    br_error_make_native(BR_STATUS_IO_ERROR, BR_ERROR_DOMAIN_WIN32, 112u),
  };
  u8 raw[768];
  u8 expected[1024];

  memset(raw, 'A', sizeof(raw));
  for (usize e = 0u; e < BR_ARRAY_COUNT(encodings); ++e) {
    for (usize n = 0u; n < BR_ARRAY_COUNT(errors); ++n) {
      for (usize i = 0u; i < BR_ARRAY_COUNT(cases); ++i) {
        br_bytes_view src = br_bytes_view_make(raw, cases[i].src_len);
        test_base64_writer sink = test_base64_writer_make(7u, cases[i].fail_after, errors[n]);
        br_io_result encoded = br_base64_encode_into(encodings[e], src, expected, sizeof(expected));
        br_io_result result = br_base64_encode_to_writer(
          encodings[e], src, br_stream_make(&sink, test_base64_writer_proc));

        assert(encoded.status == BR_STATUS_OK);
        assert(result.status == errors[n].status);
        assert(result.count == cases[i].fail_after);
        assert(result.native_error.domain == errors[n].native.domain);
        assert(result.native_error.code == errors[n].native.code);
        assert(sink.len == result.count);
        assert(memcmp(sink.data, expected, result.count) == 0);
      }
    }
  }
}

static void test_decode_to_writer_native_error(void) {
  static const struct {
    usize groups;
    usize fail_after;
  } cases[] = {
    {128u, 1u},   /* Failure in the first full flush. */
    {2u, 1u},     /* Failure in a tail-only write. */
    {256u, 386u}, /* A completed flush, then a failing full flush. */
    {129u, 386u}, /* A completed flush, then a failing tail. */
    {1u, 0u},     /* A native failure without any accepted bytes. */
  };
  static const u8 decoded[] = {'A', 'B', 'C'};
  br_error errors[] = {
    br_error_make_native(BR_STATUS_IO_ERROR, BR_ERROR_DOMAIN_POSIX_ERRNO, 5u),
    br_error_make_native(BR_STATUS_IO_ERROR, BR_ERROR_DOMAIN_WIN32, 112u),
  };
  u8 encoded[1024];

  for (usize i = 0u; i < sizeof(encoded); i += 4u) {
    memcpy(encoded + i, "QUJD", 4u);
  }
  for (usize n = 0u; n < BR_ARRAY_COUNT(errors); ++n) {
    for (usize i = 0u; i < BR_ARRAY_COUNT(cases); ++i) {
      test_base64_writer sink = test_base64_writer_make(7u, cases[i].fail_after, errors[n]);
      br_decode_into_result result =
        br_base64_decode_to_writer(br_base64_std(),
                                   br_bytes_view_make(encoded, cases[i].groups * 4u),
                                   br_stream_make(&sink, test_base64_writer_proc));

      assert(result.status == errors[n].status);
      assert(result.count == cases[i].fail_after);
      assert(result.error_offset == 0u);
      assert(result.native_error.domain == errors[n].native.domain);
      assert(result.native_error.code == errors[n].native.code);
      assert(sink.len == result.count);
      for (usize j = 0u; j < sink.len; ++j) {
        assert(sink.data[j] == decoded[j % 3u]);
      }
    }
  }
}

static void test_writer_non_native_results(void) {
  br_error error = br_error_make_native(BR_STATUS_IO_ERROR, BR_ERROR_DOMAIN_POSIX_ERRNO, 5u);
  test_base64_writer sink = test_base64_writer_make(0u, 0u, error);
  br_writer writer = br_stream_make(&sink, test_base64_writer_proc);
  br_io_result encoded;
  br_decode_into_result decoded;
  u8 raw[386];
  u8 src[516];

  encoded = br_base64_encode_to_writer(br_base64_std(), br_bytes_view_make(NULL, 0u), writer);
  decoded = br_base64_decode_to_writer(br_base64_std(), br_bytes_view_make(NULL, 0u), writer);
  assert(encoded.status == BR_STATUS_OK && encoded.count == 0u);
  assert(decoded.status == BR_STATUS_OK && decoded.count == 0u && decoded.error_offset == 0u);
  test_base64_assert_no_native(encoded.native_error);
  test_base64_assert_no_native(decoded.native_error);
  assert(sink.calls == 0u);

  sink = test_base64_writer_make(2u, SIZE_MAX, BR_ERROR_OK);
  encoded = br_base64_encode_to_writer(br_base64_std(), bv("foobar"), writer);
  assert(encoded.status == BR_STATUS_OK && encoded.count == 8u);
  test_base64_assert_no_native(encoded.native_error);
  assert(sink.calls == 4u && memcmp(sink.data, "Zm9vYmFy", 8u) == 0);

  sink = test_base64_writer_make(2u, SIZE_MAX, BR_ERROR_OK);
  decoded = br_base64_decode_to_writer(br_base64_std(), bv("Zm9vYmFy"), writer);
  assert(decoded.status == BR_STATUS_OK && decoded.count == 6u && decoded.error_offset == 0u);
  test_base64_assert_no_native(decoded.native_error);
  assert(sink.calls == 3u && memcmp(sink.data, "foobar", 6u) == 0);

  /* A successful zero-count callback becomes NO_PROGRESS, including after a flush. */
  memset(raw, 'A', sizeof(raw));
  for (usize i = 0u; i < sizeof(src); i += 4u) {
    memcpy(src + i, "QUJD", 4u);
  }
  for (usize i = 0u; i < 2u; ++i) {
    usize encode_limit = i == 0u ? 0u : 512u;
    usize decode_limit = i == 0u ? 0u : 384u;

    sink = test_base64_writer_make(7u, encode_limit, BR_ERROR_OK);
    encoded =
      br_base64_encode_to_writer(br_base64_std(), br_bytes_view_make(raw, sizeof(raw)), writer);
    assert(encoded.status == BR_STATUS_NO_PROGRESS && encoded.count == encode_limit);
    test_base64_assert_no_native(encoded.native_error);

    sink = test_base64_writer_make(7u, decode_limit, BR_ERROR_OK);
    decoded =
      br_base64_decode_to_writer(br_base64_std(), br_bytes_view_make(src, sizeof(src)), writer);
    assert(decoded.status == BR_STATUS_NO_PROGRESS && decoded.count == decode_limit);
    assert(decoded.error_offset == 0u);
    test_base64_assert_no_native(decoded.native_error);
  }

  writer = br_stream_make(NULL, NULL);
  encoded = br_base64_encode_to_writer(br_base64_std(), bv("f"), writer);
  decoded = br_base64_decode_to_writer(br_base64_std(), bv("Zg=="), writer);
  assert(encoded.status == BR_STATUS_NOT_SUPPORTED && encoded.count == 0u);
  assert(decoded.status == BR_STATUS_NOT_SUPPORTED && decoded.count == 0u);
  assert(decoded.error_offset == 0u);
  test_base64_assert_no_native(encoded.native_error);
  test_base64_assert_no_native(decoded.native_error);
}

static void test_decode_parser_errors_have_no_native_error(void) {
  static const struct {
    const char *src;
    bool padded;
    usize count;
    usize offset;
  } cases[] = {
    {"QUJD!A==", true, 0u, 4u},
    {"Zg=", true, 0u, 2u},
    {"AA=A", true, 0u, 2u},
    {"Zg==", false, 0u, 2u},
    {"QUJDZh==", true, 3u, 5u},
    {"QUJDZm9=", true, 3u, 6u},
  };
  br_error error = br_error_make_native(BR_STATUS_IO_ERROR, BR_ERROR_DOMAIN_WIN32, 112u);
  u8 dst[8];
  u8 late[514];
  br_base64_encoding raw_strict = br_base64_raw_std();
  test_base64_writer sink;
  br_decode_into_result result;

  raw_strict.strict = true;
  for (usize i = 0u; i < BR_ARRAY_COUNT(cases); ++i) {
    br_base64_encoding enc = br_base64_std();
    br_decode_into_result buffer;
    br_decode_into_result writer;

    enc.padded = cases[i].padded;
    enc.strict = true;
    memset(dst, 0xa5, sizeof(dst));
    sink = test_base64_writer_make(SIZE_MAX, SIZE_MAX, error);
    buffer = br_base64_decode_into(enc, bv(cases[i].src), dst, sizeof(dst));
    writer = br_base64_decode_to_writer(
      enc, bv(cases[i].src), br_stream_make(&sink, test_base64_writer_proc));
    assert(buffer.status == BR_STATUS_INVALID_ENCODING && buffer.count == cases[i].count);
    assert(buffer.error_offset == cases[i].offset);
    assert(writer.status == BR_STATUS_INVALID_ENCODING && writer.count == 0u);
    assert(writer.error_offset == cases[i].offset);
    test_base64_assert_no_native(buffer.native_error);
    test_base64_assert_no_native(writer.native_error);
    assert(dst[cases[i].count] == 0xa5u);
    assert(sink.calls == 0u);
  }

  /* A late canonicality error retains already-flushed progress and its input offset. */
  for (usize i = 0u; i < 512u; i += 4u) {
    memcpy(late + i, "QUJD", 4u);
  }
  memcpy(late + 512u, "Zk", 2u);
  sink = test_base64_writer_make(7u, SIZE_MAX, error);
  result = br_base64_decode_to_writer(raw_strict,
                                      br_bytes_view_make(late, sizeof(late)),
                                      br_stream_make(&sink, test_base64_writer_proc));
  assert(result.status == BR_STATUS_INVALID_ENCODING && result.count == 384u);
  assert(result.error_offset == 513u && sink.len == 384u);
  test_base64_assert_no_native(result.native_error);

  result = br_base64_decode_into(br_base64_std(), bv("Zg=="), NULL, 1u);
  assert(result.status == BR_STATUS_SHORT_BUFFER && result.count == 0u &&
         result.error_offset == 0u);
  test_base64_assert_no_native(result.native_error);
}

static void test_encoded_length_overflow(void) {
  usize groups_overflow = SIZE_MAX / 4u + 1u;
  usize raw_exact_max_len = (SIZE_MAX / 4u) * 3u + 2u;
  usize raw_overflow_len = groups_overflow * 3u;
  usize padded_overflow_len = (SIZE_MAX / 4u) * 3u + 1u;
  br_bytes_view impossible;
  u8 dst = 0u;
  br_io_result result;

  assert(br_base64_encoded_len(br_base64_std(), padded_overflow_len) == SIZE_MAX);
  assert(br_base64_encoded_len(br_base64_raw_std(), raw_exact_max_len) == SIZE_MAX);
  assert(br_base64_encoded_len(br_base64_raw_std(), raw_overflow_len) == SIZE_MAX);

  impossible = br_bytes_view_make(&dst, padded_overflow_len);
  result = br_base64_encode_into(br_base64_std(), impossible, &dst, 1u);
  assert(result.status == BR_STATUS_SHORT_BUFFER);
  assert(result.count == 0u);

  result = br_base64_encode_to_writer(br_base64_std(), impossible, br_stream_make(NULL, NULL));
  assert(result.status == BR_STATUS_OUT_OF_RANGE);
  assert(result.count == 0u);
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
  test_error_reports_partial_output();
  test_decode_to_writer_standard();
  test_decode_to_writer_raw_url();
  test_decode_to_writer_malformed_input();
  test_decode_to_writer_short_write();
  test_encode_to_writer_native_error();
  test_decode_to_writer_native_error();
  test_writer_non_native_results();
  test_decode_parser_errors_have_no_native_error();
  test_short_buffer();
  test_encoded_length_overflow();
  test_allocating_and_free_on_error();
  return 0;
}
