#Odin Coverage Checklist

This file makes the status of the current Odin-inspired work explicit instead of
describing it with vague labels.

The comparison target is the Odin checkout currently pinned in
`upstream/odin/` on April 13, 2026.

## Status Legend

| Status | Meaning |
| --- | --- |
| `done` | Implemented in Bedrock with roughly matching behavior. |
| `adapted` | Implemented, but intentionally not 1:1 because the C surface or scope is different. |
| `planned` | In scope for the current Bedrock direction, but not implemented yet. |
| `deferred` | Explicitly pushed later because it depends on bigger architecture or platform work. |
| `excluded` | Not part of the current Bedrock target. |

## `core/mem`

Current label: `partial v1`

Why this label:
- Bedrock now has the allocator contract, the first fixed, scratch, stack,
  small-stack, and dynamic arenas/allocators, and the first cross-platform
  virtual-memory-backed arena implementation.
- Odin `core/mem` is still much broader and includes specialized allocators,
  synchronized wrappers, and TLSF work that Bedrock has not ported yet.

Current Bedrock files:
- `include/bedrock/mem/alloc.h`
- `include/bedrock/mem/arena.h`
- `include/bedrock/mem/buddy_allocator.h`
- `include/bedrock/mem/compat_allocator.h`
- `include/bedrock/mem/dynamic_arena.h`
- `include/bedrock/mem/rollback_stack.h`
- `include/bedrock/mem/scratch.h`
- `include/bedrock/mem/small_stack.h`
- `include/bedrock/mem/stack.h`
- `include/bedrock/mem/tracking_allocator.h`
- `include/bedrock/mem/virtual.h`
- `include/bedrock/mem/virtual_arena.h`
- `include/bedrock/mem/virtual_arena_util.h`
- `src/mem/alloc.c`
- `src/mem/arena.c`
- `src/mem/buddy_allocator.c`
- `src/mem/compat_allocator.c`
- `src/mem/dynamic_arena.c`
- `src/mem/rollback_stack.c`
- `src/mem/scratch.c`
- `src/mem/small_stack.c`
- `src/mem/stack.c`
- `src/mem/tracking_allocator.c`
- `src/mem/virtual/common.c`
- `src/mem/virtual/virtual.c`
- `src/mem/virtual/virtual_platform.c`
- `src/mem/virtual/virtual_posix.c`
- `src/mem/virtual/virtual_windows.c`
- `src/mem/virtual/virtual_linux.c`
- `src/mem/virtual/virtual_darwin.c`
- `src/mem/virtual/virtual_freebsd.c`
- `src/mem/virtual/virtual_netbsd.c`
- `src/mem/virtual/virtual_openbsd.c`
- `src/mem/virtual/virtual_other.c`
- `src/mem/virtual/file.c`
- `src/mem/virtual/arena.c`
- `src/mem/virtual/arena_util.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| allocator dispatch model | `adapted` | `alloc.h`, `alloc.c` | Single allocator object with request/response dispatch, adapted to C. |
| basic heap allocator | `done` | `alloc.c` | Heap allocator works and is used by current modules. |
| null allocator | `done` | `alloc.c` | Implemented. |
| fail allocator | `done` | `alloc.c` | Implemented. |
| fixed-buffer arena | `adapted` | `arena.h`, `arena.c` | Implemented as the first arena shape. |
| arena mark / rewind | `done` | `arena.h`, `arena.c` | Implemented. |
| scratch allocator | `adapted` | `scratch.h`, `scratch.c` | Landed with lazy default initialization, backup allocations, and last-allocation free/resize behavior close to Odin; Bedrock omits Odin's context logger warning path and uses explicit status returns. |
| stack allocator | `adapted` | `stack.h`, `stack.c` | Landed with Odin-style buffered stack allocation, last-allocation free/resize, and double-free tolerance; Bedrock returns statuses instead of panics and documents the in-place resize check that differs from Odin's current source. |
| small stack allocator | `adapted` | `small_stack.h`, `small_stack.c` | Landed with Odin-style tiny headers, out-of-order free semantics, and overwrite-on-reuse behavior; Bedrock returns statuses instead of panics and still enforces Bedrock's power-of-two alignment contract after Odin-style clamping. |
| dynamic arena allocator | `adapted` | `dynamic_arena.h`, `dynamic_arena.c` | Landed with Odin-style block cycling, separate block/array allocators, out-band allocations, reset/free-all split, and reallocate-on-resize behavior; Bedrock keeps the generic allocator adapter on alloc/free/resize/reset only, so there are no query-feature/query-info modes yet. |
| buddy allocator | `adapted` | `buddy_allocator.h`, `buddy_allocator.c` | Landed with Odin-style in-place buddy block splitting/coalescing, fixed power-of-two backing storage, and fixed-alignment allocations; Bedrock reports init/pointer misuse as statuses and the generic adapter keeps Bedrock's alloc/free/resize/reset ABI instead of Odin's query modes. |
| compat allocator | `adapted` | `compat_allocator.h`, `compat_allocator.c` | Landed as a header-prefixed wrapper that tracks allocation size/alignment and hides parent old-size requirements from Bedrock callers. Bedrock defaults unset parents to heap, has no generic query-feature/query-info surface yet, and explicitly shifts the payload if the wrapped header prefix grows during an in-place parent resize. |
| virtual memory API | `adapted` | `virtual.h`, `src/mem/virtual/*` | Reserve/commit/decommit/release/protect landed with an Odin-style `virtual/*` split: shared `virtual_platform`, shared BSD/macOS `virtual_posix`, per-OS Linux/Darwin/FreeBSD/NetBSD/OpenBSD files, Windows, and `other`. |
| virtual growing/static arena core | `adapted` | `virtual_arena.h`, `src/mem/virtual/arena.c` | Growing and static arenas landed with allocator support, reset/destroy, mark/rewind, and optional trailing guard-page overflow protection. |
| tracking allocator | `adapted` | `tracking_allocator.h`, `tracking_allocator.c` | Landed with a dense live-entry list plus a private pointer index; it still omits Odin's mutex, feature-query, and source-location machinery. |
| mutex / locked allocator | `planned` | none | Useful later for shared allocators. |
| rollback stack allocator | `adapted` | `rollback_stack.h`, `rollback_stack.c` | Landed with Odin-style rollback/free collapse and singleton oversized blocks; Bedrock splits init into explicit buffered/dynamic entry points and reports invalid usage via statuses. |
| selected low-level `mem.odin` helpers | `planned` | none | Future work should target portable helpers like `set`, `zero`, `copy`, `compare_ptrs`, and `check_zero_ptr`, not a broad port of `raw.odin`'s Odin-runtime layout surface. |
| TLSF allocator | `deferred` | none | Odin has `tlsf/*`; Bedrock does not. |
| virtual temp / watermark helpers | `adapted` | `virtual_arena.h`, `virtual_arena.c` | Landed as explicit begin/end/ignore/check helpers that return statuses instead of Odin's assertion-based misuse handling. |
| virtual buffer-backed arena variant | `adapted` | `arena.h`, `arena.c` | Bedrock keeps fixed-buffer arenas in `br_arena` instead of duplicating Odin's `.Buffer` variant here. |
| file mapping / VM-backed file helpers | `adapted` | `virtual.h`, `src/mem/virtual/file.c` | Path-based map/unmap landed with Odin-style error categories; `virtual/file.c` now owns the high-level open/stat/map flow while per-platform files only map native handles. Bedrock v1 still does not expose the file-handle entry point publicly. |
| `virtual/arena_util.odin` helpers | `adapted` | `virtual_arena_util.h`, `src/mem/virtual/arena_util.c` | Landed as raw helpers plus typed C macros for `new`, `new_aligned`, `new_clone`, `make_slice`, `make_aligned`, and `make_multi_pointer`; Bedrock mirrors Odin's convenience layer without pretending C has `typeid` overloads. |

Summary:
- `mem` now includes the VM milestone that started the project.
- The VM implementation is now structurally close to Odin's `virtual/*` split.
- It is still not a broad Odin port yet.

## `core/bytes`

Current label: `substantial partial port`

Why this label:
- Bedrock covers all three main Odin `bytes` surfaces: slice helpers, buffer,
  and reader.
- A useful middle chunk of the package is implemented.
- Large rune-aware, stream-integrated, and convenience-heavy areas are still
  missing.

Current Bedrock files:
- `include/bedrock/bytes/bytes.h`
- `include/bedrock/bytes/buffer.h`
- `include/bedrock/bytes/reader.h`
- `src/bytes/bytes.c`
- `src/bytes/buffer.c`
- `src/bytes/reader.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| clone / free owned bytes | `adapted` | `bytes.h`, `bytes.c` | Implemented as explicit owned vs view types. |
| compare / equal / prefix / suffix | `done` | `bytes.h`, `bytes.c` | Implemented. |
| contains / contains_any | `done` | `bytes.h`, `bytes.c` | Implemented. |
| index / last_index / index_byte / last_index_byte / index_any / count | `done` | `bytes.h`, `bytes.c` | Implemented. |
| truncate_to_byte / trim_prefix / trim_suffix | `done` | `bytes.h`, `bytes.c` | Implemented. |
| join / concatenate / repeat | `adapted` | `bytes.h`, `bytes.c` | Implemented as explicit allocating helpers. |
| split / split_n / split_after / split_after_n | `adapted` | `bytes.h`, `bytes.c` | Implemented; empty-separator behavior now follows Odin's rune-aware semantics. |
| replace / replace_all / remove / remove_all | `adapted` | `bytes.h`, `bytes.c` | Implemented; empty-old replacement now follows Odin's rune-boundary semantics. |
| `bytes.Buffer` core | `adapted` | `byte_buffer.h`, `byte_buffer.c` | Init, reset, reserve, truncate, write, next, read, read_byte, unread_byte are implemented. |
| `bytes.Reader` core | `adapted` | `byte_reader.h`, `byte_reader.c` | Init, read, read_at, read_byte, unread_byte, seek are implemented. |
| rune_count / truncate_to_rune / contains_rune / index_rune | `planned` | none | Not implemented yet. |
| equal_fold | `planned` | none | Depends on higher Unicode case-folding support. |
| last_index_any | `planned` | none | Missing. |
| `bytes.Buffer` rune operations | `planned` | none | `read_rune`, `unread_rune`, `write_rune` not landed. |
| `bytes.Buffer` random access / stream helpers | `planned` | none | `read_at`, `write_at`, `write_to`, `read_from`, delimiter reads are not landed. |
| `bytes.Reader` rune operations | `planned` | none | `read_rune`, `unread_rune` not landed. |
| `bytes.Reader` `io` adapters | `adapted` | `bytes/reader.h`, `bytes/reader.c` | Exposed as a generic stream with read/read_at/seek/size/query support. |
| predicate / proc-based scans and trims | `planned` | none | `index_proc`, `trim_left_proc`, etc. not started. |
| trim cutset / trim_space / trim_null | `planned` | none | Not started. |
| split iterators / split_multi / fields | `planned` | none | Not started. |
| reverse / scrub / expand_tabs / partition / justify / alias helpers | `planned` | none | Not started. |

Summary:
- `bytes` is a real middle slice of the Odin module.
- It is not near parity.
- The missing work clusters around Unicode-aware helpers and additional buffer/reader convenience APIs.

## `core/strings`

Current label: `core slice`

Why this label:
- Bedrock now has the three most important surfaces for basic use: string ops,
  builder, and reader.
- But Odin `core/strings` is much broader than those three surfaces.
- The current Bedrock implementation is useful, but still nowhere near the full
  package breadth.

Current Bedrock files:
- `include/bedrock/strings/strings.h`
- `include/bedrock/strings/builder.h`
- `include/bedrock/strings/reader.h`
- `src/strings/strings.c`
- `src/strings/builder.c`
- `src/strings/reader.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| clone / free owned strings | `adapted` | `strings.h`, `strings.c` | Implemented with explicit ownership. |
| compare | `done` | `strings.h`, `strings.c` | Implemented. |
| contains / contains_rune | `done` | `strings.h`, `strings.c` | Implemented. |
| valid UTF-8 / rune_count | `done` | `strings.h`, `strings.c` | Implemented on top of `utf8`. |
| has_prefix / has_suffix | `done` | `strings.h`, `strings.c` | Implemented. |
| index / index_rune | `done` | `strings.h`, `strings.c` | Implemented. |
| truncate_to_byte / truncate_to_rune | `done` | `strings.h`, `strings.c` | Implemented. |
| trim_prefix / trim_suffix | `done` | `strings.h`, `strings.c` | Implemented. |
| `strings.Builder` core | `adapted` | `string_builder.h`, `string_builder.c` | Heap-backed and fixed-backing builder landed with write/pop operations. |
| `strings.Reader` core | `adapted` | `string_reader.h`, `string_reader.c` | Byte and rune read/unread plus seek are implemented. |
| ptr / cstring conversion helpers | `planned` | none | `string_from_ptr`, `clone_to_cstring`, etc. are not landed. |
| contains_any / index_any / last_index / last_index_any / index_byte / last_index_byte | `done` | `strings.h`, `strings.c` | Implemented. |
| prefix_length / common_prefix | `planned` | none | Not landed. |
| join / concatenate / count / repeat | `adapted` | `strings.h`, `strings.c` | Implemented as explicit allocating helpers plus substring counting. |
| replace / remove family | `adapted` | `strings.h`, `strings.c` | Implemented with explicit rewrite results; empty-old replacement follows rune boundaries. |
| split family | `adapted` | `strings.h`, `strings.c` | Implemented; empty-separator behavior follows Odin's rune-aware string semantics. |
| line split / iterators | `planned` | none | Not landed. |
| cut / substring helpers | `planned` | none | Not landed. |
| equal_fold | `planned` | none | Depends on higher Unicode case-folding support. |
| trim cutset / trim_space / trim_null | `planned` | none | Not landed. |
| fields / fields_proc | `planned` | none | Not landed. |
| conversion module (`to_lower`, `to_upper`, case conversion) | `planned` | none | Not landed. |
| intern table | `planned` | none | Not landed. |
| ascii set helper | `planned` | none | Not landed. |
| reverse / scrub / expand_tabs / partition / justify / edit distance | `planned` | none | Not landed. |

Summary:
- `strings` has the core operational slice.
- Split and rewrite helpers are now part of that slice.
- It should not yet be described as a broad port of Odin `core/strings`.
- The next safe growth area is the remaining convenience helpers, then
  table-driven Unicode behavior.

## `core/io`

Current label: `partial v1`

Why this label:
- Bedrock now has a minimal generic IO layer built around a single stream proc,
  closer to Odin's model than the earlier split-trait experiment.
- The in-memory byte and string types can be exposed through that stream shape.
- This is intentionally smaller than Odin's full `core/io` surface.

Current Bedrock files:
- `include/bedrock/io/io.h`
- `src/io/io.c`
- `include/bedrock/bytes/reader.h`
- `src/bytes/reader.c`
- `include/bedrock/bytes/buffer.h`
- `src/bytes/buffer.c`
- `include/bedrock/strings/reader.h`
- `src/strings/reader.c`
- `include/bedrock/strings/builder.h`
- `src/strings/builder.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| `Seek_From` / shared seek origin | `adapted` | `io.h` | Moved out of `base.h` into the IO module. |
| `Stream_Mode` / mode-set query model | `adapted` | `io.h`, `io.c` | Single stream proc with explicit mode dispatch and query support. |
| generic stream wrapper and aliases (`Reader`, `Writer`, `Seeker`) | `adapted` | `io.h`, `io.c` | Implemented as one `br_stream` plus alias typedefs. |
| shared IO result / seek result / size result types | `done` | `io.h` | Shared across generic IO and concrete adapters. |
| byte reader adapters | `adapted` | `bytes/reader.h`, `bytes/reader.c` | `bytes.Reader` now exposes a stream with read/read_at/seek/size support. |
| byte buffer adapters | `adapted` | `bytes/buffer.h`, `bytes/buffer.c` | `bytes.Buffer` now exposes a stream with read/write/size support. |
| string reader adapters | `adapted` | `strings/reader.h`, `strings/reader.c` | Exposed as a stream with read/read_at/seek/size support. |
| string builder adapters | `adapted` | `strings/builder.h`, `strings/builder.c` | Exposed as a stream with write/size support. |
| generic `read_at` / `write_at` / `size` fallbacks | `adapted` | `io.c`, `tests/test_io.c` | Falls back through `seek` when a stream does not implement those modes directly. |
| generic `read_byte` / `write_byte` helpers | `done` | `io.h`, `io.c`, `tests/test_io.c` | Landed on top of the stream API. |
| generic `read_rune` / `write_rune` helpers | `adapted` | `io.h`, `io.c`, `tests/test_io.c` | Landed on top of UTF-8; malformed multi-byte stream reads report consumed bytes because generic streams cannot unread. |
| generic `read_full` / `read_at_least` / `write_full` / `write_at_least` helpers | `adapted` | `io.h`, `io.c`, `tests/test_io.c` | Landed with Odin-style exact/minimum semantics plus Bedrock `NO_PROGRESS` guards for malformed C callbacks. |
| generic `copy` / `copy_buffer` helpers | `adapted` | `io.h`, `io.c`, `tests/test_io.c` | Landed with explicit short-write detection. |
| close / flush / destroy lifecycle operations | `adapted` | `io.h`, `io.c` | Present in the generic stream API; current in-memory streams mostly report unsupported. |
| buffered IO / scanners / pipes | `adapted` | `bufio/*`, `tests/test_bufio.c` | Core buffered IO has moved into the new `bufio` module. |

Summary:
- `io` now exists as a real foundational module.
- Bedrock now follows Odin's single-stream direction more closely than before.
- The next growth area is further `bufio` expansion and scanner/lookahead work.

## `core/bufio`

Current label: `partial v1`

Why this label:
- Bedrock now has the main buffered reader and writer types plus a combined
  read-writer adapter.
- The core buffered operations are present and tested.
- Odin still has additional `bufio` surfaces like lookahead readers and
  scanners that Bedrock has not started.

Current Bedrock files:
- `include/bedrock/bufio/common.h`
- `include/bedrock/bufio/lookahead_reader.h`
- `include/bedrock/bufio/reader.h`
- `include/bedrock/bufio/writer.h`
- `include/bedrock/bufio/read_writer.h`
- `src/bufio/lookahead_reader.c`
- `src/bufio/reader.c`
- `src/bufio/writer.c`
- `src/bufio/read_writer.c`
- `tests/test_bufio.c`
- `tests/test_bufio_lookahead.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| `bufio.Reader` core init/reset/destroy | `adapted` | `bufio/reader.h`, `bufio/reader.c` | Heap-backed and caller-buffer-backed init landed with explicit allocators. |
| `bufio.Reader` `peek` / `buffered` / `discard` | `done` | `bufio/reader.h`, `bufio/reader.c`, `test_bufio.c` | Landed with explicit `BR_STATUS_BUFFER_FULL` and `BR_STATUS_NO_PROGRESS`. |
| `bufio.Reader` `read` / `read_byte` / `unread_byte` | `done` | `bufio/reader.h`, `bufio/reader.c`, `test_bufio.c` | Landed and follows Odin's buffered-reader model. |
| `bufio.Reader` `read_rune` / `unread_rune` | `done` | `bufio/reader.h`, `bufio/reader.c`, `test_bufio.c` | Landed. |
| `bufio.Reader` `read_slice` / `read_bytes` / `read_string` | `adapted` | `bufio/reader.h`, `bufio/reader.c`, `test_bufio.c` | Landed with explicit owned result types for bytes and strings. |
| `bufio.Reader` stream adapter | `adapted` | `bufio/reader.h`, `bufio/reader.c` | Exposed as a generic read stream. |
| `bufio.Reader` `write_to` | `adapted` | `bufio/reader.h`, `bufio/reader.c`, `test_bufio.c` | Landed; Bedrock normalizes short successful generic writes into `BR_STATUS_SHORT_WRITE`. |
| `bufio.Writer` core init/reset/destroy | `adapted` | `bufio/writer.h`, `bufio/writer.c` | Heap-backed and caller-buffer-backed init landed with explicit allocators. |
| `bufio.Writer` `flush` / `available` / `buffered` | `done` | `bufio/writer.h`, `bufio/writer.c`, `test_bufio.c` | Landed. |
| `bufio.Writer` `write` / `write_byte` / `write_rune` / `write_string` | `done` | `bufio/writer.h`, `bufio/writer.c`, `test_bufio.c` | Landed with explicit short-write detection. |
| `bufio.Writer` stream adapter | `adapted` | `bufio/writer.h`, `bufio/writer.c` | Exposed as a generic write/flush stream. |
| `bufio.Writer` `read_from` | `adapted` | `bufio/writer.h`, `bufio/writer.c`, `test_bufio.c` | Landed; Bedrock currently always stages through the buffer because generic streams do not expose Odin's `read_from` specialization hook. |
| `bufio.Read_Writer` | `adapted` | `bufio/read_writer.h`, `bufio/read_writer.c`, `test_bufio.c` | Landed as a tiny combined adapter. |
| `lookahead_reader` | `adapted` | `bufio/lookahead_reader.h`, `bufio/lookahead_reader.c`, `test_bufio_lookahead.c` | Landed with explicit allocator-backed init options; exact-size semantics and EOF normalization match Odin. |
| `scanner` | `planned` | none | Not started. |

Summary:
- `bufio` is now a real partial module instead of a roadmap placeholder.
- The safe next growth area is `scanner`, then any remaining line-oriented
  convenience helpers.

## `core/sync`

Current label: `partial v1`

Why this label:
- Bedrock now has the core blocking synchronization primitives and a useful
  first extended slice, plus `sync/atomic`, public zero-value-ready primitives,
  public semaphores, several futex wait/wake backends, and the first
  `primitives_atomic` slices.
- The public shape is close enough to start integrating with other modules.
- The backend structure is still far from Odin's actual `core/sync` tree:
  timeout pieces, Haiku/WASM futex backends, and the exact per-OS primitive
  split still have no Bedrock equivalents.

Current Bedrock files:
- `include/bedrock/sync.h`
- `include/bedrock/sync/atomic.h`
- `include/bedrock/sync/futex.h`
- `include/bedrock/sync/primitives.h`
- `include/bedrock/sync/primitives_atomic.h`
- `include/bedrock/sync/thread.h`
- `include/bedrock/sync/extended.h`
- `include/bedrock/sync/sync_util.h`
- `src/sync/atomic.c`
- `src/sync/futex_darwin.c`
- `src/sync/futex_freebsd.c`
- `src/sync/futex_linux.c`
- `src/sync/futex_netbsd.c`
- `src/sync/futex_openbsd.c`
- `src/sync/futex_other.c`
- `src/sync/futex_windows.c`
- `src/sync/primitives.c`
- `src/sync/primitives_atomic.c`
- `src/sync/primitives_internal.c`
- `src/sync/extended.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| `current_thread_id` | `adapted` | `sync/thread.h`, `src/sync/primitives.c` | Landed as `br_thread_id` with native numeric OS thread IDs, matching Odin's strategy more closely than the earlier `pthread_self` byte-copy fallback. |
| `Mutex` | `adapted` | `sync/primitives.h`, `src/sync/primitives_internal.c` | Lock/unlock/try-lock landed as a zero-value-ready wrapper over `br_atomic_mutex`. Native pthread/Windows object storage has been removed from the public type. |
| `RW_Mutex` | `adapted` | `sync/primitives.h`, `src/sync/primitives_internal.c` | Exclusive/shared lock surface landed as a zero-value-ready wrapper over `br_atomic_rw_mutex`. |
| `Recursive_Mutex` | `adapted` | `sync/primitives.h`, `src/sync/primitives_internal.c` | Landed as a zero-value-ready wrapper over `br_atomic_recursive_mutex`. Bedrock preserves the documented same-owner try-lock behavior. |
| `Cond` | `adapted` | `sync/primitives.h`, `src/sync/primitives_internal.c` | Wait/signal/broadcast landed as a zero-value-ready wrapper over `br_atomic_cond`; timeout variants are still deferred because Bedrock has no `time` module yet. |
| `Sema` | `adapted` | `sync/primitives.h`, `src/sync/primitives_internal.c`, `tests/test_sync.c` | Public semaphore landed as a zero-value-ready wrapper over `br_atomic_sema`, with an optional C reset/init helper for non-zero initial counts. Timeout wait is deferred. |
| `sync_util` guard/lock aliases | `adapted` | `sync/sync_util.h` | Generic `lock`/`unlock`/`try_lock` style macros landed. Odin's function-style `guard` becomes a scoped block macro because C has no `defer`. |
| `Wait_Group` | `adapted` | `sync/extended.h`, `src/sync/extended.c` | Core add/done/wait landed. Timeout wait is deferred with the rest of the `time`-dependent APIs. |
| `Barrier` | `adapted` | `sync/extended.h`, `src/sync/extended.c` | Core init/wait landed. |
| `Once` | `adapted` | `sync/extended.h`, `src/sync/extended.c` | Landed with a generic `void *` callback plus a no-data helper instead of Odin's overloaded proc family. |
| `Ticket_Mutex` | `adapted` | `sync/extended.h`, `src/sync/extended.c` | Lock/unlock landed with C atomics. |
| `atomic.odin` surface | `adapted` | `sync/atomic.h`, `src/sync/atomic.c`, `tests/test_sync_atomic.c` | Landed as a C11-atomic-backed layer with Bedrock names and memory-order aliases. Bedrock intentionally keeps C's compare-exchange `expected` pointer contract instead of emulating Odin's tuple-return API, and currently requires compiler/target support for C11 atomics. |
| `primitives_internal.odin` | `adapted` | `src/sync/primitives_internal.c` | Public primitive wrappers now live in an Odin-shaped internal bridge over the atomic/futex layer. Bedrock keeps C-style explicit `init`/`destroy` reset/no-op helpers. |
| `primitives_atomic.odin` | `adapted` | `sync/primitives_atomic.h`, `src/sync/primitives_atomic.c`, `tests/test_sync_futex.c` | Non-timeout slice landed with `Atomic_Mutex`, `Atomic_RW_Mutex`, `Atomic_Recursive_Mutex`, `Atomic_Cond`, and `Atomic_Sema` on top of Bedrock futex. `Atomic_Recursive_Mutex` stores owner atomically to avoid C data races and follows documented recursive try-lock behavior rather than Odin's current same-owner `mutex_try_lock` branch. Timeout waits are still missing. |
| per-OS primitive split (`linux`, `darwin`, `freebsd`, `netbsd`, `openbsd`, `haiku`, `wasm`) | `planned` | none | Current thread ID dispatch is still consolidated in `src/sync/primitives.c` rather than Odin's actual per-OS primitive files. |
| Futex public surface and backends | `adapted` | `sync/futex.h`, `src/sync/futex_*.c`, `tests/test_sync_futex.c` | Futex wait/signal/broadcast landed for Linux, Windows, Darwin, FreeBSD, NetBSD, and OpenBSD; Linux is locally tested, other source ports still need target-specific verification. Timeout wait is deferred until Bedrock has `time`; Haiku and WASM are still missing. |
| auto-reset events | `planned` | none | Not landed yet. |
| benaphores / recursive benaphores | `planned` | none | Not landed yet. |
| timeout-based waits | `deferred` | none | Bedrock currently lacks a `time` module and duration type. |
| `sync/chan` | `deferred` | none | Channels are a later step, not part of the initial blocking-primitives slice. |

Summary:
- Bedrock now has the `sync` foundation needed to return to `mem` later.
- The public API is close enough to integrate.
- Public primitives now use the atomic/futex internals and are zero-value-ready
  instead of storing pthread/Windows native objects.
- The main missing work is finishing timeout waits, target-verifying non-Linux
  futex backends, adding the missing extended primitives, and completing the
  real Odin-style OS primitive split.
