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
#include <unistd.h>

static long br__futex_syscall(br_futex *futex, i32 op, u32 value) {
  return syscall(SYS_futex, (u32 *)futex, op, value, NULL, NULL, 0);
}

bool br_futex_wait(br_futex *futex, u32 expected) {
  long rc;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  rc = br__futex_syscall(futex, FUTEX_WAIT_PRIVATE, expected);
  if (rc == 0) {
    return true;
  }

  /*
  Odin treats interrupted waits and value mismatches as normal wake paths.
  Bedrock reports only real backend failures as `false`.
  */
  return errno == EINTR || errno == EAGAIN;
}

void br_futex_signal(br_futex *futex) {
  if (futex == NULL) {
    return;
  }

  BR_UNUSED(br__futex_syscall(futex, FUTEX_WAKE_PRIVATE, 1u));
}

void br_futex_broadcast(br_futex *futex) {
  if (futex == NULL) {
    return;
  }

  BR_UNUSED(br__futex_syscall(futex, FUTEX_WAKE_PRIVATE, (u32)INT_MAX));
}

#else
typedef u8 br__sync_futex_linux_translation_unit;
#endif
