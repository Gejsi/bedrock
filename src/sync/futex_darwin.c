#include <bedrock/types.h>

#if defined(__APPLE__) && defined(__MACH__)

#include <bedrock/sync/futex.h>

#include <errno.h>

#define BR__DARWIN_UL_COMPARE_AND_WAIT 1u
#define BR__DARWIN_ULF_WAKE_ALL 0x00000100u
#define BR__DARWIN_ULF_NO_ERRNO 0x01000000u

/*
Odin prefers os_sync_wait_on_address on newer Darwin targets and falls back to
ulock. Bedrock uses the ulock path until a sys/time layer can select the newer
API and expose timeout waits cleanly.
*/
extern int __ulock_wait(u32 operation, void *addr, u64 value, u32 timeout_us);
extern int __ulock_wake(u32 operation, void *addr, u64 wake_value);

bool br_futex_wait(br_futex *futex, u32 expected) {
  int rc;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  rc = __ulock_wait(
    BR__DARWIN_UL_COMPARE_AND_WAIT | BR__DARWIN_ULF_NO_ERRNO, futex, (u64)expected, 0u);
  if (rc >= 0) {
    return true;
  }

  return rc == -EINTR || rc == -EFAULT;
}

void br_futex_signal(br_futex *futex) {
  int rc;

  if (futex == NULL) {
    return;
  }

  do {
    rc = __ulock_wake(BR__DARWIN_UL_COMPARE_AND_WAIT | BR__DARWIN_ULF_NO_ERRNO, futex, 0u);
  } while (rc == -EINTR || rc == -EFAULT);
}

void br_futex_broadcast(br_futex *futex) {
  int rc;

  if (futex == NULL) {
    return;
  }

  do {
    rc = __ulock_wake(BR__DARWIN_UL_COMPARE_AND_WAIT | BR__DARWIN_ULF_NO_ERRNO |
                        BR__DARWIN_ULF_WAKE_ALL,
                      futex,
                      0u);
  } while (rc == -EINTR || rc == -EFAULT);
}

#else
typedef u8 br__sync_futex_darwin_translation_unit;
#endif
