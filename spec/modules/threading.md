# Threading

## Status

`thread` is a v1 module: a thin, explicit OS-thread wrapper. It excludes thread
pools, TLS, and cancellation (see Non-Goals). Blocking and coordination live in
`sync`; this module owns thread lifecycle only.

## Design

A thread handle is a small, CALLER-ALLOCATED struct with an explicit lifecycle,
matching every other Bedrock handle (`br_bufio_reader`, `br_virtual_arena`). It
needs no allocator: the OS owns the thread stack, and the handle is a plain
value the caller places on the stack or embeds in their own struct.

```c
typedef int (*br_thread_fn)(void *arg);

typedef struct br_thread {
  /* opaque to consumers. Carries ONLY: the native handle (pthread_t / HANDLE),
     the native thread id (for the self-join guard), and an atomic lifecycle
     state. It carries NO user data and NO exit code. */
  ...
} br_thread;

typedef struct br_thread_options {
  size_t stack_size;   /* 0 = OS default */
  const char *name;    /* best-effort debug name, may be NULL; COPIED at create */
} br_thread_options;

br_status br_thread_create(br_thread *thread, br_thread_fn fn, void *arg);
br_status br_thread_create_ex(br_thread *thread, br_thread_fn fn, void *arg,
                              const br_thread_options *options);
br_status br_thread_join(br_thread *thread, int *exit_code); /* exit_code may be NULL */
br_status br_thread_detach(br_thread *thread);
void br_thread_yield(void);
```

`br_current_thread_id` is NOT redefined here — it lives in `sync/thread.h`.
This module reuses it.

## The core invariant: the spawned thread never touches the handle

After the startup latch (below), the spawned thread holds NO reference to the
`br_thread` — it never reads or writes the handle for its name, its exit code,
its id, or its state. This is what makes a caller-allocated handle safe under
detach: a detached caller may let its handle leave scope while the thread still
runs, so any thread-side access to the handle would be a use-after-free into a
dead frame — at startup (name read), at termination (exit-code write), or
anytime (state/id). The invariant forbids all of them. Three consequences:

1. **Exit codes ride the substrate's native join channel, never the handle.**
   The user `fn` returns an `int`; the backend transports it through the OS join
   mechanism. POSIX: the trampoline returns `(void *)(intptr_t)code` and
   `br_thread_join` recovers it from `pthread_join`'s `retval` and casts back
   (`int`→`intptr_t`→`int` is exact). Windows: the `_beginthreadex` proc returns
   `(unsigned)code` and join recovers it via `GetExitCodeThread` after
   `WaitForSingleObject` (exact for all `int`). The handle never carries the
   code. A detached thread's exit code is unretrievable by design — consistent,
   since nobody can join it. (This is the sign/width-safe answer to storing an
   int through a `void*`: the cast round-trip, not a handle field.)

2. **Startup data uses a creator-stack control block + a create-side
   handshake.** The trampoline needs `{fn, arg, name}`. That block lives on the
   CREATOR's stack inside `br_thread_create`'s frame; `br_thread_create` does not
   return until the spawned thread has copied it into the thread's own locals and
   signaled a one-shot internal semaphore. After create returns, the caller may
   detach and drop the handle freely — the thread already has everything. No
   allocator, no handle involvement, no lifetime hazard. This is a
   create-INTERNAL handshake, not a resurrection of Odin's user-facing
   create/start two-phase gate (which we deleted): we removed the gate that
   existed for context injection, and a hidden handshake reappears purely for
   memory safety. libc's own `pthread_create` solves it the same way — it copies
   its arguments into the new thread's setup before returning.

3. **Identity for the self-join guard is written CREATOR-side, never
   thread-side.** The handle's native id is filled by the creator from the spawn
   call's out-param (`pthread_create`'s `pthread_t*`; `_beginthreadex`'s
   `thrdaddr`), so the guard never needs the thread to write its own id into the
   handle (which on POSIX it would have to, since `gettid` is self-only —
   another side-door use-after-free, avoided).

## Lifecycle state machine

A handle is FRESH (joinable) after a successful create, and moves to a terminal
JOINED or DETACHED. Transitions are atomic (a CAS on an embedded state field),
so concurrent misuse is race-safe and never undefined:

| Call | FRESH | JOINED | DETACHED | zeroed/never-created |
| --- | --- | --- | --- | --- |
| join | joins → JOINED, OK | INVALID_STATE | INVALID_STATE | INVALID_STATE |
| detach | detaches → DETACHED, OK | INVALID_STATE | INVALID_STATE | INVALID_STATE |
| join on SELF | INVALID_ARGUMENT (deadlock avoidance) | — | — | — |

Self-join (a thread joining its own handle) is rejected up front:
`pthread_equal(pthread_self(), thread->native)` on POSIX, `GetCurrentThreadId()
== thread->id` on Windows — both reading only creator-written fields (see
consequence 3 above), no OS join call made. Raw `pthread_join` on self is
`EDEADLK` or a hang, so Bedrock returns `BR_STATUS_INVALID_ARGUMENT`.
Double-join, join-after-detach, and double-detach — all undefined in raw
pthread/Win32 — return `BR_STATUS_INVALID_STATE`. This uniform guarding is the
module's core value over the raw substrates.

## Create-failure contract

On failure `br_thread_create[_ex]` leaves `*thread` inert (as if zero-
initialized) and starts no thread. Status mapping from the substrate:
- resource exhaustion (pthread `EAGAIN`, Windows thread-creation failure) →
  `BR_STATUS_OUT_OF_MEMORY` (foundation.md's status for "the system could not
  provide the resource").
- invalid arguments (NULL `fn`, NULL `thread`) → `BR_STATUS_INVALID_ARGUMENT`.
- a name/stack option the platform cannot honor never fails create — options are
  best-effort (see below).

## Zero-value contract

A zero-initialized `br_thread` is INERT: a defined, unstarted state on which
join/detach return `BR_STATUS_INVALID_STATE`. Unlike a virtual arena, a thread
is NOT zero-value-ready — it has no lazy form, because spawning requires an
explicit OS call. A handle is live only after `br_thread_create` returns OK.

## Exit codes and the entry signature

`br_thread_fn` is `int(*)(void *)` returning an int exit code, matching C11
`thrd_start_t`'s shape and validated against pthread's `void*(*)(void*)` and
Win32's `DWORD(*)(LPVOID)` (int round-trips through both). Richer return values
go through the user's `arg` struct: the caller puts a result field in the struct
it passes, the thread writes it, the caller reads it after join. (A typed handle
return à la Rust's `JoinHandle<T>` needs generics C lacks; the int exit code plus
the arg-struct idiom is the honest C form.)

## Thread naming (best-effort)

`options.name` is a DEBUG AID, never load-bearing, never fails a create, and is
COPIED during the startup handshake (so callers may pass a temporary). It is
applied from INSIDE the entry trampoline as the thread's first act, which
satisfies macOS uniformly (its `pthread_setname_np` names only the calling
thread). Per-OS: Linux truncates to 15 bytes + NUL; macOS caps at 64; Windows
uses `SetThreadDescription`; BSD uses `pthread_set_name_np`. A name that cannot
be set is ignored.

## Backend

POSIX: pthreads (already linked `-pthread`). Windows: `_beginthreadex`, NEVER
`CreateThread`. A general C library and its users call libc on worker threads,
and `CreateThread` skips per-thread CRT initialization — corrupting `errno`,
`strtok`, and locale state and leaking the CRT per-thread block on exit.
`_beginthreadex` initializes that state, returns a joinable/closeable HANDLE, and
runs CRT cleanup. This is an intentional deviation from Odin, whose
`thread_windows.odin` uses `CreateThread` (filed in
`tracking/odin-suspected-bugs.md`); Odin sidesteps the issue with its own
runtime, but a C library cannot. C11 `<threads.h>` is rejected as the substrate:
macOS ships none.

## Non-Goals (v1)

- **No `is_done`** — Odin ships it; Bedrock deliberately refuses it. Polling
  whether a thread finished invites busy-wait loops that should be joins or
  parkers, the result is racy (true says nothing actionable — the thread may
  still be tearing down), and it has no portable implementation (Win32
  timeout-0 wait vs no clean pthread equivalent). Callers who need completion
  signalling join (blocking, correct) or set a `br_atomic` flag in their arg
  struct (explicit, race-clear). Recorded here as an explicit Odin deviation.
- **No thread pools** — a later higher-level module over threads + atomics +
  queues.
- **No TLS** — bring your own via the arg struct.
- **No terminate/cancel** — `pthread_cancel` is a documented footgun (async
  cancellation corrupts held locks and allocator state); cooperative
  cancellation is the caller's job via an atomic flag in the arg.
- **No priority** — an options-struct candidate at most, deferred until a
  consumer needs it.
- **No implicit allocator, temp-allocator, or context propagation** — the Odin
  coupling that kept this module deferred.

## Interaction with sync

Blocking and coordination are `sync`'s job. To block a thread until signaled,
use `br_parker` (`sync/extended.h`); for mutual exclusion, `br_mutex` etc. This
module offers lifecycle only, not coordination, and does not invent a
thread-level wait.

## Porter notes

Headers: `include/bedrock/thread/thread.h` (+ `include/bedrock/thread.h`
umbrella, registered in `include/bedrock.h`). Impl: `src/thread/thread_posix.c`,
`src/thread/thread_windows.c`. The entry trampoline: (1) copies `{fn, arg,
name}` from the creator-stack control block into thread locals, (2) posts the
handshake semaphore, (3) applies the name to self, (4) calls `fn(arg)`, (5)
returns the int through the substrate. The trampoline touches the control block
only in step 1 (before the post) and NEVER touches the `br_thread` handle. The
creator writes the native id/handle into `*thread` from the spawn out-param.
The self-join check runs before the join state transition, so a rejected
self-join leaves the handle FRESH and joinable by another thread. Acceptance: the 24
raw-pthread sites (test_sync 12, test_sync_futex 9, test_virtual_mem/
test_mutex_allocator/test_tracking_allocator 1 each, all currently
`#if !defined(_WIN32)`) convert — thread fns become
`int f(void *){ ...; return 0; }`, create/join calls become `br_thread_*`, the
`_WIN32` guards drop — and run platform-uniform under MODE=thread-sanitize
(join is a happens-before edge on both substrates).
