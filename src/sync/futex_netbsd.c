#include <bedrock/types.h>

#if defined(__NetBSD__)

#include <bedrock/sync/futex.h>

#include <errno.h>
#include <limits.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define BR__NETBSD_FUTEX_PRIVATE_FLAG 128
#define BR__NETBSD_FUTEX_WAIT_PRIVATE (0 | BR__NETBSD_FUTEX_PRIVATE_FLAG)
#define BR__NETBSD_FUTEX_WAKE_PRIVATE (1 | BR__NETBSD_FUTEX_PRIVATE_FLAG)

static bool
br__futex_duration_to_timespec(br_duration duration, struct timespec *timeout, br_duration *chunk) {
  const br_duration max_chunk = (br_duration)INT32_MAX * BR_SECOND;

  if (duration <= 0 || timeout == NULL) {
    return false;
  }

  *chunk = duration < max_chunk ? duration : max_chunk;
  timeout->tv_sec = (time_t)(*chunk / BR_SECOND);
  timeout->tv_nsec = (long)(*chunk % BR_SECOND);
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

  rc = syscall(SYS___futex, futex, BR__NETBSD_FUTEX_WAIT_PRIVATE, expected, NULL, NULL, 0);
  if (rc == 0) {
    return true;
  }

  return errno == EINTR || errno == EAGAIN;
}

bool br_futex_wait_with_timeout(br_futex *futex, u32 expected, br_duration duration) {
  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }
  for (;;) {
    struct timespec timeout;
    br_duration chunk;
    long rc;

    if (!br__futex_duration_to_timespec(duration, &timeout, &chunk)) {
      return false;
    }

    rc = syscall(SYS___futex, futex, BR__NETBSD_FUTEX_WAIT_PRIVATE, expected, &timeout, NULL, 0);
    if (rc == 0) {
      return true;
    }
    if (errno != ETIMEDOUT) {
      return errno == EINTR || errno == EAGAIN;
    }
    if (duration <= chunk) {
      return false;
    }
    duration -= chunk;
    if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
      return true;
    }
  }
}

void br_futex_signal(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  BR_UNUSED(syscall(SYS___futex, futex, BR__NETBSD_FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0));
}

void br_futex_broadcast(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  BR_UNUSED(syscall(SYS___futex, futex, BR__NETBSD_FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0));
}

#else
typedef u8 br__sync_futex_netbsd_translation_unit;
#endif
