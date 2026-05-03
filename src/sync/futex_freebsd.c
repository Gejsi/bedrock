#include <bedrock/types.h>

#if defined(__FreeBSD__)

#include <bedrock/sync/futex.h>

#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/umtx.h>

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
