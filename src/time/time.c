#include <bedrock/time/time.h>

br_tick br_tick_add(br_tick tick, br_duration duration) {
  tick.nsec += duration;
  return tick;
}

br_duration br_tick_diff(br_tick start, br_tick end) {
  return end.nsec - start.nsec;
}

br_duration br_tick_since(br_tick start) {
  return br_tick_diff(start, br_tick_now());
}

br_duration br_tick_lap_time(br_tick *prev) {
  br_tick now;
  br_duration duration = 0;

  now = br_tick_now();
  if (prev->nsec != 0) {
    duration = br_tick_diff(*prev, now);
  }
  *prev = now;
  return duration;
}

br_duration br_time_diff(br_time start, br_time end) {
  return end.nsec - start.nsec;
}

br_duration br_time_since(br_time start) {
  return br_time_diff(start, br_time_now());
}

i64 br_duration_nanoseconds(br_duration duration) {
  return duration;
}

f64 br_duration_microseconds(br_duration duration) {
  return br_duration_seconds(duration) * 1e6;
}

f64 br_duration_milliseconds(br_duration duration) {
  return br_duration_seconds(duration) * 1e3;
}

f64 br_duration_seconds(br_duration duration) {
  br_duration sec = duration / BR_SECOND;
  br_duration nsec = duration % BR_SECOND;
  return (f64)sec + (f64)nsec / 1e9;
}

f64 br_duration_minutes(br_duration duration) {
  br_duration min = duration / BR_MINUTE;
  br_duration nsec = duration % BR_MINUTE;
  return (f64)min + (f64)nsec / (60.0 * 1e9);
}

f64 br_duration_hours(br_duration duration) {
  br_duration hour = duration / BR_HOUR;
  br_duration nsec = duration % BR_HOUR;
  return (f64)hour + (f64)nsec / (60.0 * 60.0 * 1e9);
}
