#ifndef BEDROCK_TIME_INTERNAL_H
#define BEDROCK_TIME_INTERNAL_H

#include <bedrock/time/time.h>

/*
Convert a normalized seconds/nanoseconds pair without overflowing at the
negative int64 boundary. The nanoseconds argument must be in 0..999999999.
*/
static inline i64 br__time_saturating_seconds_nanos(i64 seconds, i64 nanoseconds) {
  i64 min_seconds = INT64_MIN / BR_SECOND;
  i64 min_nanoseconds = INT64_MIN % BR_SECOND;

  if (min_nanoseconds < 0) {
    min_seconds -= 1;
    min_nanoseconds += BR_SECOND;
  }

  if (seconds > (INT64_MAX - nanoseconds) / BR_SECOND) {
    return INT64_MAX;
  }
  if (seconds < min_seconds || (seconds == min_seconds && nanoseconds < min_nanoseconds)) {
    return INT64_MIN;
  }

  if (seconds >= 0) {
    return seconds * BR_SECOND + nanoseconds;
  }
  return (seconds + 1) * BR_SECOND - (BR_SECOND - nanoseconds);
}

#endif
