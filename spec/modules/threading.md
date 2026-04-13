# Threading

## Status

`core:thread` is not a v1 module.

It should be treated as a later compatibility layer once memory, containers, and
basic IO are stable.

## Why It Is Deferred

Odin's thread package is tightly coupled to features that do not map cleanly to
C:

- implicit context propagation
- automatic temp allocator cleanup rules
- polymorphic helper entry points
- OS-specific backends baked into the same surface

That is a bad place to start.

## What To Preserve

The useful part of Odin's design is not the exact API. It is the idea that a
thread handle should be a small explicit object with clear lifecycle calls.

## Proposed C Direction

When Bedrock grows a thread module, start from a plain ABI:

```c
typedef int (*br_thread_fn)(void *arg);

typedef struct br_thread br_thread;

int br_thread_create(br_thread *out, br_thread_fn fn, void *arg);
int br_thread_join(br_thread *thread, int *exit_code);
int br_thread_detach(br_thread *thread);
void br_thread_yield(void);
```

Important rules:

- no implicit allocator inheritance
- no hidden temp allocator setup/teardown
- no polymorphic helper overloads
- user data is passed explicitly

If callers want richer execution context, they pass a struct pointer.

## Thread Pools

Thread pools are useful, but they are not foundational enough for v1.

If added later, they should likely live in a higher-level module on top of:

- threads
- atomics
- queues
- synchronization primitives

## Interaction With Memory

Per-thread arenas are encouraged, but user-owned.

Bedrock should not silently create one arena model for single-threaded code and
another for multi-threaded code. That kind of implicit policy becomes impossible
to reason about in C.
