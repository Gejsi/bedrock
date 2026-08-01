#ifndef BEDROCK_SYNC_PRIMITIVES_ATOMIC_INTERNAL_H
#define BEDROCK_SYNC_PRIMITIVES_ATOMIC_INTERNAL_H

#include <bedrock/sync/primitives_atomic.h>

typedef void (*br__atomic_sema_wait_hook_fn)(void *context);

/*
Private test seam for forcing contention after a timed waiter observes a
permit. Production callers use br_atomic_sema_wait_with_timeout.
*/
bool br__atomic_sema_wait_with_timeout_hooked(br_atomic_sema *sema,
                                              br_duration duration,
                                              br__atomic_sema_wait_hook_fn before_wait,
                                              br__atomic_sema_wait_hook_fn before_cas,
                                              void *context);

#endif
