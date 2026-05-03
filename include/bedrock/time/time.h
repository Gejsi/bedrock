#ifndef BEDROCK_TIME_TIME_H
#define BEDROCK_TIME_TIME_H

#include <bedrock/base.h>

BR_EXTERN_C_BEGIN

/*
Type representing duration, with nanosecond precision.
*/
typedef i64 br_duration;

#define BR_NANOSECOND ((br_duration)1)
#define BR_MICROSECOND ((br_duration)(1000 * BR_NANOSECOND))
#define BR_MILLISECOND ((br_duration)(1000 * BR_MICROSECOND))
#define BR_SECOND ((br_duration)(1000 * BR_MILLISECOND))
#define BR_MINUTE ((br_duration)(60 * BR_SECOND))
#define BR_HOUR ((br_duration)(60 * BR_MINUTE))
#define BR_MIN_DURATION ((br_duration)INT64_MIN)
#define BR_MAX_DURATION ((br_duration)INT64_MAX)

/*
Specifies time since the Unix epoch, with nanosecond precision.
*/
typedef struct br_time {
  i64 nsec;
} br_time;

/*
Type representing monotonic time, useful for measuring durations.
*/
typedef struct br_tick {
  i64 nsec;
} br_tick;

bool br_time_is_supported(void);

br_time br_time_now(void);
void br_sleep(br_duration duration);
void br_yield(void);

br_tick br_tick_now(void);
br_tick br_tick_add(br_tick tick, br_duration duration);
br_duration br_tick_diff(br_tick start, br_tick end);
br_duration br_tick_since(br_tick start);

/*
Incrementally obtain durations since the last tick.

The `prev` pointer must point to a valid tick. If `*prev` is zero-initialized,
the returned duration is 0 and `*prev` is set to the current tick.
*/
br_duration br_tick_lap_time(br_tick *prev);

br_duration br_time_diff(br_time start, br_time end);
br_duration br_time_since(br_time start);

i64 br_duration_nanoseconds(br_duration duration);
f64 br_duration_microseconds(br_duration duration);
f64 br_duration_milliseconds(br_duration duration);
f64 br_duration_seconds(br_duration duration);
f64 br_duration_minutes(br_duration duration);
f64 br_duration_hours(br_duration duration);

BR_EXTERN_C_END

#endif
