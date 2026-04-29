#include <bedrock/types.h>

#if !defined(__linux__)

#include <bedrock/sync/futex.h>

bool br_futex_wait(br_futex *futex, u32 expected) {
  BR_UNUSED(futex);
  BR_UNUSED(expected);
  return false;
}

void br_futex_signal(br_futex *futex) {
  BR_UNUSED(futex);
}

void br_futex_broadcast(br_futex *futex) {
  BR_UNUSED(futex);
}

#else
typedef u8 br__sync_futex_other_translation_unit;
#endif
