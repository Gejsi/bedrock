#include <assert.h>
#include <bedrock.h>

static bool test_f64_close(f64 lhs, f64 rhs) {
  f64 diff = lhs > rhs ? lhs - rhs : rhs - lhs;
  return diff < 0.000001;
}

static void test_duration_units(void) {
  assert(BR_NANOSECOND == 1);
  assert(BR_MICROSECOND == 1000);
  assert(BR_MILLISECOND == 1000 * BR_MICROSECOND);
  assert(BR_SECOND == 1000 * BR_MILLISECOND);
  assert(BR_MINUTE == 60 * BR_SECOND);
  assert(BR_HOUR == 60 * BR_MINUTE);

  assert(br_duration_nanoseconds(3 * BR_SECOND + 5) == 3 * BR_SECOND + 5);
  assert(test_f64_close(br_duration_microseconds(BR_MILLISECOND), 1000.0));
  assert(test_f64_close(br_duration_milliseconds(BR_SECOND), 1000.0));
  assert(test_f64_close(br_duration_seconds(BR_SECOND + 500 * BR_MILLISECOND), 1.5));
  assert(test_f64_close(br_duration_minutes(BR_MINUTE + 30 * BR_SECOND), 1.5));
  assert(test_f64_close(br_duration_hours(BR_HOUR + 30 * BR_MINUTE), 1.5));
}

static void test_tick_helpers(void) {
  br_tick start = {10};
  br_tick end = {25};
  br_tick added;
  br_tick lap = {0};
  br_duration lap_duration;

  assert(br_tick_diff(start, end) == 15);
  added = br_tick_add(start, 5);
  assert(added.nsec == 15);

  lap_duration = br_tick_lap_time(&lap);
  assert(lap_duration == 0);
  assert(lap.nsec != 0 || !br_time_is_supported());
}

static void test_now_and_sleep(void) {
  br_tick start;
  br_tick end;
  br_duration elapsed;

  if (!br_time_is_supported()) {
    return;
  }

  start = br_tick_now();
  br_yield();
  br_sleep(1 * BR_MILLISECOND);
  end = br_tick_now();
  elapsed = br_tick_diff(start, end);

  assert(end.nsec >= start.nsec);
  assert(elapsed >= 0);
}

int main(void) {
  test_duration_units();
  test_tick_helpers();
  test_now_and_sleep();
  return 0;
}
