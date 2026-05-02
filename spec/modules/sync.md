# Sync Module Notes

This module exists to bring Bedrock close to Odin's `core/sync`.

The current Bedrock implementation is a useful first slice, but it is not yet a
close implementation-level port. The public surface is reasonably close; the
backend structure is still materially different.

## Current Bedrock State

What is already landed:

- `atomic`
- `Futex` with a Linux backend
- `Atomic_Mutex`
- `Atomic_RW_Mutex`
- `Atomic_Recursive_Mutex`
- `Atomic_Cond`
- `Atomic_Sema`
- `Mutex`
- `RW_Mutex`
- `Recursive_Mutex`
- `Cond`
- guard/lock helper layer
- `Wait_Group`
- `Barrier`
- `Once`
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
- [src/sync/futex_other.c](/home/gejsi/Desktop/bedrock/src/sync/futex_other.c:1)
- [src/sync/primitives.c](/home/gejsi/Desktop/bedrock/src/sync/primitives.c:1)
- [src/sync/primitives_atomic.c](/home/gejsi/Desktop/bedrock/src/sync/primitives_atomic.c:1)
- [src/sync/primitives_posix.c](/home/gejsi/Desktop/bedrock/src/sync/primitives_posix.c:1)
- [src/sync/primitives_windows.c](/home/gejsi/Desktop/bedrock/src/sync/primitives_windows.c:1)
- [src/sync/primitives_other.c](/home/gejsi/Desktop/bedrock/src/sync/primitives_other.c:1)
- [src/sync/extended.c](/home/gejsi/Desktop/bedrock/src/sync/extended.c:1)

Important current divergences from Odin:

1. Mixed public primitive backends

Bedrock now routes the public Linux/futex-target primitives through Bedrock's
atomic/futex layer, so `Mutex`, `RW_Mutex`, `Recursive_Mutex`, and `Cond` are
zero-value-ready on that path.

Other backends still implement the public primitives directly on top of:

- Windows `SRWLOCK`, `CONDITION_VARIABLE`, and `CRITICAL_SECTION`
- non-Linux POSIX `pthread_*`

Odin does not use pthread wrappers for its non-Windows primitives. Its
non-Windows path is built out of atomics plus futex-style wait/wake helpers.

2. Explicit `init`/`destroy`

Odin's sync primitives are designed around a zero-value-ready model. Bedrock
keeps explicit initialization and destruction for API compatibility, but the
Linux/futex public primitive backend is now zero-value-ready. Non-Linux POSIX
and the Windows recursive mutex still require their native initialization
contracts.

3. Extra fallback backend

Bedrock currently has [src/sync/primitives_other.c](/home/gejsi/Desktop/bedrock/src/sync/primitives_other.c:1)
as a generic unsupported-platform stub. Odin does not have a corresponding
`primitives_other.odin`; it enumerates supported backends explicitly.

4. Lower layers still missing after `atomic` / Linux `futex`

Bedrock now has an initial `atomic` layer, native numeric thread IDs, Linux
futex wait/wake, `Atomic_Mutex`, `Atomic_RW_Mutex`,
`Atomic_Recursive_Mutex`, `Atomic_Cond`, `Atomic_Sema`, and a Linux public
primitive bridge to that layer, but it still has no equivalents of:

- `primitives_internal.odin`
- timeout pieces of `primitives_atomic.odin`
- non-Linux `futex_*`

The current Bedrock atomic layer is intentionally C-shaped. It is implemented
over C11 atomics and keeps C's compare-exchange `expected` pointer contract
instead of trying to fake Odin's result-tuple shape at the ABI level.

This is only an abstraction boundary right now, not a universal portability
layer. Targets that do not provide C11 atomics fail at `sync/atomic.h` with an
explicit requirement instead of falling through to backend-specific compiler
errors.

The current futex layer is similarly partial: Linux is implemented with the
kernel futex syscall, while other targets compile through an unsupported stub
until their Odin-equivalent backends are ported.

`Atomic_Recursive_Mutex` intentionally differs from Odin in two ways. It stores
a lower `br_atomic_mutex` directly so the atomic layer remains independent from
the public primitive backend split. Its owner field is atomic to avoid a C data
race when another thread checks ownership while the current owner unlocks.
Bedrock also follows the documented recursive try-lock behavior for same-thread
reentry; the current Odin source appears to call `mutex_try_lock` in that
branch.

That is the main reason the current sync module should be treated as an interim
implementation, not as a parity-complete port.

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
  futex_other.c
  primitives.c
  primitives_internal.c
  primitives_atomic.c
  primitives_windows.c
  primitives_linux.c
  primitives_darwin.c
  primitives_freebsd.c
  primitives_netbsd.c
  primitives_openbsd.c
  primitives_haiku.c
  primitives_wasm.c
  futex_windows.c
  futex_linux.c
  futex_darwin.c
  futex_freebsd.c
  futex_netbsd.c
  futex_openbsd.c
  futex_haiku.c
  futex_wasm.c
  extended.c
```

That does not mean every file must be a literal transliteration, but the
responsibility split should be close.

## Recommended Rewrite Order

1. Remove the illusion that the current backend is Odin-like

- treat the current pthread/Windows implementation as an interim compatibility
  slice
- stop describing it as if the lower sync stack were already ported

2. Add the missing lower sync layers

- keep the landed `atomic` layer and grow around it
- add the remaining futex wrappers
- `primitives_internal`
- finish `primitives_atomic`

3. Split non-Windows primitives by Odin's OS tree

- Linux
- Darwin
- FreeBSD
- NetBSD
- OpenBSD
- later Haiku / WASM if Bedrock wants those targets

4. Revisit zero-value-ready primitives on remaining backends

Linux/futex public primitives now satisfy the zero-value model. The remaining
work is moving the other Odin-supported non-Windows backends off pthread-shaped
wrappers and deciding how much Windows parity is possible around recursive
mutex initialization.

5. Add the next missing public pieces

- timeout waits once `time` exists
- semaphores
- auto-reset events
- benaphores / recursive benaphores
- later `chan`

## Why Sync Before Returning To Mem

The sync rewrite is not a detour. It is the prerequisite for the missing
thread-safe half of `mem`:

- `mutex_allocator`
- mutex in `tracking_allocator`
- mutex in virtual arena

If Bedrock wants `mem` parity, `sync` has to stop being just a wrapper layer.
