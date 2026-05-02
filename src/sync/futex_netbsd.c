#include <bedrock/types.h>

#if defined(__NetBSD__)

#include <bedrock/sync/futex.h>

#include <errno.h>
#include <limits.h>
#include <sys/syscall.h>
#include <unistd.h>

#define BR__NETBSD_FUTEX_PRIVATE_FLAG 128
#define BR__NETBSD_FUTEX_WAIT_PRIVATE (0 | BR__NETBSD_FUTEX_PRIVATE_FLAG)
#define BR__NETBSD_FUTEX_WAKE_PRIVATE (1 | BR__NETBSD_FUTEX_PRIVATE_FLAG)

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
