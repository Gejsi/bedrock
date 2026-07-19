#include <assert.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bedrock.h>

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
  br_io_result r;

  /* never-truncate: undersized dst -> SHORT_BUFFER, count 0. */
  r = br_format_i64(12345, 10, buf, 3);
  assert(r.status == BR_STATUS_SHORT_BUFFER && r.count == 0u);
  r = br_format_i64(12345, 10, NULL, 0);
  assert(r.status == BR_STATUS_SHORT_BUFFER && r.count == 0u);

  /* negative prec in a non-shortest mode -> INVALID_ARGUMENT. */
  r = br_format_f64(1.5, BR_FLOAT_DECIMAL, -1, buf, sizeof(buf));
  assert(r.status == BR_STATUS_INVALID_ARGUMENT);
  /* SHORTEST ignores prec (even a negative one is fine). */
  r = br_format_f64(1.5, BR_FLOAT_SHORTEST, -1, buf, sizeof(buf));
  assert(r.status == BR_STATUS_OK);

  /* enormous prec must not crash/overflow; a small dst just gets SHORT_BUFFER. */
  assert(br_format_f64_bound(BR_FLOAT_DECIMAL, INT32_MAX) < (size_t)-1 / 2u);
  r = br_format_f64(1.0, BR_FLOAT_DECIMAL, 1000000, buf, sizeof(buf));
  assert(r.status == BR_STATUS_SHORT_BUFFER && r.count == 0u);

  /* bad base -> INVALID_ARGUMENT. */
  r = br_format_i64(1, 37, buf, sizeof(buf));
  assert(r.status == BR_STATUS_INVALID_ARGUMENT);
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
    long mant;
    int bexp;
    int prec;
    uint8_t buf[64];
    br_io_result fr;

    if (line[0] == '#' || line[0] == '\n') {
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
    /* input is "<mant>p<exp>". */
    if (sscanf(input, "%ldp%d", &mant, &bexp) != 2) {
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

int main(void) {
  test_f32_double_rounding_witnesses();
  test_int_parse_basic();
  test_int_overflow_saturates();
  test_int_bases();
  test_strict_vs_prefix();
  test_i32_u32_narrowing();
  test_bool();
  test_float_specials();
  test_format_buffer_and_prec();
  test_locale_independence();
  test_atof1k_roundtrip();
  test_ftoa1k_roundtrip();
  test_testfp_exponent();
  return 0;
}
