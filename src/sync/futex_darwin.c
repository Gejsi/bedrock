#include <bedrock/types.h>

#if defined(__APPLE__) && defined(__MACH__)

#include <bedrock/sync/futex.h>

#include <errno.h>

#define BR__DARWIN_OS_SYNC_FLAGS 0u
#define BR__DARWIN_UL_COMPARE_AND_WAIT 1u
#define BR__DARWIN_ULF_WAKE_ALL 0x00000100u
#define BR__DARWIN_ULF_NO_ERRNO 0x01000000u

#if defined(__has_attribute)
#if __has_attribute(weak_import)
#define BR__DARWIN_WEAK_IMPORT __attribute__((weak_import))
#endif
#endif
#ifndef BR__DARWIN_WEAK_IMPORT
#define BR__DARWIN_WEAK_IMPORT
#endif

extern i32
os_sync_wait_on_address(void *addr, u64 value, usize size, u32 flags) BR__DARWIN_WEAK_IMPORT;
extern i32 os_sync_wake_by_address_any(void *addr, usize size, u32 flags) BR__DARWIN_WEAK_IMPORT;
extern i32 os_sync_wake_by_address_all(void *addr, usize size, u32 flags) BR__DARWIN_WEAK_IMPORT;
extern i32 __ulock_wait2(u32 operation, void *addr, u64 value, u64 timeout_ns, u64 value2)
  BR__DARWIN_WEAK_IMPORT;
extern i32
__ulock_wait(u32 operation, void *addr, u64 value, u32 timeout_us) BR__DARWIN_WEAK_IMPORT;
extern i32 __ulock_wake(u32 operation, void *addr, u64 wake_value) BR__DARWIN_WEAK_IMPORT;

static bool br__darwin_os_wait(br_futex *futex, u32 expected, bool *handled) {
  i32 rc;

  if (os_sync_wait_on_address == NULL) {
    *handled = false;
    return false;
  }

  *handled = true;
  rc = os_sync_wait_on_address(futex, (u64)expected, sizeof(*futex), BR__DARWIN_OS_SYNC_FLAGS);
  if (rc >= 0) {
    return true;
  }

  return errno == EINTR || errno == EFAULT;
}

static bool br__darwin_ulock_wait(br_futex *futex, u32 expected) {
  i32 rc;

  if (__ulock_wait2 != NULL) {
    rc = __ulock_wait2(
      BR__DARWIN_UL_COMPARE_AND_WAIT | BR__DARWIN_ULF_NO_ERRNO, futex, (u64)expected, 0u, 0u);
  } else {
    if (__ulock_wait == NULL) {
      return false;
    }
    rc = __ulock_wait(
      BR__DARWIN_UL_COMPARE_AND_WAIT | BR__DARWIN_ULF_NO_ERRNO, futex, (u64)expected, 0u);
  }

  if (rc >= 0) {
    return true;
  }

  return rc == -EINTR || rc == -EFAULT;
}

static bool br__darwin_os_wake(br_futex *futex, bool broadcast, bool *handled) {
  i32 rc;

  if ((!broadcast && os_sync_wake_by_address_any == NULL) ||
      (broadcast && os_sync_wake_by_address_all == NULL)) {
    *handled = false;
    return false;
  }

  *handled = true;
  do {
    if (broadcast) {
      rc = os_sync_wake_by_address_all(futex, sizeof(*futex), BR__DARWIN_OS_SYNC_FLAGS);
    } else {
      rc = os_sync_wake_by_address_any(futex, sizeof(*futex), BR__DARWIN_OS_SYNC_FLAGS);
    }
    if (rc >= 0 || errno == ENOENT) {
      return true;
    }
  } while (errno == EINTR || errno == EFAULT);

  return false;
}

static void br__darwin_ulock_wake(br_futex *futex, bool broadcast) {
  u32 operation;
  i32 rc;

  if (__ulock_wake == NULL) {
    return;
  }

  operation = BR__DARWIN_UL_COMPARE_AND_WAIT | BR__DARWIN_ULF_NO_ERRNO;
  if (broadcast) {
    operation |= BR__DARWIN_ULF_WAKE_ALL;
  }

  do {
    rc = __ulock_wake(operation, futex, 0u);
  } while (rc == -EINTR || rc == -EFAULT);
}

bool br_futex_wait(br_futex *futex, u32 expected) {
  bool handled;
  bool ok;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  /*
  Match Odin's preference order while avoiding hard minimum-OS or SDK debt:
  Apple's newer os_sync_* APIs first, then __ulock_wait2, then __ulock_wait.
  Timeout support will use the parallel timeout symbols once Bedrock has time.
  */
  handled = false;
  ok = br__darwin_os_wait(futex, expected, &handled);
  if (handled) {
    return ok;
  }

  return br__darwin_ulock_wait(futex, expected);
}

void br_futex_signal(br_futex *futex) {
  bool handled;

  if (futex == NULL) {
    return;
  }

  handled = false;
  if (br__darwin_os_wake(futex, false, &handled) || handled) {
    return;
  }

  br__darwin_ulock_wake(futex, false);
}

void br_futex_broadcast(br_futex *futex) {
  bool handled;

  if (futex == NULL) {
    return;
  }

  handled = false;
  if (br__darwin_os_wake(futex, true, &handled) || handled) {
    return;
  }

  br__darwin_ulock_wake(futex, true);
}

#else
typedef u8 br__sync_futex_darwin_translation_unit;
#endif
