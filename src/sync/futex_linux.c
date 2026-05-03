#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <bedrock/types.h>

#if defined(__linux__)

#include <bedrock/sync/futex.h>

#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

static long br__futex_syscall(br_futex *futex, i32 op, u32 value, const struct timespec *timeout) {
  return syscall(SYS_futex, (u32 *)futex, op, value, timeout, NULL, 0);
}

static bool br__futex_duration_to_timespec(br_duration duration, struct timespec *ts) {
  if (duration <= 0 || ts == NULL) {
    return false;
  }

  ts->tv_sec = (time_t)(duration / BR_SECOND);
  ts->tv_nsec = (long)(duration % BR_SECOND);
  return true;
}

bool br_futex_wait(br_futex *futex, u32 expected) {
  long rc;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  rc = br__futex_syscall(futex, FUTEX_WAIT_PRIVATE, expected, NULL);
  if (rc == 0) {
    return true;
  }

  /*
  Odin treats interrupted waits and value mismatches as normal wake paths.
  Bedrock reports only real backend failures as `false`.
  */
  return errno == EINTR || errno == EAGAIN;
}

bool br_futex_wait_with_timeout(br_futex *futex, u32 expected, br_duration duration) {
  struct timespec timeout;
  long rc;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }
  if (!br__futex_duration_to_timespec(duration, &timeout)) {
    return false;
  }

  rc = br__futex_syscall(futex, FUTEX_WAIT_PRIVATE, expected, &timeout);
  if (rc == 0) {
    return true;
  }

  if (errno == ETIMEDOUT) {
    return false;
  }
  return errno == EINTR || errno == EAGAIN;
}

void br_futex_signal(br_futex *futex) {
  if (futex == NULL) {
    return;
  }

  BR_UNUSED(br__futex_syscall(futex, FUTEX_WAKE_PRIVATE, 1u, NULL));
}

void br_futex_broadcast(br_futex *futex) {
  if (futex == NULL) {
    return;
  }

  BR_UNUSED(br__futex_syscall(futex, FUTEX_WAKE_PRIVATE, (u32)INT_MAX, NULL));
}

#else
typedef u8 br__sync_futex_linux_translation_unit;
#endif
