#ifndef BEDROCK_SYNC_FUTEX_H
#define BEDROCK_SYNC_FUTEX_H

#include <bedrock/sync/atomic.h>
#include <bedrock/time.h>

BR_EXTERN_C_BEGIN

#if defined(__linux__) || defined(_WIN32) || (defined(__APPLE__) && defined(__MACH__)) ||          \
  defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define BR_SYNC_HAS_FUTEX 1
#else
#define BR_SYNC_HAS_FUTEX 0
#endif

/*
Odin models Futex as a distinct u32. Bedrock keeps the same storage shape, but
uses an atomic u32 so callers can safely load/store the futex word in C.
*/
typedef br_atomic_u32 br_futex;

#define BR_FUTEX_INIT(value) BR_ATOMIC_INIT((u32)(value))

/*
Sleep if the futex contains `expected`. The return value is Bedrock-specific:
Odin asserts backend failures, while C callers need a way to notice unsupported
or failed waits.
*/
bool br_futex_wait(br_futex *futex, u32 expected);
bool br_futex_wait_with_timeout(br_futex *futex, u32 expected, br_duration duration);
void br_futex_signal(br_futex *futex);
void br_futex_broadcast(br_futex *futex);

BR_EXTERN_C_END

#endif
