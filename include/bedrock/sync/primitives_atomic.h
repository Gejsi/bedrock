#ifndef BEDROCK_SYNC_PRIMITIVES_ATOMIC_H
#define BEDROCK_SYNC_PRIMITIVES_ATOMIC_H

#include <bedrock/sync/futex.h>

BR_EXTERN_C_BEGIN

/*
First slice of Odin's `primitives_atomic.odin`.

Timeout waits still depend on a Bedrock `time` module, so this starts with the
non-timeout semaphore primitive needed by the rest of the atomic sync stack.
*/
typedef struct br_atomic_sema {
  br_futex count;
} br_atomic_sema;

#define BR_ATOMIC_SEMA_INIT(count) {.count = BR_FUTEX_INIT(count)}

void br_atomic_sema_init(br_atomic_sema *sema, u32 count);
void br_atomic_sema_post(br_atomic_sema *sema, u32 count);
void br_atomic_sema_wait(br_atomic_sema *sema);

BR_EXTERN_C_END

#endif
