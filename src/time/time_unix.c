#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <bedrock/time/time.h>

#if (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__) || defined(__NetBSD__) ||    \
  defined(__OpenBSD__)

#include <errno.h>
#include <sched.h>
#include <time.h>

#if defined(__APPLE__) && defined(__MACH__)
#define BR__TIME_TICK_CLOCK 4
#else
#define BR__TIME_TICK_CLOCK CLOCK_MONOTONIC
#endif

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

  if (clock_gettime(BR__TIME_TICK_CLOCK, &ts) == 0) {
    result.nsec = (i64)ts.tv_sec * BR_SECOND + (i64)ts.tv_nsec;
  }
  return result;
}

void br_yield(void) {
  sched_yield();
}

#else
typedef u8 br__time_unix_translation_unit;
#endif
