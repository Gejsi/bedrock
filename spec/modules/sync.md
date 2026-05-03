# Sync Module Notes

This module exists to bring Bedrock close to Odin's `core/sync`.

The current Bedrock implementation is a useful first slice, but it is not yet a
close implementation-level port. The public surface is reasonably close; the
backend structure is still materially different.

## Current Bedrock State

What is already landed:

- `atomic`
- `Futex` with Linux, Windows, Darwin, FreeBSD, NetBSD, and OpenBSD basic backends
- `Atomic_Mutex`
- `Atomic_RW_Mutex`
- `Atomic_Recursive_Mutex`
- `Atomic_Cond`
- `Atomic_Sema`
- futex, atomic condition, atomic semaphore, public condition, public
  semaphore, and wait-group timeout waits
- `Mutex`
- `RW_Mutex`
- `Recursive_Mutex`
- `Cond`
- `Sema`
- guard/lock helper layer
- `Wait_Group`
- `Barrier`
- `Once`
- `Auto_Reset_Event`
- `Ticket_Mutex`

Current implementation shape:

- [include/bedrock/sync/atomic.h](/home/gejsi/Desktop/bedrock/include/bedrock/sync/atomic.h:1)
- [include/bedrock/sync/futex.h](/home/gejsi/Desktop/bedrock/include/bedrock/sync/futex.h:1)
- [include/bedrock/sync/primitives.h](/home/gejsi/Desktop/bedrock/include/bedrock/sync/primitives.h:1)
- [include/bedrock/sync/primitives_atomic.h](/home/gejsi/Desktop/bedrock/include/bedrock/sync/primitives_atomic.h:1)
- [include/bedrock/sync/thread.h](/home/gejsi/Desktop/bedrock/include/bedrock/sync/thread.h:1)
- [include/bedrock/sync/extended.h](/home/gejsi/Desktop/bedrock/include/bedrock/sync/extended.h:1)
- [include/bedrock/sync/sync_util.h](/home/gejsi/Desktop/bedrock/include/bedrock/sync/sync_util.h:1)
- [src/sync/atomic.c](/home/gejsi/Desktop/bedrock/src/sync/atomic.c:1)
- [src/sync/futex_linux.c](/home/gejsi/Desktop/bedrock/src/sync/futex_linux.c:1)
- [src/sync/futex_windows.c](/home/gejsi/Desktop/bedrock/src/sync/futex_windows.c:1)
- [src/sync/futex_darwin.c](/home/gejsi/Desktop/bedrock/src/sync/futex_darwin.c:1)
- [src/sync/futex_freebsd.c](/home/gejsi/Desktop/bedrock/src/sync/futex_freebsd.c:1)
- [src/sync/futex_netbsd.c](/home/gejsi/Desktop/bedrock/src/sync/futex_netbsd.c:1)
- [src/sync/futex_openbsd.c](/home/gejsi/Desktop/bedrock/src/sync/futex_openbsd.c:1)
- [src/sync/futex_other.c](/home/gejsi/Desktop/bedrock/src/sync/futex_other.c:1)
- [src/sync/primitives.c](/home/gejsi/Desktop/bedrock/src/sync/primitives.c:1)
- [src/sync/primitives_atomic.c](/home/gejsi/Desktop/bedrock/src/sync/primitives_atomic.c:1)
- [src/sync/primitives_internal.c](/home/gejsi/Desktop/bedrock/src/sync/primitives_internal.c:1)
- [src/sync/extended.c](/home/gejsi/Desktop/bedrock/src/sync/extended.c:1)

Important current divergences from Odin:

1. C-shaped public wrappers

Bedrock's public `Mutex`, `RW_Mutex`, `Recursive_Mutex`, `Cond`, and `Sema`
now store zero-value-ready atomic/futex-backed words instead of native
`pthread`/Windows objects. This matches Odin's zero-value model much more
closely.

The wrapper API is still C-shaped: Bedrock keeps explicit `init`/`destroy`
functions as reset/no-op compatibility helpers, while Odin exposes the
zero-value contract directly through package procedures.

2. Backend coverage is still not parity-complete

The shared atomic/futex primitive layer is now the only public primitive
backend. Basic wait/wake source ports exist for Linux, Windows, Darwin,
FreeBSD, NetBSD, and OpenBSD. Only the Linux path has been exercised locally in
this repository. Haiku and WASM futex backends are still missing.

3. Timeout pieces are now wired through the main primitive stack

Bedrock now has the minimal `time` module foundation and the Odin-shaped
timeout variants for futex, atomic condition variables, atomic semaphores,
public condition variables, public semaphores, and wait groups.

Intentional deviation: Odin's current `wait_group_wait_with_timeout` passes the
full duration into each condition-wait loop iteration. Bedrock tracks remaining
duration so spurious wakeups do not restart the public wait-group timeout
window.

4. Lower layers still missing after `atomic` / futex

Bedrock now has an initial `atomic` layer, native numeric thread IDs,
`Atomic_Mutex`, `Atomic_RW_Mutex`, `Atomic_Recursive_Mutex`, `Atomic_Cond`,
`Atomic_Sema`, public primitive wrappers, and several futex backends, but it
still has no equivalents of:

- Haiku / WASM `futex_*`
- Odin's exact per-OS `primitives_*` split for thread ID functions

The current Bedrock atomic layer is intentionally C-shaped. It is implemented
over C11 atomics and keeps C's compare-exchange `expected` pointer contract
instead of trying to fake Odin's result-tuple shape at the ABI level.

This is only an abstraction boundary right now, not a universal portability
layer. Targets that do not provide C11 atomics fail at `sync/atomic.h` with an
explicit requirement instead of falling through to backend-specific compiler
errors.

The current futex layer is still partial: the non-Linux backends are source
ports from Odin's strategy and need target-specific verification.

`Atomic_Recursive_Mutex` intentionally differs from Odin in two ways. It stores
a lower `br_atomic_mutex` directly so the atomic layer remains independent from
the public primitive backend split. Its owner field is atomic to avoid a C data
race when another thread checks ownership while the current owner unlocks.
Bedrock also follows the documented recursive try-lock behavior for same-thread
reentry; the current Odin source appears to call `mutex_try_lock` in that
branch.

That is the main reason the current sync module should still be treated as a
partial port, not as parity-complete.

## Odin File Map

The real comparison target is Odin's actual file tree:

- public surface:
  - `primitives.odin`
  - `sync_util.odin`
  - `extended.odin`
- lower layers:
  - `atomic.odin`
  - `primitives_internal.odin`
  - `primitives_atomic.odin`
- per-OS primitives:
  - `primitives_windows.odin`
  - `primitives_linux.odin`
  - `primitives_darwin.odin`
  - `primitives_freebsd.odin`
  - `primitives_netbsd.odin`
  - `primitives_openbsd.odin`
  - `primitives_haiku.odin`
  - `primitives_wasm.odin`
- futex backends:
  - `futex_windows.odin`
  - `futex_linux.odin`
  - `futex_darwin.odin`
  - `futex_freebsd.odin`
  - `futex_netbsd.odin`
  - `futex_openbsd.odin`
  - `futex_haiku.odin`
  - `futex_wasm.odin`
- later surface:
  - `chan/chan.odin`

Bedrock should eventually look closer to this shape:

```text
include/bedrock/sync/
  atomic.h
  primitives.h
  extended.h
  sync_util.h

src/sync/
  atomic.c
  futex_linux.c
  primitives.c
  primitives_internal.c
  primitives_atomic.c
  futex_windows.c
  futex_linux.c
  futex_darwin.c
  futex_freebsd.c
  futex_netbsd.c
  futex_openbsd.c
  futex_haiku.c
  futex_wasm.c
  futex_other.c
  extended.c
```

That does not mean every file must be a literal transliteration, but the
responsibility split should be close.

## Recommended Rewrite Order

1. Verify and finish the remaining backend layer

- target-test Windows, Darwin, FreeBSD, NetBSD, and OpenBSD futex backends
- add Haiku / WASM only if Bedrock wants those targets
- keep unsupported targets as compile-time failures instead of spinning on a
  fake futex

2. Split thread ID primitives by Odin's OS tree

- Linux
- Darwin
- FreeBSD
- NetBSD
- OpenBSD
- later Haiku / WASM if Bedrock wants those targets

3. Add the next missing public pieces

- benaphores / recursive benaphores
- parker / one-shot events
- later `chan`

## Why Sync Before Returning To Mem

The sync rewrite is not a detour. It is the prerequisite for the missing
thread-safe half of `mem`:

- `mutex_allocator`
- mutex in `tracking_allocator`
- mutex in virtual arena

If Bedrock wants `mem` parity, `sync` has to stop being just a wrapper layer.
