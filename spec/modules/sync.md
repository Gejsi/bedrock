# Sync Module Notes

This module exists to bring Bedrock close to Odin's `core/sync` while still
using the strengths of a C implementation.

## Current Direction

The first slice targets:

- `primitives.odin`
- `sync_util.odin`
- the useful blocking subset of `extended.odin`

Specifically:

- `Mutex`
- `RW_Mutex`
- `Recursive_Mutex`
- `Cond`
- guard/lock helper layer
- `Wait_Group`
- `Barrier`
- `Once`
- `Ticket_Mutex`

## Intentional Bedrock Adaptations

These are deliberate, not accidental:

1. Native OS backends instead of Odin's atomic/futex internals

Bedrock currently implements the public sync surface directly on top of:

- POSIX `pthread_*`
- Windows `SRWLOCK`, `CONDITION_VARIABLE`, and `CRITICAL_SECTION`

This keeps the behavior close while avoiding a premature port of Odin's deeper
atomic/futex stack.

2. Explicit init/destroy for OS-backed primitives

Odin's sync primitives are designed around a zero-value-ready model.
Bedrock currently uses:

- explicit `*_init`
- explicit `*_destroy`
- static-init macros where the platform supports them

This is the main C-facing divergence. It keeps construction/destruction honest
for native OS primitives instead of hiding lazy initialization behind lock
calls.

3. Scoped guard macros instead of function-style guards

Odin's `guard` helpers rely on `defer`.
Bedrock keeps the same intent, but exposes block macros because C has no direct
equivalent:

```c
br_guard(&mutex) {
  /* critical section */
}
```

## Deferred Sync Work

These are still expected later, but not part of the first sync milestone:

- timeout-based wait APIs
- semaphores
- auto-reset events
- benaphores
- Odin's atomic mutex / cond / sema layer
- futex-backed internals
- channels

## Why Sync Before Returning To Mem

The first sync slice directly unlocks the missing `mem` parity work:

- `mutex_allocator`
- mutex in tracking allocator
- mutex in virtual arena

So `sync` is not a detour. It is the clean prerequisite for finishing the
thread-safe half of `mem`.
