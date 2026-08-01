#include <bedrock/types.h>

#if defined(__FreeBSD__)

#include <bedrock/sync/futex.h>

#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/umtx.h>
#include <time.h>

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
  int rc;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  /*
  Odin simulates an infinite wait with a 4-hour timeout loop. Bedrock uses
  FreeBSD's native no-timeout form: the _umtx_op timeout arguments are optional,
  and timeout behavior is only requested when uaddr/uaddr2 describe a timespec.
  */
  rc = _umtx_op(futex, UMTX_OP_WAIT_UINT, (unsigned long)expected, NULL, NULL);
  if (rc == 0) {
    return true;
  }

  return errno == EINTR || errno == EAGAIN || errno == EBUSY;
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
    void *timeout_size;
    int rc;

    if (!br__futex_duration_to_timespec(duration, &timeout, &chunk)) {
      return false;
    }

    timeout_size = (void *)(uptr)sizeof(timeout);
    rc = _umtx_op(futex, UMTX_OP_WAIT_UINT, (unsigned long)expected, timeout_size, &timeout);
    if (rc == 0) {
      return true;
    }
    if (errno != ETIMEDOUT) {
      return errno == EINTR || errno == EAGAIN || errno == EBUSY;
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
  BR_UNUSED(_umtx_op(futex, UMTX_OP_WAKE, 1ul, NULL, NULL));
}

void br_futex_broadcast(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  BR_UNUSED(_umtx_op(futex, UMTX_OP_WAKE, (unsigned long)INT_MAX, NULL, NULL));
}

#else
typedef u8 br__sync_futex_freebsd_translation_unit;
#endif
