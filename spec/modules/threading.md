# Threading

## Status

`thread` is a thin OS-thread lifecycle module. Blocking and coordination live
in `sync`; this module owns creation, joining, detaching, naming, stack sizing,
and yielding.

## Public shape

```c
typedef int (*br_thread_fn)(void *arg);

typedef struct br_thread {
  /* Opaque implementation fields. Do not inspect or copy a live handle. */
  br_atomic_u32 state;
  void *control;
} br_thread;

#define BR_THREAD_INIT ...

typedef struct br_thread_options {
  size_t stack_size; /* 0 = OS default */
  const char *name;  /* best-effort debug name, copied at create */
} br_thread_options;

br_status br_thread_create(br_thread *thread, br_thread_fn fn, void *arg);
br_status br_thread_create_ex(br_thread *thread, br_thread_fn fn, void *arg,
                              const br_thread_options *options);
br_status br_thread_join(br_thread *thread, int *exit_code);
br_status br_thread_detach(br_thread *thread);
void br_thread_yield(void);
```

The handle is caller-owned and may live on the stack or inside another object.
It is platform-independent: native pthread and Windows handles stay in an
internal control block. A live handle must not be copied or passed to another
create call. A terminal joined or detached handle may be initialized again by
`br_thread_create`.

Handle storage must outlive every operation using it. Concurrent join/detach is
race-safe, but a losing caller can return before the winning caller has
finished. The creator must therefore wait for all contenders before dropping or
reusing the handle.

## Ownership model

The native APIs do not copy a caller's launch object. `pthread_create` and
`_beginthreadex` copy the pointer value and later pass that same pointer to the
new thread. Therefore, launch storage must outlive the handoff.

Bedrock allocates one fixed-size control block from its process-heap allocator.
It contains:

- the native handle
- `{fn, arg}` and the copied debug name
- the exact `int` result
- a two-owner atomic reference count

The handle owns one reference and the worker owns one reference. Native
creation failure leaves ownership with the creator, which frees the block.
After successful creation:

- join waits for the worker, reads the result, closes or consumes the native
  handle, and releases the handle reference
- detach transfers native cleanup to the OS and releases the handle reference
- the worker releases its reference when the callback finishes
- whichever side releases the final reference frees the block

The worker never receives a pointer to the public `br_thread`. Consequently, a
successful detach may be followed immediately by dropping or reusing the
public handle while the worker continues.

The internal process-heap allocation is intentional. Passing a user allocator
would require that allocator to remain alive and support cross-thread freeing.
The allocation is small compared with the native thread and stack, and thread
pools amortize it for workloads that submit frequent short tasks.

## Lifecycle

The public atomic state follows this internal state machine:

```text
INERT -> FRESH
FRESH -> JOINING -> JOINED
FRESH -> DETACHING -> DETACHED
JOINING -> FRESH   (self-join or native failure)
DETACHING -> FRESH (native failure)
```

Join and detach first claim the FRESH handle with an atomic compare-exchange.
Only the winner accesses the control block or native handle. Concurrent losers
return `BR_STATUS_INVALID_STATE`; they cannot race with native handle
destruction.

A POSIX `pthread_join` failure, `pthread_detach` failure, Windows wait failure,
or Windows close failure restores FRESH and returns
`BR_STATUS_INVALID_STATE`. Terminal states are published only after the native
operation succeeds. POSIX also installs a cleanup handler around the blocking
join so default deferred cancellation of the joining thread restores FRESH.
Asynchronous pthread cancellation is outside the contract; Bedrock neither
enables nor exposes forced cancellation.

## Self-join identity

Join claims JOINING before checking identity, so detach can no longer invalidate
the target concurrently. POSIX can then safely compare `pthread_t` values with
`pthread_equal`. Windows workers record their control-block pointer in internal
C thread-local storage and compare that stable token.

This avoids two native-identity hazards:

- POSIX detach cannot invalidate a `pthread_t` before `pthread_equal`
- Windows numeric thread IDs can be reused after a thread terminates even while
  its HANDLE remains joinable

A rejected self-join restores FRESH and returns
`BR_STATUS_INVALID_ARGUMENT`, leaving another thread able to join or detach.

## Results

The callback's `int` result is stored directly in the control block. Native
join or wait provides the completion synchronization before
`br_thread_join` reads it. This preserves every `int` value without relying on
POSIX integer-to-pointer or Windows unsigned-to-signed conversions.

Richer results travel through the caller's `arg` object. The worker writes the
object and the caller reads it after a successful join.

The callback must return normally. Calling `pthread_exit`, `_endthreadex`,
`ExitThread`, or an equivalent native escape bypasses Bedrock's result contract
and is unsupported.

## Creation and options

If `thread` is non-NULL, create initializes it to INERT before validation.
Creation can fail with:

- `BR_STATUS_INVALID_ARGUMENT` for a NULL handle, NULL callback, or invalid
  native creation arguments
- `BR_STATUS_OUT_OF_MEMORY` when the control block or native thread resources
  cannot be allocated
- `BR_STATUS_OUT_OF_RANGE` when a requested stack size cannot be represented
  or accepted by the native API
- `BR_STATUS_INVALID_STATE` for another unexpected native creation failure

Thread names are copied into the control block and applied by the worker before
the callback. Naming is only a debugging aid and remains best-effort: Linux
truncates to 15 bytes plus NUL, macOS caps names at 64 bytes, Windows uses
`SetThreadDescription`, and BSD uses its pthread naming API.

Stack size is not best-effort. A nonzero request is either installed or create
fails. Silently using the default stack after the caller requested another size
would make memory and overflow assumptions unreliable. POSIX installs the value
with `pthread_attr_setstacksize`; Windows passes it to `_beginthreadex` with
`STACK_SIZE_PARAM_IS_A_RESERVATION`.

The callback may begin before create returns, matching the native APIs. A
callback that receives access to its own public handle must synchronize with the
creator before using it.

## Zero and reuse

`BR_THREAD_INIT` creates an inert handle. An ordinary output variable need not
be initialized before a valid create call because create uses `atomic_init`.
Join and detach require either an initialized inert handle or a handle produced
by create; reading an uninitialized atomic object is not portable C.

A handle can be reused after successful join or detach. It must not be reused
while FRESH, JOINING, or DETACHING.

## Backends

Linux, macOS, FreeBSD, OpenBSD, and NetBSD use pthreads. Windows uses
`_beginthreadex`, not `CreateThread`, because threads running C code require
Microsoft CRT per-thread initialization and cleanup. C11 `<threads.h>` is not
the substrate because it is unavailable on some supported systems, including
macOS.

Linux and macOS run the full debug, release, ASan/UBSan, and TSan CI matrix.
Windows runs debug and release CI jobs. The FreeBSD, OpenBSD, and NetBSD
branches are implemented against their native pthread declarations but still
require target CI before Bedrock can claim the same verification level for
them. In particular, FreeBSD and OpenBSD expose thread naming through
`<pthread_np.h>`, while NetBSD declares its differently shaped
`pthread_setname_np` in `<pthread.h>`.

## Deliberate exclusions

- No forced termination. Asynchronous `pthread_cancel` and `TerminateThread`
  can abandon locks, allocators, TLS, and application invariants.
- No ambient allocator or Odin runtime-context propagation.
- No public create/start split. Odin needs that gate to configure language
  runtime context before execution; Bedrock options are complete at create.
- No priority until a real caller can define portable fallback and permission
  semantics.
- No `is_done` yet. Completion polling needs a concrete consumer and contract.
- No pool inside this primitive module. A pool is a separate executor over
  threads, synchronization, and a task queue.

## Odin comparison

Odin's `Thread` is heap-allocated and carries procedure data, user arguments,
allocator ownership, runtime context, temporary-allocator cleanup, state, and
native synchronization. It supports suspended create/start, priority,
`is_done`, forced termination, convenience run helpers, and a task pool.

Bedrock keeps only the parts that form a sound general C lifecycle API:
caller-owned handles, immediate creation, join, detach, exact integer results,
name, stack size, and yield. The control block solves native lifetime problems
without exposing Odin's runtime context or allocator conventions.

The Odin thread pool remains useful source material, but it is not part of the
primitive thread contract and should not be ported verbatim.
