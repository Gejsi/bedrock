#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <bedrock.h>

static br_date date_of(i64 y, int8_t m, int8_t d) {
  br_date date;
  date.year = y;
  date.month = m;
  date.day = d;
  return date;
}

/*
ABSOLUTE known-value vectors — the correctness anchor that round-trips alone
cannot provide (a uniformly-off-by-k ordinal function would still round-trip and
still map 1970->0, because the error cancels). These constants were verified
against an INDEPENDENT oracle, Python's `datetime.date.toordinal()` and
`datetime.timestamp()`, which use the same proleptic-Gregorian ordinal system
(ordinal 1 == 0001-01-01):
  date(1970,1,1).toordinal() == 719163
  date(2000,1,1).toordinal() == 730120
  datetime(2000,1,1,tz=utc).timestamp() == 946684800
  datetime(2004,2,29,tz=utc).timestamp() == 1078012800  (leap day)
*/
static void test_absolute_ordinals(void) {
  assert(br_date_to_ordinal(date_of(1970, 1, 1)) == 719163);
  assert(br_date_to_ordinal(date_of(2000, 1, 1)) == 730120);
  /* ordinal 1 is 0001-01-01 by definition. */
  assert(br_date_to_ordinal(date_of(1, 1, 1)) == 1);
}

static void test_absolute_epoch(void) {
  br_datetime dt;
  br_time_result r;

  /* 1970-01-01T00:00:00Z == 0 nsec. */
  dt = br_datetime_from_components(1970, 1, 1, 0, 0, 0, 0).value;
  r = br_datetime_to_time(dt);
  assert(r.status == BR_STATUS_OK && r.value.nsec == 0);

  /* 2000-01-01T00:00:00Z == 946684800 sec. */
  dt = br_datetime_from_components(2000, 1, 1, 0, 0, 0, 0).value;
  r = br_datetime_to_time(dt);
  assert(r.status == BR_STATUS_OK && r.value.nsec == 946684800ll * 1000000000ll);

  /* 2004-02-29T00:00:00Z == 1078012800 sec (leap day exists). */
  dt = br_datetime_from_components(2004, 2, 29, 0, 0, 0, 0).value;
  r = br_datetime_to_time(dt);
  assert(r.status == BR_STATUS_OK && r.value.nsec == 1078012800ll * 1000000000ll);
}

static void test_bridge_out_of_range(void) {
  /* br_time is int64 ns since 1970 (~civil years 1677..2262). A datetime
     outside that window returns OUT_OF_RANGE via the overflow-safe check —
     never a silent wrap (the pre-ruling bug mapped 9999 -> 1816). */
  br_datetime far_future = br_datetime_from_components(9999, 12, 31, 23, 59, 59, 999999999).value;
  br_datetime just_past = br_datetime_from_components(2263, 1, 1, 0, 0, 0, 0).value;
  br_datetime far_past = br_datetime_from_components(1000, 1, 1, 0, 0, 0, 0).value;
  br_time_result rf = br_datetime_to_time(far_future);
  br_time_result rp = br_datetime_to_time(just_past);
  br_time_result rq = br_datetime_to_time(far_past);

  assert(rf.status == BR_STATUS_OUT_OF_RANGE && rf.value.nsec == 0);
  assert(rp.status == BR_STATUS_OUT_OF_RANGE);
  assert(rq.status == BR_STATUS_OUT_OF_RANGE);

  /* In-window instants near both extremes succeed and do NOT wrap. */
  {
    br_datetime y2262 = br_datetime_from_components(2262, 1, 1, 0, 0, 0, 0).value;
    br_datetime y1678 = br_datetime_from_components(1678, 1, 1, 0, 0, 0, 0).value;
    br_time_result a = br_datetime_to_time(y2262);
    br_time_result b = br_datetime_to_time(y1678);
    assert(a.status == BR_STATUS_OK);
    assert(b.status == BR_STATUS_OK && b.value.nsec < 0); /* pre-1970 is negative */
  }
}

static void test_ordinal_round_trip(void) {
  /* Round-trip a spread of dates incl. leap years, century and 400-year
     boundaries, negative (B.C.) years, and the extreme representable years
     (BR_DATETIME_MIN/MAX_YEAR themselves — these prove the ordinal computation
     is overflow-clean at the bound, the UBSan regression target for the
     pure-int64 400-year-cycle formula). B.C. coverage is round-trip-only (no
     independent B.C. oracle was sourced). */
  static const i64 years[] = {BR_DATETIME_MIN_YEAR,
                              -10000,
                              -400,
                              -100,
                              -4,
                              -1,
                              0,
                              1,
                              1900,
                              1999,
                              2000,
                              2004,
                              9999,
                              100000,
                              BR_DATETIME_MAX_YEAR};
  size_t yi;
  int8_t m;

  for (yi = 0u; yi < sizeof(years) / sizeof(years[0]); ++yi) {
    for (m = 1; m <= 12; ++m) {
      br_date d = date_of(years[yi], m, 15);
      br_ordinal ord = br_date_to_ordinal(d);
      br_date back = br_ordinal_to_date(ord);
      assert(back.year == d.year && back.month == d.month && back.day == d.day);
    }
  }

  /* The exact endpoints: first day of MIN_YEAR and last day of MAX_YEAR must
     produce BR_ORDINAL_MIN/MAX and round-trip through a date. */
  {
    br_date min_first = date_of(BR_DATETIME_MIN_YEAR, 1, 1);
    br_date max_last = date_of(BR_DATETIME_MAX_YEAR, 12, 31);
    assert(br_date_to_ordinal(min_first) == BR_ORDINAL_MIN);
    assert(br_date_to_ordinal(max_last) == BR_ORDINAL_MAX);
  }

  /* Ordinal MIN/MAX map back to themselves through a date. */
  {
    br_date lo = br_ordinal_to_date(BR_ORDINAL_MIN);
    br_date hi = br_ordinal_to_date(BR_ORDINAL_MAX);
    assert(br_date_to_ordinal(lo) == BR_ORDINAL_MIN);
    assert(br_date_to_ordinal(hi) == BR_ORDINAL_MAX);
  }
}

static void test_leap_year(void) {
  assert(!br_is_leap_year(1900)); /* century, not /400 */
  assert(br_is_leap_year(2000));  /* /400 */
  assert(br_is_leap_year(2004));  /* /4 */
  assert(!br_is_leap_year(2001));
  assert(br_is_leap_year(0)); /* year 0 is /400 in the proleptic calendar */
}

static void test_epoch_round_trip(void) {
  /* A spread of instants round-trips datetime <-> time exactly, including
     negative (pre-epoch) times. Values are kept within br_time's int64-ns
     window (~1677..2262); the second-count * 1e9 must itself stay in range, so
     the pre-epoch entries use a large-but-representable magnitude rather than
     an out-of-window one (e.g. year 1, which br_time cannot hold). */
  static const i64 secs[] = {0, 1, -1, 86400, -86400, 946684800, 1078012800, -2208988800};
  size_t i;

  for (i = 0u; i < sizeof(secs) / sizeof(secs[0]); ++i) {
    br_time t;
    br_datetime dt;
    br_time_result back;
    t.nsec = secs[i] * 1000000000ll + 123456789ll;
    dt = br_time_to_datetime(t);
    back = br_datetime_to_time(dt);
    assert(back.status == BR_STATUS_OK && back.value.nsec == t.nsec);
  }
}

static void test_validation(void) {
  /* Zero-value contracts. */
  br_time_of_day zero_tod;
  br_date zero_date;
  memset(&zero_tod, 0, sizeof(zero_tod));
  memset(&zero_date, 0, sizeof(zero_date));
  assert(br_time_of_day_validate(zero_tod) == BR_STATUS_OK);         /* midnight is valid */
  assert(br_date_validate(zero_date) == BR_STATUS_INVALID_ARGUMENT); /* month/day 0 invalid */

  /* Out-of-range fields -> INVALID_ARGUMENT. */
  assert(br_date_validate(date_of(2020, 13, 1)) == BR_STATUS_INVALID_ARGUMENT); /* month */
  assert(br_date_validate(date_of(2020, 2, 30)) == BR_STATUS_INVALID_ARGUMENT); /* day */
  assert(br_date_validate(date_of(2021, 2, 29)) ==
         BR_STATUS_INVALID_ARGUMENT);                             /* not a leap year */
  assert(br_date_validate(date_of(2020, 2, 29)) == BR_STATUS_OK); /* leap year */

  {
    br_time_of_day t;
    memset(&t, 0, sizeof(t));
    t.hour = 24;
    assert(br_time_of_day_validate(t) == BR_STATUS_INVALID_ARGUMENT);
    t.hour = 0;
    t.second = 60; /* never valid post-smear */
    assert(br_time_of_day_validate(t) == BR_STATUS_INVALID_ARGUMENT);
    t.second = 0;
    t.nano = 1000000000;
    assert(br_time_of_day_validate(t) == BR_STATUS_INVALID_ARGUMENT);
  }

  /* from_components rejects bad fields with a zeroed value. */
  {
    br_datetime_result r = br_datetime_from_components(2020, 13, 1, 0, 0, 0, 0);
    assert(r.status == BR_STATUS_INVALID_ARGUMENT);
  }
}

static void test_add_subtract(void) {
  br_datetime base = br_datetime_from_components(2020, 1, 31, 12, 0, 0, 0).value;
  br_delta one_day;
  br_datetime_result next;
  br_delta diff;

  one_day.days = 1;
  one_day.seconds = 0;
  one_day.nanos = 0;
  next = br_datetime_add_delta(base, one_day);
  assert(next.status == BR_STATUS_OK);
  assert(next.value.date.year == 2020 && next.value.date.month == 2 && next.value.date.day == 1);

  /* subtract recovers the one-day span. */
  diff = br_datetime_subtract(next.value, base);
  assert(diff.days == 1 && diff.seconds == 0 && diff.nanos == 0);

  /* nano carry: adding 1e9 nanos advances one second. */
  {
    br_delta d;
    br_datetime_result r;
    d.days = 0;
    d.seconds = 0;
    d.nanos = 1000000000;
    r = br_datetime_add_delta(base, d);
    assert(r.status == BR_STATUS_OK);
    assert(r.value.time.second == 1 && r.value.time.nano == 0);
  }
}

int main(void) {
  test_absolute_ordinals();
  test_absolute_epoch();
  test_bridge_out_of_range();
  test_ordinal_round_trip();
  test_leap_year();
  test_epoch_round_trip();
  test_validation();
  test_add_subtract();
  return 0;
}
