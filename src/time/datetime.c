#include <bedrock/time/datetime.h>

/*
Proleptic-Gregorian calendar math, ported from the Reingold-Dershowitz
algorithm. All arithmetic is pure int64: the ordinal is computed with the
400-year-cycle reformulation (146097 days per 400 years) so no intermediate
term overflows within the representable year range. The naive R-D year term
(365*y + y/4 - y/100 + y/400) would transiently exceed int64 before its later
terms corrected it; the cycle form never does, because the year bounds
(BR_DATETIME_MIN/MAX_YEAR) are chosen as the widest at which every term of this
formula stays in int64 with margin.
*/

static const int8_t br__month_days[13] = {-1, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/* Days in 400 proleptic-Gregorian years: 400*365 + 97 leap days. */
#define BR__DAYS_PER_400_YEARS 146097ll

/* Floored division: rounds toward negative infinity, not toward zero. This is
   what the calendar math needs so that years before 1 A.D. (negative) land on
   the correct ordinal. */
static i64 br__floor_div(i64 x, i64 y) {
  i64 q = x / y;
  i64 r = x % y;
  if (r != 0 && ((r < 0) != (y < 0))) {
    q -= 1;
  }
  return q;
}

/* Floored modulo: remainder takes the sign of the divisor, in [0, y) for y > 0.
   Computed without multiplying the quotient back (which would overflow near the
   int64 limits), so it is safe for the extreme instants. */
static i64 br__floor_mod(i64 x, i64 y) {
  i64 r = x % y;
  if (r != 0 && ((r < 0) != (y < 0))) {
    r += y;
  }
  return r;
}

/* Floored divmod: quotient floored, remainder takes the sign of the divisor. */
static void br__divmod(i64 x, i64 y, i64 *out_q, i64 *out_r) {
  i64 q = x / y;
  i64 r = x % y;
  if (r != 0 && ((r < 0) != (y < 0))) {
    q -= 1;
    r += y;
  }
  *out_q = q;
  *out_r = r;
}

bool br_is_leap_year(i64 year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int8_t br__days_in_month(i64 year, int8_t month) {
  if (month == 2 && br_is_leap_year(year)) {
    return 29;
  }
  return br__month_days[month];
}

br_ordinal br_date_to_ordinal(br_date date) {
  i64 y = date.year - 1;
  /* Split y into whole 400-year cycles plus a remainder in [0, 400). The full
     cycles contribute a fixed 146097 days each; the remainder's leap days are
     counted with the standard 4/100 rule (no 400 term, since rc < 400). This
     keeps every intermediate far inside int64 across the whole year range,
     unlike the naive 365*y accumulation. */
  i64 cycle = br__floor_div(y, 400);
  i64 rc = y - cycle * 400;
  i64 ordinal = BR__DAYS_PER_400_YEARS * cycle;

  ordinal += 365 * rc + rc / 4 - rc / 100;
  ordinal += br__floor_div(367 * (i64)date.month - 362, 12);

  if (date.month <= 2) {
    /* no correction */
  } else if (br_is_leap_year(date.year)) {
    ordinal -= 1;
  } else {
    ordinal -= 2;
  }

  ordinal += (i64)date.day;
  return ordinal;
}

/* Estimate the year containing `ordinal` via the 400/100/4/1-year cycle counts,
   with the boundary correction Reingold-Dershowitz specify. */
static i64 br__ordinal_to_year(br_ordinal ordinal) {
  i64 d0 = ordinal - 1;
  i64 n400, d1, n100, d2, n4, d3, n1, d4;
  i64 year;

  br__divmod(d0, 365 * 400 + 100 - 3, &n400, &d1);
  br__divmod(d1, 365 * 100 + 25 - 1, &n100, &d2);
  br__divmod(d2, 365 * 4 + 1, &n4, &d3);
  br__divmod(d3, 365, &n1, &d4);

  year = 400 * n400 + 100 * n100 + 4 * n4 + n1;
  if (n100 == 4 || n1 == 4) {
    return year; /* last day of a leap cycle: year is correct as-is */
  }
  return year + 1;
}

br_date br_ordinal_to_date(br_ordinal ordinal) {
  i64 year = br__ordinal_to_year(ordinal);
  br_date jan1;
  br_date mar1;
  i64 prior_days;
  i64 correction;
  i64 month;
  br_date month1;
  br_date result;

  jan1.year = year;
  jan1.month = 1;
  jan1.day = 1;
  prior_days = ordinal - br_date_to_ordinal(jan1);

  mar1.year = year;
  mar1.month = 3;
  mar1.day = 1;
  if (ordinal < br_date_to_ordinal(mar1)) {
    correction = 0;
  } else if (br_is_leap_year(year)) {
    correction = 1;
  } else {
    correction = 2;
  }

  month = br__floor_div(12 * (prior_days + correction) + 373, 367);

  month1.year = year;
  month1.month = (int8_t)month;
  month1.day = 1;

  result.year = year;
  result.month = (int8_t)month;
  result.day = (int8_t)(ordinal - br_date_to_ordinal(month1) + 1);
  return result;
}

br_status br_date_validate(br_date date) {
  if (date.year < BR_DATETIME_MIN_YEAR || date.year > BR_DATETIME_MAX_YEAR) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (date.month < 1 || date.month > 12) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (date.day < 1 || date.day > br__days_in_month(date.year, date.month)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  return BR_STATUS_OK;
}

br_status br_time_of_day_validate(br_time_of_day t) {
  if (t.hour < 0 || t.hour > 23) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (t.minute < 0 || t.minute > 59) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  /* second 60 is never stored: a parsed leap second is smeared to 59 before any
     datetime is built, so validation rejects 60 uniformly. */
  if (t.second < 0 || t.second > 59) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (t.nano < 0 || t.nano > 999999999) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  return BR_STATUS_OK;
}

br_status br_datetime_validate(br_datetime dt) {
  br_status status = br_date_validate(dt.date);
  if (status != BR_STATUS_OK) {
    return status;
  }
  return br_time_of_day_validate(dt.time);
}

static br_datetime_result br__datetime_result(br_datetime value, br_status status) {
  br_datetime_result result;
  result.value = value;
  result.status = status;
  return result;
}

br_datetime_result br_datetime_from_components(
  i64 year, int8_t month, int8_t day, int8_t hour, int8_t minute, int8_t second, int32_t nano) {
  br_datetime dt;
  br_status status;

  dt.date.year = year;
  dt.date.month = month;
  dt.date.day = day;
  dt.time.hour = hour;
  dt.time.minute = minute;
  dt.time.second = second;
  dt.time.nano = nano;

  status = br_datetime_validate(dt);
  if (status != BR_STATUS_OK) {
    br_datetime zero;
    memset(&zero, 0, sizeof(zero));
    return br__datetime_result(zero, status);
  }
  return br__datetime_result(dt, BR_STATUS_OK);
}

/* Seconds since midnight for a time of day (nanos handled separately). */
static i64 br__tod_seconds(br_time_of_day t) {
  return ((i64)t.hour * 60 + (i64)t.minute) * 60 + (i64)t.second;
}

#define BR__SECONDS_PER_DAY 86400ll
#define BR__NANOS_PER_SEC 1000000000ll

br_datetime_result br_datetime_add_delta(br_datetime dt, br_delta delta) {
  br_ordinal ord = br_date_to_ordinal(dt.date);
  i64 seconds = br__tod_seconds(dt.time) + delta.seconds;
  i64 nanos = (i64)dt.time.nano + delta.nanos;
  i64 days = ord + delta.days;
  i64 carry;
  br_date date;
  br_datetime out;

  /* Normalize nanos into seconds (floored, so negative deltas borrow). */
  carry = br__floor_div(nanos, BR__NANOS_PER_SEC);
  nanos -= carry * BR__NANOS_PER_SEC;
  seconds += carry;

  /* Normalize seconds into days. */
  carry = br__floor_div(seconds, BR__SECONDS_PER_DAY);
  seconds -= carry * BR__SECONDS_PER_DAY;
  days += carry;

  date = br_ordinal_to_date((br_ordinal)days);
  out.date = date;
  out.time.hour = (int8_t)(seconds / 3600);
  out.time.minute = (int8_t)((seconds / 60) % 60);
  out.time.second = (int8_t)(seconds % 60);
  out.time.nano = (int32_t)nanos;

  return br__datetime_result(out, br_datetime_validate(out));
}

br_delta br_datetime_subtract(br_datetime a, br_datetime b) {
  br_delta delta;
  delta.days = (i64)br_date_to_ordinal(a.date) - (i64)br_date_to_ordinal(b.date);
  delta.seconds = br__tod_seconds(a.time) - br__tod_seconds(b.time);
  delta.nanos = (i64)a.time.nano - (i64)b.time.nano;
  return delta;
}

/* Ordinal of the Unix epoch (1970-01-01), derived from the same R-D function
   rather than a magic constant, so the bridge stays self-consistent with the
   calendar math. */
static br_ordinal br__unix_epoch_ordinal(void) {
  br_date epoch;
  epoch.year = 1970;
  epoch.month = 1;
  epoch.day = 1;
  return br_date_to_ordinal(epoch);
}

static br_time_result br__time_result(i64 nsec, br_status status) {
  br_time_result result;
  result.value.nsec = nsec;
  result.status = status;
  return result;
}

br_time_result br_datetime_to_time(br_datetime dt) {
  br_ordinal ord = br_date_to_ordinal(dt.date);
  i64 days = (i64)ord - (i64)br__unix_epoch_ordinal();
  i64 seconds;
  i64 nsec;

  /* Overflow-safe: bound each step against the i64 limits BEFORE performing it,
     so a datetime beyond br_time's ~1677..2262 window returns OUT_OF_RANGE
     instead of silently wrapping. The intraday `seconds` and `nano` addends are
     both non-negative, so only `days*SECONDS_PER_DAY` and `total*NANOS_PER_SEC`
     can breach a limit; bounds use floored division to stay exact at the
     negative extreme (truncation would wrongly reject the INT64_MIN instant).
     The `+ addend` bumps the max bound down by one unit so the later add cannot
     overflow, and never subtracts from INT64_MIN (which would itself wrap). */
  seconds = br__tod_seconds(dt.time); /* 0 .. 86399 */

  /* days*SECONDS_PER_DAY, leaving room for + seconds. */
  if (days > br__floor_div(INT64_MAX - seconds, BR__SECONDS_PER_DAY) ||
      days < br__floor_div(INT64_MIN, BR__SECONDS_PER_DAY)) {
    return br__time_result(0, BR_STATUS_OUT_OF_RANGE);
  }
  seconds += days * BR__SECONDS_PER_DAY;

  /* seconds*NANOS_PER_SEC + nano must land within [INT64_MIN, INT64_MAX]. The
     upper bound divides the headroom (INT64_MAX - nano). The lower bound cannot
     just compare against floor(INT64_MIN/NANOS_PER_SEC): the smallest valid
     instant (INT64_MIN ns) sits at seconds == that floored quotient WITH a
     specific positive nano, so the quotient row is only out of range when nano
     is below INT64_MIN's own floored remainder. Both checks avoid the multiply,
     staying overflow-safe at the extreme. */
  if (seconds > br__floor_div(INT64_MAX - (i64)dt.time.nano, BR__NANOS_PER_SEC)) {
    return br__time_result(0, BR_STATUS_OUT_OF_RANGE);
  }
  if (seconds < br__floor_div(INT64_MIN, BR__NANOS_PER_SEC) ||
      (seconds == br__floor_div(INT64_MIN, BR__NANOS_PER_SEC) &&
       (i64)dt.time.nano < br__floor_mod(INT64_MIN, BR__NANOS_PER_SEC))) {
    return br__time_result(0, BR_STATUS_OUT_OF_RANGE);
  }
  /* The guards above prove the result lands in [INT64_MIN, INT64_MAX], but the
     product `seconds * NANOS_PER_SEC` transiently exceeds int64 at the negative
     extreme before `+ nano` pulls it back to exactly INT64_MIN. For negative
     seconds, shift the multiply up by one unit so the product stays in range,
     then subtract the corresponding remainder -- pure int64, no wide type. */
  if (seconds >= 0) {
    nsec = seconds * BR__NANOS_PER_SEC + (i64)dt.time.nano;
  } else {
    nsec = (seconds + 1) * BR__NANOS_PER_SEC - (BR__NANOS_PER_SEC - (i64)dt.time.nano);
  }
  return br__time_result(nsec, BR_STATUS_OK);
}

br_datetime br_time_to_datetime(br_time t) {
  /* Remainders via floored-mod (no multiply-back): total_seconds*NANOS_PER_SEC
     would overflow int64 at the INT64_MIN-nanosecond instant. */
  i64 total_seconds = br__floor_div(t.nsec, BR__NANOS_PER_SEC);
  i64 nanos = br__floor_mod(t.nsec, BR__NANOS_PER_SEC);
  i64 days = br__floor_div(total_seconds, BR__SECONDS_PER_DAY);
  i64 secs = br__floor_mod(total_seconds, BR__SECONDS_PER_DAY);
  br_datetime out;

  out.date = br_ordinal_to_date((br_ordinal)(br__unix_epoch_ordinal() + days));
  out.time.hour = (int8_t)(secs / 3600);
  out.time.minute = (int8_t)((secs / 60) % 60);
  out.time.second = (int8_t)(secs % 60);
  out.time.nano = (int32_t)nanos;
  return out;
}
