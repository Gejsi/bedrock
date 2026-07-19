#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <bedrock.h>

static br_string_view sv(const char *s) {
  return br_string_view_from_cstr(s);
}

/* Format helper: format into a fixed buffer, return the NUL-terminated text (or
   NULL on non-OK status, with the status out-param set). */
static br_status format_into(br_time t, int32_t off, char *out, usize out_cap) {
  br_io_result r = br_rfc3339_format(t, off, (u8 *)out, out_cap);
  if (r.status == BR_STATUS_OK) {
    out[r.count] = '\0';
  }
  return r.status;
}

static void test_parse_basic(void) {
  br_rfc3339_result r = br_rfc3339_parse(sv("1985-04-12T23:20:50Z"));
  assert(r.status == BR_STATUS_OK);
  assert(r.utc_offset_min == 0);
  assert(!r.is_leap_second);
  assert(r.consumed == 20u);

  /* The instant matches a component build of the same civil time. */
  {
    br_datetime dt = br_datetime_from_components(1985, 4, 12, 23, 20, 50, 0).value;
    br_time_result bridged = br_datetime_to_time(dt);
    assert(bridged.status == BR_STATUS_OK && r.value.nsec == bridged.value.nsec);
  }
}

static void test_separators_and_case(void) {
  /* T, t, and space are all accepted for the date-time separator; Z and z for
     the zero offset. */
  assert(br_rfc3339_parse(sv("2020-01-02T03:04:05Z")).status == BR_STATUS_OK);
  assert(br_rfc3339_parse(sv("2020-01-02t03:04:05z")).status == BR_STATUS_OK);
  assert(br_rfc3339_parse(sv("2020-01-02 03:04:05Z")).status == BR_STATUS_OK);

  /* All three denote the same instant. */
  {
    br_time a = br_rfc3339_parse(sv("2020-01-02T03:04:05Z")).value;
    br_time b = br_rfc3339_parse(sv("2020-01-02t03:04:05z")).value;
    br_time c = br_rfc3339_parse(sv("2020-01-02 03:04:05Z")).value;
    assert(a.nsec == b.nsec && b.nsec == c.nsec);
  }
}

static void test_fractional(void) {
  /* 1..9 digits scale to nanoseconds by digit count. */
  br_rfc3339_result r1 = br_rfc3339_parse(sv("2020-01-01T00:00:00.5Z"));
  br_rfc3339_result r9 = br_rfc3339_parse(sv("2020-01-01T00:00:00.123456789Z"));
  br_rfc3339_result r12 = br_rfc3339_parse(sv("2020-01-01T00:00:00.123456789999Z"));

  assert(r1.status == BR_STATUS_OK);
  assert(r1.value.nsec % 1000000000ll == 500000000ll); /* .5 -> 500 ms */

  assert(r9.status == BR_STATUS_OK);
  assert(r9.value.nsec % 1000000000ll == 123456789ll);

  /* >9 digits: extra digits accepted-but-ignored, value is the first 9. */
  assert(r12.status == BR_STATUS_OK);
  assert(r12.value.nsec % 1000000000ll == 123456789ll);

  /* A dot with no digits is malformed. */
  assert(br_rfc3339_parse(sv("2020-01-01T00:00:00.Z")).status == BR_STATUS_INVALID_ENCODING);
}

static void test_leap_second(void) {
  /* Accepted only as minute 59 second 60; smeared to :59.999999999 + flagged. */
  br_rfc3339_result ok = br_rfc3339_parse(sv("1990-12-31T23:59:60Z"));
  assert(ok.status == BR_STATUS_OK);
  assert(ok.is_leap_second);
  {
    br_datetime dt = br_time_to_datetime(ok.value);
    assert(dt.time.second == 59 && dt.time.nano == 999999999);
  }

  /* A :60 at any other minute is malformed (component validation rejects it). */
  assert(br_rfc3339_parse(sv("1990-12-31T23:30:60Z")).status == BR_STATUS_INVALID_ENCODING);
  assert(br_rfc3339_parse(sv("1990-12-31T12:00:60Z")).status == BR_STATUS_INVALID_ENCODING);
}

static void test_offsets(void) {
  /* Offset normalizes to UTC and is preserved. +05:30 means local is ahead of
     UTC, so the UTC instant is earlier. */
  br_rfc3339_result plus = br_rfc3339_parse(sv("2020-01-01T12:00:00+05:30"));
  br_rfc3339_result minus = br_rfc3339_parse(sv("2020-01-01T12:00:00-08:00"));
  br_rfc3339_result zulu = br_rfc3339_parse(sv("2020-01-01T12:00:00Z"));

  assert(plus.status == BR_STATUS_OK && plus.utc_offset_min == 330);
  assert(minus.status == BR_STATUS_OK && minus.utc_offset_min == -480);
  assert(zulu.status == BR_STATUS_OK && zulu.utc_offset_min == 0);

  /* +05:30 local == 06:30 UTC; -08:00 local == 20:00 UTC. */
  assert(plus.value.nsec == zulu.value.nsec - 330ll * 60 * 1000000000ll);
  assert(minus.value.nsec == zulu.value.nsec + 480ll * 60 * 1000000000ll);
}

static void test_malformed(void) {
  /* Each returns INVALID_ENCODING; several checked for consumed at the bad byte. */
  assert(br_rfc3339_parse(sv("")).status == BR_STATUS_INVALID_ENCODING);
  assert(br_rfc3339_parse(sv("not-a-date")).status == BR_STATUS_INVALID_ENCODING);
  assert(br_rfc3339_parse(sv("2020-13-01T00:00:00Z")).status ==
         BR_STATUS_INVALID_ENCODING); /* month */
  assert(br_rfc3339_parse(sv("2020-01-32T00:00:00Z")).status ==
         BR_STATUS_INVALID_ENCODING); /* day */
  assert(br_rfc3339_parse(sv("2020-01-01T25:00:00Z")).status ==
         BR_STATUS_INVALID_ENCODING); /* hour */
  assert(br_rfc3339_parse(sv("2020-01-01T00:60:00Z")).status ==
         BR_STATUS_INVALID_ENCODING); /* minute */
  assert(br_rfc3339_parse(sv("2020-01-01T00:00:00")).status ==
         BR_STATUS_INVALID_ENCODING);                                              /* no offset */
  assert(br_rfc3339_parse(sv("2020-01-01")).status == BR_STATUS_INVALID_ENCODING); /* date only */

  /* consumed points at the first bad byte: the year is 4 good digits, then '-'
     expected but 'X' found at index 4. */
  {
    br_rfc3339_result r = br_rfc3339_parse(sv("2020X01-01T00:00:00Z"));
    assert(r.status == BR_STATUS_INVALID_ENCODING && r.consumed == 4u);
  }
}

static void test_strict_vs_prefix(void) {
  const char *with_trailer = "2020-01-01T00:00:00Z and then some log text";

  /* Strict rejects trailing bytes. */
  assert(br_rfc3339_parse(sv(with_trailer)).status == BR_STATUS_INVALID_ENCODING);

  /* Prefix accepts them, consuming exactly the timestamp (20 bytes). */
  {
    br_rfc3339_result r = br_rfc3339_parse_prefix(sv(with_trailer));
    assert(r.status == BR_STATUS_OK);
    assert(r.consumed == 20u);
  }

  /* On an exact timestamp, strict and prefix agree. */
  {
    br_rfc3339_result strict = br_rfc3339_parse(sv("2020-01-01T00:00:00Z"));
    br_rfc3339_result prefix = br_rfc3339_parse_prefix(sv("2020-01-01T00:00:00Z"));
    assert(strict.status == BR_STATUS_OK && prefix.status == BR_STATUS_OK);
    assert(strict.consumed == prefix.consumed);
  }
}

static void test_round_trip(void) {
  /* parse -> format preserves the instant and offset for canonical cases. */
  static const char *cases[] = {
    "2020-01-02T03:04:05Z",
    "1985-04-12T23:20:50.520000000Z", /* trailing zeros will trim to .52 */
    "2020-06-15T12:30:00+05:30",
    "1970-01-01T00:00:00Z",
  };
  static const char *expect[] = {
    "2020-01-02T03:04:05Z",
    "1985-04-12T23:20:50.52Z",
    "2020-06-15T12:30:00+05:30",
    "1970-01-01T00:00:00Z",
  };
  size_t i;

  for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    br_rfc3339_result r = br_rfc3339_parse(sv(cases[i]));
    char out[BR_RFC3339_MAX + 1];
    assert(r.status == BR_STATUS_OK);
    assert(format_into(r.value, r.utc_offset_min, out, sizeof(out)) == BR_STATUS_OK);
    assert(strcmp(out, expect[i]) == 0);
  }
}

static void test_leap_round_trip_asymmetry(void) {
  /* The :60 leap parses to the smeared instant; re-formatting emits
     :59.999999999 (documented text asymmetry — the instant is preserved, the
     text is not). */
  br_rfc3339_result r = br_rfc3339_parse(sv("1990-12-31T23:59:60Z"));
  char out[BR_RFC3339_MAX + 1];
  assert(r.status == BR_STATUS_OK && r.is_leap_second);
  assert(format_into(r.value, r.utc_offset_min, out, sizeof(out)) == BR_STATUS_OK);
  assert(strcmp(out, "1990-12-31T23:59:59.999999999Z") == 0);
}

static void test_format_true_extremes(void) {
  /* The REAL boundaries are br_time's int64-ns limits (~year 2262 and 1677),
     not year 9999 (which br_time cannot hold). The exact extreme instants both
     format and round-trip. Pinned strings verified against Python's datetime
     for INT64_MAX/MIN nanoseconds since 1970. */
  char out[BR_RFC3339_MAX + 1];
  br_time hi;
  br_time lo;
  br_rfc3339_result rt;

  hi.nsec = INT64_MAX; /* 2262-04-11T23:47:16.854775807Z */
  assert(format_into(hi, 0, out, sizeof(out)) == BR_STATUS_OK);
  assert(strcmp(out, "2262-04-11T23:47:16.854775807Z") == 0);
  rt = br_rfc3339_parse(sv(out));
  assert(rt.status == BR_STATUS_OK && rt.value.nsec == hi.nsec);

  lo.nsec = INT64_MIN; /* 1677-09-21T00:12:43.145224192Z */
  assert(format_into(lo, 0, out, sizeof(out)) == BR_STATUS_OK);
  assert(strcmp(out, "1677-09-21T00:12:43.145224192Z") == 0);
  rt = br_rfc3339_parse(sv(out));
  assert(rt.status == BR_STATUS_OK && rt.value.nsec == lo.nsec);

  /* format cannot range-fail (every br_time is a 4-digit year); its only
     failure is SHORT_BUFFER. Undersized buffer -> SHORT_BUFFER, count 0. */
  {
    br_time t;
    br_io_result r;
    t.nsec = 0;
    r = br_rfc3339_format(t, 0, (u8 *)out, 5u);
    assert(r.status == BR_STATUS_SHORT_BUFFER && r.count == 0u);
  }
}

static void test_parse_out_of_range(void) {
  /* RFC 3339 grammar admits years the int64-ns br_time cannot hold. Such a
     timestamp is well-FORMED (consumed = full length) but the instant is
     unrepresentable, so parse propagates the bridge's OUT_OF_RANGE. */
  br_rfc3339_result before = br_rfc3339_parse(sv("1500-01-01T00:00:00Z"));
  br_rfc3339_result after = br_rfc3339_parse(sv("3000-01-01T00:00:00Z"));

  assert(before.status == BR_STATUS_OUT_OF_RANGE);
  assert(before.consumed == 20u); /* whole timestamp consumed — text was valid */
  assert(after.status == BR_STATUS_OUT_OF_RANGE);
  assert(after.consumed == 20u);

  /* A year just inside the window parses fine. */
  assert(br_rfc3339_parse(sv("2000-01-01T00:00:00Z")).status == BR_STATUS_OK);
}

int main(void) {
  test_parse_basic();
  test_separators_and_case();
  test_fractional();
  test_leap_second();
  test_offsets();
  test_malformed();
  test_strict_vs_prefix();
  test_round_trip();
  test_leap_round_trip_asymmetry();
  test_format_true_extremes();
  test_parse_out_of_range();
  return 0;
}
