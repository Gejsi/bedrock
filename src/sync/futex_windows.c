#include <bedrock/types.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#include <bedrock/sync/futex.h>

#include <windows.h>

bool br_futex_wait(br_futex *futex, u32 expected) {
  u32 compare;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  compare = expected;
  /*
  Odin wraps RtlWaitOnAddress to preserve Windows error reporting. Bedrock's
  no-timeout v1 only needs the sleep/wake behavior, so the public
  WaitOnAddress API is enough here.
  */
  return WaitOnAddress((volatile void *)futex, &compare, sizeof(compare), INFINITE) != 0;
}

void br_futex_signal(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  WakeByAddressSingle((void *)futex);
}

void br_futex_broadcast(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  WakeByAddressAll((void *)futex);
}

#else
typedef u8 br__sync_futex_windows_translation_unit;
#endif
