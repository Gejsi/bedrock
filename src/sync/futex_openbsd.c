#include <bedrock/types.h>

#if defined(__OpenBSD__)

#include <bedrock/sync/futex.h>

#include <errno.h>
#include <limits.h>
#include <time.h>

#define BR__OPENBSD_FUTEX_WAIT 1
#define BR__OPENBSD_FUTEX_WAKE 2
#define BR__OPENBSD_FUTEX_PRIVATE_FLAG 128
#define BR__OPENBSD_FUTEX_WAIT_PRIVATE (BR__OPENBSD_FUTEX_WAIT | BR__OPENBSD_FUTEX_PRIVATE_FLAG)
#define BR__OPENBSD_FUTEX_WAKE_PRIVATE (BR__OPENBSD_FUTEX_WAKE | BR__OPENBSD_FUTEX_PRIVATE_FLAG)

extern int futex(br_futex *futex, int op, u32 value, const void *timeout);

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

bool br_futex_wait(br_futex *futex_word, u32 expected) {
  int rc;

  if (futex_word == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex_word, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  rc = futex(futex_word, BR__OPENBSD_FUTEX_WAIT_PRIVATE, expected, NULL);
  if (rc != -1) {
    return true;
  }

  return errno == EINTR || errno == EAGAIN || errno == ETIMEDOUT;
}

bool br_futex_wait_with_timeout(br_futex *futex_word, u32 expected, br_duration duration) {
  if (futex_word == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex_word, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }
  for (;;) {
    struct timespec timeout;
    br_duration chunk;
    int rc;

    if (!br__futex_duration_to_timespec(duration, &timeout, &chunk)) {
      return false;
    }

    rc = futex(futex_word, BR__OPENBSD_FUTEX_WAIT_PRIVATE, expected, &timeout);
    if (rc != -1) {
      return true;
    }
    if (errno != ETIMEDOUT) {
      return errno == EINTR || errno == EAGAIN;
    }
    if (duration <= chunk) {
      return false;
    }
    duration -= chunk;
    if (br_atomic_load_explicit(futex_word, BR_ATOMIC_ACQUIRE) != expected) {
      return true;
    }
  }
}

void br_futex_signal(br_futex *futex_word) {
  if (futex_word == NULL) {
    return;
  }
  BR_UNUSED(futex(futex_word, BR__OPENBSD_FUTEX_WAKE_PRIVATE, 1u, NULL));
}

void br_futex_broadcast(br_futex *futex_word) {
  if (futex_word == NULL) {
    return;
  }
  BR_UNUSED(futex(futex_word, BR__OPENBSD_FUTEX_WAKE_PRIVATE, (u32)INT_MAX, NULL));
}

#else
typedef u8 br__sync_futex_openbsd_translation_unit;
#endif
