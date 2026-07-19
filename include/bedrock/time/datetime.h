#ifndef BEDROCK_TIME_DATETIME_H
#define BEDROCK_TIME_DATETIME_H

#include <bedrock/base.h>
#include <bedrock/time/time.h>

BR_EXTERN_C_BEGIN

/*
A minimal civil (naive) calendar layer: proleptic-Gregorian dates, times of day,
and the bridge to Unix-nanosecond instants. There is no timezone database; a
br_datetime is always a local/civil value, and RFC 3339 offsets are carried
separately by the rfc3339 layer.
*/

/* Proleptic-Gregorian day number; ordinal 1 is January 1st of year 1. */
typedef int64_t br_ordinal;

typedef struct br_date {
  int64_t year; /* BR_DATETIME_MIN_YEAR .. BR_DATETIME_MAX_YEAR */
  int8_t month; /* 1..12 */
  int8_t day;   /* 1..days-in-month (leap-aware) */
} br_date;

typedef struct br_time_of_day {
  int8_t hour;   /* 0..23 */
  int8_t minute; /* 0..59 */
  int8_t second; /* 0..59 */
  int32_t nano;  /* 0..999999999 */
} br_time_of_day;

/* A naive civil datetime: a date plus a time of day, no timezone. */
typedef struct br_datetime {
  br_date date;
  br_time_of_day time;
} br_datetime;

/* A signed span, used by add/subtract; fields need not be normalized on input. */
typedef struct br_delta {
  int64_t days;
  int64_t seconds;
  int64_t nanos;
} br_delta;

/*
Year bounds are the widest at which every intermediate term of the
proleptic-Gregorian ordinal computation (the 400-year-cycle form) stays within
int64 with margin -- deliberately a hair inside a naive i64-saturating limit,
but still astronomically beyond any reachable instant (br_time spans civil years
~1677..2262) and any real calendar use. Keeping the calendar core pure int64
avoids a 128-bit arithmetic path and its non-portable pieces.
*/
#define BR_DATETIME_MIN_YEAR (-25000000000000000ll)
#define BR_DATETIME_MAX_YEAR (25000000000000000ll)
#define BR_ORDINAL_MIN ((br_ordinal) - 9131062500000000365ll)
#define BR_ORDINAL_MAX ((br_ordinal)9131062500000000000ll)

/*
Convert between a date and its proleptic-Gregorian ordinal day number. The
conversions are exact inverses across the whole representable range, including
years before 1 A.D. Callers pass validated dates; an unvalidated out-of-range
date produces an unspecified (but memory-safe) ordinal.
*/
br_ordinal br_date_to_ordinal(br_date date);
br_date br_ordinal_to_date(br_ordinal ordinal);

/*
Report whether `year` is a Gregorian leap year.
*/
bool br_is_leap_year(int64_t year);

/*
Validate a caller-built value. An out-of-range field is caller misuse and
returns BR_STATUS_INVALID_ARGUMENT (distinct from the parse layer's
BR_STATUS_INVALID_ENCODING for malformed text).
*/
br_status br_date_validate(br_date date);
br_status br_time_of_day_validate(br_time_of_day t);
br_status br_datetime_validate(br_datetime dt);

typedef struct br_datetime_result {
  br_datetime value;
  br_status status;
} br_datetime_result;

/*
Build a datetime from components, validating each. Returns
BR_STATUS_INVALID_ARGUMENT (and a zeroed value) if any field is out of range.
*/
br_datetime_result br_datetime_from_components(
  int64_t year, int8_t month, int8_t day, int8_t hour, int8_t minute, int8_t second, int32_t nano);

/*
Add a (possibly unnormalized) delta to a datetime, returning a normalized,
re-validated result.
*/
br_datetime_result br_datetime_add_delta(br_datetime dt, br_delta delta);

/*
Return `a - b` as a normalized delta.
*/
br_delta br_datetime_subtract(br_datetime a, br_datetime b);

typedef struct br_time_result {
  br_time value;
  br_status status;
} br_time_result;

/*
Bridge a civil datetime to a Unix-nanosecond instant, treating it as UTC.
`br_time` holds int64 nanoseconds since 1970, spanning civil years ~1677..2262
only, while a datetime spans far more — so this direction can fail: a datetime
outside the representable window returns BR_STATUS_OUT_OF_RANGE (via an
overflow-safe check, never a silent wrap).
*/
br_time_result br_datetime_to_time(br_datetime dt);

/*
Bridge a Unix-nanosecond instant to a civil datetime (UTC). Total: every int64
nanosecond value maps to a valid civil datetime.
*/
br_datetime br_time_to_datetime(br_time t);

BR_EXTERN_C_END

#endif
