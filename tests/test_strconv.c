#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bedrock.h>

#include "data/strconv_decimal_cases.h"

#ifndef BR_TEST_DATA_DIR
#error "BR_TEST_DATA_DIR must be defined by the build"
#endif

static br_string_view sv(const char *s) {
  return br_string_view_from_cstr(s);
}

static u32 f32_bits(float f) {
  u32 b;
  memcpy(&b, &f, sizeof(b));
  return b;
}

static u64 f64_bits(double f) {
  u64 b;
  memcpy(&b, &f, sizeof(b));
  return b;
}

/* ---- the verified f32 double-rounding witnesses (the sharpest gate) ---- */

static void test_f32_double_rounding_witnesses(void) {
  br_parse_f32_result a = br_parse_f32(sv("1.00000017881393432617187499"));
  br_parse_f32_result b = br_parse_f32(sv("1.0000000596046448"));

  assert(a.status == BR_STATUS_OK);
  /* Native f32 rounds to 0x3f800001; parse-as-f64-then-narrow wrongly gives 0x3f800002. */
  assert(f32_bits(a.value) == 0x3f800001u);

  assert(b.status == BR_STATUS_OK);
  /* Correctly-rounded f32 for this input, per strtof as the oracle. */
  assert(f32_bits(b.value) == f32_bits(strtof("1.0000000596046448", NULL)));
}

static void check_compensated_float(char *text, bool neg) {
  size_t len = strlen(text);
  u64 expected64 = neg ? 0xbff0000000000000uLL : 0x3ff0000000000000uLL;
  u32 expected32 = neg ? 0xbf800000u : 0x3f800000u;
  br_parse_f64_result r64 = br_parse_f64(sv(text));
  br_parse_f32_result r32 = br_parse_f32(sv(text));

  assert(r64.status == BR_STATUS_OK && r64.consumed == len);
  assert(r32.status == BR_STATUS_OK && r32.consumed == len);
  assert(f64_bits(r64.value) == expected64 && f32_bits(r32.value) == expected32);

  text[len] = 'x';
  text[len + 1u] = '\0';
  r64 = br_parse_f64_prefix(sv(text));
  r32 = br_parse_f32_prefix(sv(text));
  assert(r64.status == BR_STATUS_OK && r64.consumed == len);
  assert(r32.status == BR_STATUS_OK && r32.consumed == len);
  assert(f64_bits(r64.value) == expected64 && f32_bits(r32.value) == expected32);
  r64 = br_parse_f64(sv(text));
  r32 = br_parse_f32(sv(text));
  assert(r64.status == BR_STATUS_INVALID_ENCODING && r64.consumed == len);
  assert(r32.status == BR_STATUS_INVALID_ENCODING && r32.consumed == len);
  text[len] = '\0';
}

static void test_float_long_place_value(void) {
  static const size_t zero_counts[] = {384u, 800u, 5000u, 100000u};
  size_t i;

  for (i = 0u; i < sizeof(zero_counts) / sizeof(zero_counts[0]); i += 1u) {
    size_t zeros = zero_counts[i];
    size_t cap = zeros + 32u;
    char *text = (char *)malloc(cap);
    unsigned sign;

    assert(text != NULL);
    for (sign = 0u; sign < 2u; sign += 1u) {
      bool neg = sign != 0u;
      size_t start = neg ? 1u : 0u;
      size_t end;
      int n;

      text[0] = '-';
      text[start] = '1';
      memset(text + start + 1u, '0', zeros);
      end = start + 1u + zeros;
      n = snprintf(text + end, cap - end, "e-%zu", zeros);
      assert(n > 0 && (size_t)n < cap - end);
      check_compensated_float(text, neg);

      /* The same long integer with a fractional part must keep its position. */
      n = snprintf(text + end, cap - end, ".0e-%zu", zeros);
      assert(n > 0 && (size_t)n < cap - end);
      check_compensated_float(text, neg);

      /* Leading integer zeroes do not contribute significant places. */
      memcpy(text + start, "0001", 4u);
      memset(text + start + 4u, '0', zeros);
      end = start + 4u + zeros;
      n = snprintf(text + end, cap - end, "e-%zu", zeros);
      assert(n > 0 && (size_t)n < cap - end);
      check_compensated_float(text, neg);

      /* Long leading fractional zeroes cancel against a positive exponent. */
      memcpy(text + start, "0.", 2u);
      memset(text + start + 2u, '0', zeros);
      text[start + 2u + zeros] = '1';
      end = start + 3u + zeros;
      n = snprintf(text + end, cap - end, "e+%zu", zeros + 1u);
      assert(n > 0 && (size_t)n < cap - end);
      check_compensated_float(text, neg);
    }
    free(text);
  }
}

static void test_float_extreme_exponents(void) {
  const char *large = "1e999999999999999999999999999999999999";
  const char *small = "-1e-999999999999999999999999999999999999";
  br_parse_f64_result r64 = br_parse_f64(sv(large));
  br_parse_f32_result r32 = br_parse_f32(sv(large));

  assert(r64.status == BR_STATUS_OUT_OF_RANGE && r64.consumed == strlen(large));
  assert(r32.status == BR_STATUS_OUT_OF_RANGE && r32.consumed == strlen(large));
  assert(f64_bits(r64.value) == 0x7ff0000000000000uLL);
  assert(f32_bits(r32.value) == 0x7f800000u);
  r64 = br_parse_f64(sv(small));
  r32 = br_parse_f32(sv(small));
  assert(r64.status == BR_STATUS_OK && r64.consumed == strlen(small));
  assert(r32.status == BR_STATUS_OK && r32.consumed == strlen(small));
  assert(f64_bits(r64.value) == 0x8000000000000000uLL);
  assert(f32_bits(r32.value) == 0x80000000u);
  assert(br_parse_f64(sv("0e999999999999999999999999999999999999")).value == 0.0);
  assert(br_parse_f32(sv("-0e999999999999999999999999999999999999")).value == 0.0f);

  /* An incomplete exponent remains trailing syntax, with the numeric prefix intact. */
  r64 = br_parse_f64_prefix(sv("1e+"));
  r32 = br_parse_f32_prefix(sv("1e-"));
  assert(r64.status == BR_STATUS_OK && r64.value == 1.0 && r64.consumed == 1u);
  assert(r32.status == BR_STATUS_OK && r32.value == 1.0f && r32.consumed == 1u);
}

/* ---- integers ---- */

static void test_int_parse_basic(void) {
  br_parse_i64_result i;
  br_parse_u64_result u;

  i = br_parse_i64(sv("0"), 10);
  assert(i.status == BR_STATUS_OK && i.value == 0 && i.consumed == 1u);

  i = br_parse_i64(sv("-9223372036854775808"), 10);
  assert(i.status == BR_STATUS_OK && i.value == INT64_MIN);

  i = br_parse_i64(sv("9223372036854775807"), 10);
  assert(i.status == BR_STATUS_OK && i.value == INT64_MAX);

  u = br_parse_u64(sv("18446744073709551615"), 10);
  assert(u.status == BR_STATUS_OK && u.value == UINT64_MAX);

  /* '+' sign accepted for signed; rejected as a digit for unsigned only via '-'. */
  i = br_parse_i64(sv("+42"), 10);
  assert(i.status == BR_STATUS_OK && i.value == 42);

  u = br_parse_u64(sv("-1"), 10);
  assert(u.status == BR_STATUS_INVALID_ENCODING);
}

static void test_int_overflow_saturates(void) {
  br_parse_i64_result i = br_parse_i64(sv("99999999999999999999999"), 10);
  br_parse_u64_result u = br_parse_u64(sv("99999999999999999999999"), 10);
  br_parse_i64_result neg = br_parse_i64(sv("-99999999999999999999999"), 10);

  assert(i.status == BR_STATUS_OUT_OF_RANGE && i.value == INT64_MAX);
  assert(u.status == BR_STATUS_OUT_OF_RANGE && u.value == UINT64_MAX);
  assert(neg.status == BR_STATUS_OUT_OF_RANGE && neg.value == INT64_MIN);
}

static void test_int_bases(void) {
  int base;

  /* Round-trip every base 2..36 across a spread of values. */
  for (base = 2; base <= 36; ++base) {
    static const int64_t vals[] = {0, 1, -1, 255, -256, 1000000, -1000000, INT64_MAX, INT64_MIN};
    size_t k;

    for (k = 0u; k < sizeof(vals) / sizeof(vals[0]); ++k) {
      uint8_t buf[80];
      br_io_result fr = br_format_i64(vals[k], base, buf, sizeof(buf));
      br_parse_i64_result pr;

      assert(fr.status == BR_STATUS_OK);
      pr = br_parse_i64((br_string_view){(const char *)buf, fr.count}, base);
      assert(pr.status == BR_STATUS_OK && pr.value == vals[k]);
    }
  }

  /* base-0 inference. */
  assert(br_parse_i64(sv("0xff"), 0).value == 255);
  assert(br_parse_i64(sv("0o17"), 0).value == 15);
  assert(br_parse_i64(sv("0b101"), 0).value == 5);
  assert(br_parse_i64(sv("42"), 0).value == 42);
  /* 0d/0z are NOT recognized (dropped): "0d5" parses as 0 then trailing junk. */
  assert(br_parse_i64(sv("0d5"), 0).status == BR_STATUS_INVALID_ENCODING);

  /* bad base -> INVALID_ARGUMENT. */
  assert(br_parse_i64(sv("1"), 37).status == BR_STATUS_INVALID_ARGUMENT);
  assert(br_parse_i64(sv("1"), 1).status == BR_STATUS_INVALID_ARGUMENT);
}

static void test_int_format_max_bounds(void) {
  uint8_t signed_buf[BR_FORMAT_I64_MAX];
  uint8_t unsigned_buf[BR_FORMAT_U64_MAX];
  br_io_result r;

  r = br_format_i64(INT64_MIN, 2, signed_buf, sizeof(signed_buf));
  assert(r.status == BR_STATUS_OK && r.count == BR_FORMAT_I64_MAX);
  assert(signed_buf[0] == '-');
  assert(br_parse_i64((br_string_view){(const char *)signed_buf, r.count}, 2).value == INT64_MIN);

  r = br_format_u64(UINT64_MAX, 2, unsigned_buf, sizeof(unsigned_buf));
  assert(r.status == BR_STATUS_OK && r.count == BR_FORMAT_U64_MAX);
  assert(br_parse_u64((br_string_view){(const char *)unsigned_buf, r.count}, 2).value ==
         UINT64_MAX);
}

static void test_strict_vs_prefix(void) {
  assert(br_parse_i64(sv("12x"), 10).status == BR_STATUS_INVALID_ENCODING);
  {
    br_parse_i64_result p = br_parse_i64_prefix(sv("12x"), 10);
    assert(p.status == BR_STATUS_OK && p.value == 12 && p.consumed == 2u);
  }
  assert(br_parse_i64(sv(""), 10).status == BR_STATUS_INVALID_ENCODING);
  assert(br_parse_i64((br_string_view){NULL, 0u}, 10).status == BR_STATUS_INVALID_ENCODING);
  {
    br_parse_f64_result p = br_parse_f64_prefix(sv("3.14abc"));
    assert(p.status == BR_STATUS_OK && p.value == 3.14 && p.consumed == 4u);
  }
  assert(br_parse_f64(sv("3.14abc")).status == BR_STATUS_INVALID_ENCODING);
}

static void test_i32_u32_narrowing(void) {
  assert(br_parse_i32(sv("2147483647"), 10).status == BR_STATUS_OK);
  {
    br_parse_i32_result over = br_parse_i32(sv("2147483648"), 10);
    assert(over.status == BR_STATUS_OUT_OF_RANGE && over.value == INT32_MAX);
  }
  {
    br_parse_u32_result over = br_parse_u32(sv("4294967296"), 10);
    assert(over.status == BR_STATUS_OUT_OF_RANGE && over.value == UINT32_MAX);
  }
}

static void test_bool(void) {
  uint8_t buf[8];
  br_io_result r;

  assert(br_parse_bool(sv("true")).value == true);
  assert(br_parse_bool(sv("t")).value == true);
  assert(br_parse_bool(sv("1")).value == true);
  assert(br_parse_bool(sv("false")).value == false &&
         br_parse_bool(sv("false")).status == BR_STATUS_OK);
  assert(br_parse_bool(sv("0")).status == BR_STATUS_OK);
  assert(br_parse_bool(sv("yes")).status == BR_STATUS_INVALID_ENCODING);
  assert(br_parse_bool(sv("TRUE")).status == BR_STATUS_INVALID_ENCODING); /* case-sensitive */

  r = br_format_bool(true, buf, sizeof(buf));
  assert(r.status == BR_STATUS_OK && r.count == 4u && memcmp(buf, "true", 4) == 0);
  r = br_format_bool(false, buf, sizeof(buf));
  assert(r.status == BR_STATUS_OK && r.count == 5u && memcmp(buf, "false", 5) == 0);
}

static void test_float_specials(void) {
  uint8_t buf[8];
  br_io_result r;

  assert(isinf(br_parse_f64(sv("inf")).value));
  assert(isinf(br_parse_f64(sv("Infinity")).value));
  assert(br_parse_f64(sv("-INF")).value < 0.0);
  assert(isnan(br_parse_f64(sv("nan")).value));
  assert(isnan(br_parse_f64(sv("NaN")).value));

  r = br_format_f64((double)INFINITY, BR_FLOAT_SHORTEST, 0, buf, sizeof(buf));
  assert(r.status == BR_STATUS_OK && memcmp(buf, "+Inf", 4) == 0);
  r = br_format_f64(-(double)INFINITY, BR_FLOAT_SHORTEST, 0, buf, sizeof(buf));
  assert(r.status == BR_STATUS_OK && memcmp(buf, "-Inf", 4) == 0);
  r = br_format_f64((double)NAN, BR_FLOAT_SHORTEST, 0, buf, sizeof(buf));
  assert(r.status == BR_STATUS_OK && memcmp(buf, "NaN", 3) == 0);
}

static void test_format_buffer_and_prec(void) {
  uint8_t buf[512];
  uint8_t sentinel[8];
  br_io_result r;
  size_t bound;
  size_t bound32;
  uint8_t *large;

  /* never-truncate: undersized dst -> SHORT_BUFFER, count 0. */
  r = br_format_i64(12345, 10, buf, 3);
  assert(r.status == BR_STATUS_SHORT_BUFFER && r.count == 0u);
  r = br_format_i64(12345, 10, NULL, 0);
  assert(r.status == BR_STATUS_SHORT_BUFFER && r.count == 0u);

  memset(sentinel, 0xa5, sizeof(sentinel));
  r = br_format_f64(123.5, BR_FLOAT_DECIMAL, 3, sentinel, 3u);
  assert(r.status == BR_STATUS_SHORT_BUFFER && r.count == 0u);
  for (size_t i = 0u; i < sizeof(sentinel); i += 1u) {
    assert(sentinel[i] == 0xa5u);
  }

  memset(sentinel, 0x5a, sizeof(sentinel));
  r = br_format_f32(123.5f, BR_FLOAT_EXPONENT, 3, sentinel, 3u);
  assert(r.status == BR_STATUS_SHORT_BUFFER && r.count == 0u);
  for (size_t i = 0u; i < sizeof(sentinel); i += 1u) {
    assert(sentinel[i] == 0x5au);
  }

  /* negative prec in a non-shortest mode -> INVALID_ARGUMENT. */
  r = br_format_f64(1.5, BR_FLOAT_DECIMAL, -1, buf, sizeof(buf));
  assert(r.status == BR_STATUS_INVALID_ARGUMENT);
  /* SHORTEST ignores prec (even a negative one is fine). */
  r = br_format_f64(1.5, BR_FLOAT_SHORTEST, -1, buf, sizeof(buf));
  assert(r.status == BR_STATUS_OK);

  /* The largest accepted precision terminates, fits its bound, and remains
     atomic when the destination is short. */
  bound = br_format_f64_bound(BR_FLOAT_DECIMAL, BR_FORMAT_FLOAT_PRECISION_MAX);
  large = (uint8_t *)malloc(bound);
  assert(large != NULL);
  r = br_format_f64(1.0, BR_FLOAT_DECIMAL, BR_FORMAT_FLOAT_PRECISION_MAX, large, bound);
  assert(r.status == BR_STATUS_OK && r.count <= bound);
  assert(r.count == (size_t)BR_FORMAT_FLOAT_PRECISION_MAX + 2u);
  free(large);

  bound32 = br_format_f32_bound(BR_FLOAT_EXPONENT, BR_FORMAT_FLOAT_PRECISION_MAX);
  large = (uint8_t *)malloc(bound32);
  assert(large != NULL);
  r = br_format_f32(1.0f, BR_FLOAT_EXPONENT, BR_FORMAT_FLOAT_PRECISION_MAX, large, bound32);
  assert(r.status == BR_STATUS_OK && r.count <= bound32);
  assert(r.count == (size_t)BR_FORMAT_FLOAT_PRECISION_MAX + 6u);
  free(large);

  memset(sentinel, 0x3c, sizeof(sentinel));
  r =
    br_format_f64(1.0, BR_FLOAT_DECIMAL, BR_FORMAT_FLOAT_PRECISION_MAX, sentinel, sizeof(sentinel));
  assert(r.status == BR_STATUS_SHORT_BUFFER && r.count == 0u);
  for (size_t i = 0u; i < sizeof(sentinel); i += 1u) {
    assert(sentinel[i] == 0x3cu);
  }

  assert(br_format_f64_bound(BR_FLOAT_DECIMAL, INT_MAX) == bound);
  r = br_format_f64(1.0, BR_FLOAT_DECIMAL, INT_MAX, sentinel, sizeof(sentinel));
  assert(r.status == BR_STATUS_INVALID_ARGUMENT && r.count == 0u);
  assert(br_format_f32_bound(BR_FLOAT_EXPONENT, INT_MAX) == bound32);
  r = br_format_f32(1.0f, BR_FLOAT_EXPONENT, INT_MAX, sentinel, sizeof(sentinel));
  assert(r.status == BR_STATUS_INVALID_ARGUMENT && r.count == 0u);
  for (size_t i = 0u; i < sizeof(sentinel); i += 1u) {
    assert(sentinel[i] == 0x3cu);
  }

  r = br_format_f64(8.0, BR_FLOAT_GENERAL, 0, buf, sizeof(buf));
  assert(r.status == BR_STATUS_OK && r.count == 1u && buf[0] == '8');
  r = br_format_f32(8.0f, BR_FLOAT_GENERAL, 0, buf, sizeof(buf));
  assert(r.status == BR_STATUS_OK && r.count == 1u && buf[0] == '8');

  /* bad base -> INVALID_ARGUMENT. */
  r = br_format_i64(1, 37, buf, sizeof(buf));
  assert(r.status == BR_STATUS_INVALID_ARGUMENT);
}

static void check_formatter_native_error(br_io_result result) {
  assert(result.native_error.domain == BR_ERROR_DOMAIN_NONE);
  assert(result.native_error.code == 0u);
}

static void test_formatter_native_errors(void) {
  uint8_t buf[32];

  check_formatter_native_error(br_format_i64(-42, 10, buf, sizeof(buf)));
  check_formatter_native_error(br_format_i64(-42, 10, buf, 1u));
  check_formatter_native_error(br_format_i64(-42, 1, buf, sizeof(buf)));
  check_formatter_native_error(br_format_u64(42u, 10, buf, sizeof(buf)));
  check_formatter_native_error(br_format_u64(42u, 10, NULL, 0u));
  check_formatter_native_error(br_format_u64(42u, 37, buf, sizeof(buf)));
  check_formatter_native_error(br_format_bool(true, buf, sizeof(buf)));
  check_formatter_native_error(br_format_bool(false, buf, 1u));
  check_formatter_native_error(br_format_f64(1.5, BR_FLOAT_SHORTEST, 0, buf, sizeof(buf)));
  check_formatter_native_error(br_format_f64(1.5, BR_FLOAT_DECIMAL, 2, buf, 1u));
  check_formatter_native_error(br_format_f64(1.5, BR_FLOAT_DECIMAL, -1, buf, sizeof(buf)));
  check_formatter_native_error(br_format_f32(1.5f, BR_FLOAT_SHORTEST, 0, buf, sizeof(buf)));
  check_formatter_native_error(br_format_f32(1.5f, BR_FLOAT_EXPONENT, 2, NULL, 0u));
  check_formatter_native_error(br_format_f32(1.5f, BR_FLOAT_GENERAL, INT_MAX, buf, sizeof(buf)));
  check_formatter_native_error(br_format_f64((double)NAN, BR_FLOAT_SHORTEST, 0, buf, sizeof(buf)));
  check_formatter_native_error(
    br_format_f32((float)INFINITY, BR_FLOAT_SHORTEST, 0, buf, sizeof(buf)));
}

static void test_f32_shortest_bounds(void) {
  static const u32 patterns[] = {0x61b50acbu, 0xe1b50acbu, 0x5ce3d86au, 0xdce3d86au};
  size_t maximum = 0u;

  for (size_t i = 0u; i < sizeof(patterns) / sizeof(patterns[0]); i += 1u) {
    u8 buf[BR_FORMAT_F32_SHORTEST_MAX];
    float value;
    br_io_result formatted;
    br_string_builder builder;
    br_string_builder_io_result appended;
    br_string_view view;

    memcpy(&value, &patterns[i], sizeof(value));
    assert(br_format_f32_bound(BR_FLOAT_SHORTEST, INT_MIN) == sizeof(buf));
    formatted = br_format_f32(value, BR_FLOAT_SHORTEST, INT_MIN, buf, sizeof(buf));
    assert(formatted.status == BR_STATUS_OK && formatted.count <= sizeof(buf));
    assert(formatted.native_error.domain == BR_ERROR_DOMAIN_NONE &&
           formatted.native_error.code == 0u);
    assert(f32_bits(br_parse_f32((br_string_view){(const char *)buf, formatted.count}).value) ==
           patterns[i]);
    if (formatted.count > maximum) {
      maximum = formatted.count;
    }

    /* The builder reserves the bound rather than a guessed oversized buffer. */
    br_string_builder_init(&builder, br_allocator_heap());
    appended = br_string_builder_write_f32(&builder, value, BR_FLOAT_SHORTEST, INT_MIN);
    assert(appended.status == BR_STATUS_OK && appended.count == formatted.count);
    view = br_string_builder_view(&builder);
    assert(view.len == formatted.count && memcmp(view.data, buf, view.len) == 0);
    br_string_builder_destroy(&builder);
  }
  assert(maximum == BR_FORMAT_F32_SHORTEST_MAX);
}

static void test_locale_independence(void) {
  uint8_t buf[32];
  br_io_result r;
  br_parse_f64_result p;

  /* Try to install a comma-radix locale; skip cleanly if unavailable. */
  if (setlocale(LC_NUMERIC, "de_DE.UTF-8") == NULL && setlocale(LC_NUMERIC, "de_DE") == NULL) {
    setlocale(LC_NUMERIC, "C");
    return;
  }

  p = br_parse_f64(sv("3.14"));
  assert(p.status == BR_STATUS_OK && p.value == 3.14);

  r = br_format_f64(3.14, BR_FLOAT_DECIMAL, 2, buf, sizeof(buf));
  assert(r.status == BR_STATUS_OK && r.count == 4u && memcmp(buf, "3.14", 4) == 0);

  setlocale(LC_NUMERIC, "C");
}

/* ---- Paxson harness helpers ---- */

static FILE *open_data(const char *name) {
  char path[1024];
  int n = snprintf(path, sizeof(path), "%s/%s", BR_TEST_DATA_DIR, name);

  assert(n > 0 && (size_t)n < sizeof(path));
  return fopen(path, "r");
}

/*
Round-trip every decimal in atof1k.txt: parse to f64, format shortest, re-parse,
and require the two parses agree bit-for-bit (a shortest formatter must reproduce
exactly the value it was given).
*/
static void test_atof1k_roundtrip(void) {
  FILE *f = open_data("atof1k.txt");
  char line[256];
  int count = 0;

  assert(f != NULL);
  while (fgets(line, sizeof(line), f) != NULL) {
    size_t len = strlen(line);
    br_parse_f64_result p1;
    uint8_t buf[64];
    br_io_result fr;
    br_parse_f64_result p2;

    while (len > 0u && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
      line[--len] = '\0';
    }
    if (len == 0u || line[0] == '#') {
      continue;
    }
    p1 = br_parse_f64((br_string_view){line, len});
    assert(p1.status == BR_STATUS_OK);
    fr = br_format_f64(p1.value, BR_FLOAT_SHORTEST, 0, buf, sizeof(buf));
    assert(fr.status == BR_STATUS_OK);
    p2 = br_parse_f64((br_string_view){(const char *)buf, fr.count});
    assert(p2.status == BR_STATUS_OK);
    assert(f64_bits(p1.value) == f64_bits(p2.value));
    count += 1;
  }
  fclose(f);
  assert(count > 0);
  printf("  atof1k: %d round-trips ok\n", count);
}

/*
ftoa1k.txt holds one C99 hexfloat (%a, exact bits) per line. strtod parses the
exact value (libm as oracle); confirm br_parse_f64 of a shortest-formatted
rendering round-trips to the same bits.
*/
static void test_ftoa1k_roundtrip(void) {
  FILE *f = open_data("ftoa1k.txt");
  char line[256];
  int count = 0;

  assert(f != NULL);
  while (fgets(line, sizeof(line), f) != NULL) {
    size_t len = strlen(line);
    double exact;
    uint8_t buf[64];
    br_io_result fr;
    br_parse_f64_result p;

    while (len > 0u && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
      line[--len] = '\0';
    }
    if (len == 0u || line[0] == '#') {
      continue;
    }
    exact = strtod(line, NULL); /* oracle: exact hexfloat -> double */
    fr = br_format_f64(exact, BR_FLOAT_SHORTEST, 0, buf, sizeof(buf));
    assert(fr.status == BR_STATUS_OK);
    p = br_parse_f64((br_string_view){(const char *)buf, fr.count});
    assert(p.status == BR_STATUS_OK);
    assert(f64_bits(p.value) == f64_bits(exact));
    count += 1;
  }
  fclose(f);
  assert(count > 0);
  printf("  ftoa1k: %d round-trips ok\n", count);
}

/*
testfp.txt has 4 fields/line: type, verb, input, expected. The input is itself
in Go's `%b` mantissa-p-exponent form (e.g. "8511030020275656p-342"); build the
exact value with ldexp (libm oracle), then for the `%.Ne` lines assert
br_format_*(EXPONENT, N) == expected. The `%b`-verb output lines are skipped
(Bedrock does not implement Go's binary-float verb) with a counted log line.
*/
static void test_testfp_exponent(void) {
  FILE *f = open_data("testfp.txt");
  char line[256];
  int driven = 0;
  int skipped_b = 0;

  assert(f != NULL);
  while (fgets(line, sizeof(line), f) != NULL) {
    char type[16];
    char verb[16];
    char input[128];
    char expected[128];
    int64_t mant;
    int bexp;
    int prec;
    uint8_t buf[64];
    br_io_result fr;
    size_t len = strlen(line);

    /* Strip trailing CR/LF so a CRLF checkout never leaves a phantom byte. */
    while (len > 0u && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
      line[--len] = '\0';
    }
    if (line[0] == '#' || line[0] == '\0') {
      continue;
    }
    if (sscanf(line, "%15s %15s %127s %127s", type, verb, input, expected) != 4) {
      continue;
    }
    if (strcmp(verb, "%b") == 0) {
      skipped_b += 1;
      continue;
    }
    /* verb is "%.Ne": pull N. */
    if (sscanf(verb, "%%.%de", &prec) != 1) {
      continue;
    }
    /*
    input is "<mant>p<exp>". The mantissa is a full 53-bit significand (up to 16
    decimal digits), so it must be read into a 64-bit integer -- a platform
    `long` is only 32-bit on Windows (LLP64) and would truncate it.
    */
    if (sscanf(input, "%" SCNd64 "p%d", &mant, &bexp) != 2) {
      continue;
    }

    if (strcmp(type, "float64") == 0) {
      double v = ldexp((double)mant, bexp);
      fr = br_format_f64(v, BR_FLOAT_EXPONENT, prec, buf, sizeof(buf));
    } else {
      float v = ldexpf((float)mant, bexp);
      fr = br_format_f32(v, BR_FLOAT_EXPONENT, prec, buf, sizeof(buf));
    }
    assert(fr.status == BR_STATUS_OK);
    assert(fr.count == strlen(expected));
    assert(memcmp(buf, expected, fr.count) == 0);
    driven += 1;
  }
  fclose(f);
  assert(driven > 0);
  printf("  testfp: %d %%.Ne cases driven, %d %%b lines skipped\n", driven, skipped_b);
}

static void test_exact_decimal_parsing(void) {
  size_t i;

  for (i = 0u; i < sizeof(br_test_decimal_parse_cases) / sizeof(br_test_decimal_parse_cases[0]);
       i += 1u) {
    const br_test_decimal_parse_case *test = &br_test_decimal_parse_cases[i];
    size_t len = strlen(test->input);
    char *text = (char *)malloc(len + 2u);
    size_t consumed;
    br_status status;
    u64 bits;
    unsigned pass;

    assert(text != NULL);
    assert(test->width == 32u || test->width == 64u);
    assert(test->status == BR_STATUS_OK || test->status == BR_STATUS_OUT_OF_RANGE);
    memcpy(text, test->input, len + 1u);
    for (pass = 0u; pass < 2u; pass += 1u) {
      if (test->width == 64u) {
        br_parse_f64_result r = pass == 0u ? br_parse_f64(sv(text)) : br_parse_f64_prefix(sv(text));
        bits = f64_bits(r.value);
        consumed = r.consumed;
        status = r.status;
      } else {
        br_parse_f32_result r = pass == 0u ? br_parse_f32(sv(text)) : br_parse_f32_prefix(sv(text));
        bits = f32_bits(r.value);
        consumed = r.consumed;
        status = r.status;
      }
      if (status != test->status || consumed != len || bits != test->bits) {
        fprintf(stderr,
                "exact decimal parse %zu f%u pass %u: bits %016" PRIx64 ", expected %016" PRIx64
                "\n",
                i,
                test->width,
                pass,
                bits,
                test->bits);
      }
      assert(status == test->status && consumed == len && bits == test->bits);
      text[len] = 'x';
      text[len + 1u] = '\0';
    }
    assert(br_parse_f64(sv(text)).status == BR_STATUS_INVALID_ENCODING);
    assert(br_parse_f32(sv(text)).status == BR_STATUS_INVALID_ENCODING);
    free(text);
  }
}

static br_io_result
format_decimal_case(const br_test_decimal_format_case *test, u8 *dst, size_t cap) {
  if (test->width == 64u) {
    double value;
    memcpy(&value, &test->bits, sizeof(value));
    return br_format_f64(value, test->format, test->precision, dst, cap);
  } else {
    u32 bits = (u32)test->bits;
    float value;
    assert(test->width == 32u);
    memcpy(&value, &bits, sizeof(value));
    return br_format_f32(value, test->format, test->precision, dst, cap);
  }
}

static void test_exact_decimal_formatting(void) {
  size_t i;

  for (i = 0u; i < sizeof(br_test_decimal_format_cases) / sizeof(br_test_decimal_format_cases[0]);
       i += 1u) {
    const br_test_decimal_format_case *test = &br_test_decimal_format_cases[i];
    size_t len = strlen(test->expected);
    size_t bound = test->width == 64u ? br_format_f64_bound(test->format, test->precision)
                                      : br_format_f32_bound(test->format, test->precision);
    u8 *buf = (u8 *)malloc(bound + 1u);
    br_io_result result;
    size_t j;

    assert(buf != NULL && len > 0u && len <= bound);
    memset(buf, 0xa5, bound + 1u);
    result = format_decimal_case(test, buf, len);
    check_formatter_native_error(result);
    if (result.status != BR_STATUS_OK || result.count != len ||
        memcmp(buf, test->expected, len) != 0) {
      fprintf(
        stderr, "exact decimal format %zu f%u precision %d\n", i, test->width, test->precision);
    }
    assert(result.status == BR_STATUS_OK && result.count == len);
    assert(memcmp(buf, test->expected, len) == 0 && buf[len] == 0xa5u);

    memset(buf, 0xa5, bound + 1u);
    result = format_decimal_case(test, buf, len - 1u);
    assert(result.status == BR_STATUS_SHORT_BUFFER && result.count == 0u);
    check_formatter_native_error(result);
    for (j = 0u; j <= bound; j += 1u) {
      assert(buf[j] == 0xa5u);
    }
    free(buf);
  }
}

static void test_float_max_precision_digits(void) {
  const br_test_decimal_format_case *test = NULL;
  size_t i;
  const char *exponent;
  size_t prefix;
  size_t exponent_pos = (size_t)BR_FORMAT_FLOAT_PRECISION_MAX + 2u;
  size_t count;
  u8 *buf;
  double value;
  br_io_result result;

  for (i = 0u; i < sizeof(br_test_decimal_format_cases) / sizeof(br_test_decimal_format_cases[0]);
       i += 1u) {
    const br_test_decimal_format_case *candidate = &br_test_decimal_format_cases[i];
    if (candidate->width == 64u && candidate->bits == 1u &&
        candidate->format == BR_FLOAT_EXPONENT && candidate->precision >= 800) {
      test = candidate;
      break;
    }
  }
  assert(test != NULL);
  exponent = strchr(test->expected, 'e');
  assert(exponent != NULL);
  prefix = (size_t)(exponent - test->expected);
  count = exponent_pos + strlen(exponent);
  buf = (u8 *)malloc(count);
  assert(buf != NULL);
  memcpy(&value, &test->bits, sizeof(value));
  result = br_format_f64(value, BR_FLOAT_EXPONENT, BR_FORMAT_FLOAT_PRECISION_MAX, buf, count);
  assert(result.status == BR_STATUS_OK && result.count == count);
  check_formatter_native_error(result);

  /* The fixture contains the entire exact expansion, not merely 1.0's zeroes. */
  assert(memcmp(buf, test->expected, prefix) == 0);
  for (i = prefix; i < exponent_pos; i += 1u) {
    assert(buf[i] == '0');
  }
  assert(memcmp(buf + exponent_pos, exponent, strlen(exponent)) == 0);
  free(buf);
}

int main(void) {
  test_f32_double_rounding_witnesses();
  test_float_long_place_value();
  test_float_extreme_exponents();
  test_int_parse_basic();
  test_int_overflow_saturates();
  test_int_bases();
  test_int_format_max_bounds();
  test_strict_vs_prefix();
  test_i32_u32_narrowing();
  test_bool();
  test_float_specials();
  test_format_buffer_and_prec();
  test_formatter_native_errors();
  test_f32_shortest_bounds();
  test_exact_decimal_parsing();
  test_exact_decimal_formatting();
  test_float_max_precision_digits();
  test_locale_independence();
  test_atof1k_roundtrip();
  test_ftoa1k_roundtrip();
  test_testfp_exponent();
  return 0;
}
