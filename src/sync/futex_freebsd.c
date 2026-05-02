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
  Odin simulates an infinite wait with a long timeout loop. Bedrock uses the
  kernel's no-timeout form for the no-timeout v1 surface.
  */
  rc = _umtx_op(futex, UMTX_OP_WAIT_UINT, (unsigned long)expected, NULL, NULL);
  if (rc == 0) {
    return true;
  }

  return errno == EINTR || errno == EAGAIN;
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
