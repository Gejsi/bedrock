#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <bedrock/time/time.h>

#if defined(__linux__)

#include <errno.h>
#include <sched.h>
#include <time.h>

bool br_time_is_supported(void) {
  return true;
}

br_time br_time_now(void) {
  struct timespec ts;
  br_time result = {0};

  if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
    result.nsec = (i64)ts.tv_sec * BR_SECOND + (i64)ts.tv_nsec;
  }
  return result;
}

void br_sleep(br_duration duration) {
  struct timespec ts;

  if (duration <= 0) {
    return;
  }

  ts.tv_sec = (time_t)(duration / BR_SECOND);
  ts.tv_nsec = (long)(duration % BR_SECOND);
  while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
  }
}

br_tick br_tick_now(void) {
  struct timespec ts;
  br_tick result = {0};

  if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0) {
    result.nsec = (i64)ts.tv_sec * BR_SECOND + (i64)ts.tv_nsec;
  }
  return result;
}

void br_yield(void) {
  sched_yield();
}

#else
typedef u8 br__time_linux_translation_unit;
#endif
