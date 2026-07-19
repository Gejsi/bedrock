#Odin Coverage Checklist

This file makes the status of the current Odin-inspired work explicit instead of
describing it with vague labels.

The comparison target is the Odin checkout currently pinned in
`upstream/odin/` on July 19, 2026 (`2c25fb9`).

Note: the `ecbb204..2c25fb9` delta was audited on July 19, 2026. Odin's
`Dynamic_Arena` gained per-request alignment; Bedrock now matches it (effective
alignment `max(minimum_alignment, request)` on both the in-block and out-band
paths). Odin's virtual `reserve` also gained an optional address hint (not
exposed in Bedrock yet; intentional for now). All other changes were
annotations, docs, or dropped Haiku/Essence backends.

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
- `include/bedrock/mem/mutex_allocator.h`
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
- `src/mem/mutex_allocator.c`
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
| small stack allocator | `adapted` | `small_stack.h`, `small_stack.c` | Landed with Odin-style tiny headers, out-of-order free semantics, and overwrite-on-reuse behavior; Bedrock returns statuses instead of panics and still enforces Bedrock's power-of-two alignment contract after Odin-style clamping. The clamp matches Odin's 32 on 64-bit targets; a hypothetical ILP32 target would clamp at 16 where Odin stays at 32. |
| dynamic arena allocator | `adapted` | `dynamic_arena.h`, `dynamic_arena.c` | Landed with Odin-style block cycling, separate block/array allocators, out-band allocations, reset/free-all split, reallocate-on-resize behavior, and per-request alignment (`max(minimum_alignment, request)`, floored on the out-band path too); Bedrock keeps the generic allocator adapter on alloc/free/resize/reset only, so there are no query-feature/query-info modes yet. |
| buddy allocator | `adapted` | `buddy_allocator.h`, `buddy_allocator.c` | Landed with Odin-style in-place buddy block splitting/coalescing, fixed power-of-two backing storage, and fixed-alignment allocations; Bedrock reports init/pointer misuse as statuses and the generic adapter keeps Bedrock's alloc/free/resize/reset ABI instead of Odin's query modes. |
| compat allocator | `adapted` | `compat_allocator.h`, `compat_allocator.c` | Landed as a header-prefixed wrapper that tracks allocation size/alignment and hides parent old-size requirements from Bedrock callers. Bedrock defaults unset parents to heap, has no generic query-feature/query-info surface yet, and explicitly shifts the payload if the wrapped header prefix grows during an in-place parent resize. |
| virtual memory API | `adapted` | `virtual.h`, `src/mem/virtual/*` | Reserve/commit/decommit/release/protect landed with an Odin-style `virtual/*` split: shared `virtual_platform`, shared BSD/macOS `virtual_posix`, per-OS Linux/Darwin/FreeBSD/NetBSD/OpenBSD files, Windows, and `other`. |
| virtual growing/static arena core | `adapted` | `virtual_arena.h`, `src/mem/virtual/arena.c` | Growing and static arenas landed with allocator support, reset/destroy, mark/rewind, an embedded mutex matching Odin's `virtual.Arena`, and optional trailing guard-page overflow protection. Static arenas require explicit init; Bedrock intentionally does not adopt Odin's lazy static-init-on-first-alloc. |
| tracking allocator | `adapted` | `tracking_allocator.h`, `tracking_allocator.c` | Landed with a dense live-entry list plus a private pointer index and an embedded mutex matching Odin's `Tracking_Allocator`; it still omits feature-query and source-location machinery. |
| mutex / locked allocator | `adapted` | `mutex_allocator.h`, `mutex_allocator.c` | Landed as an Odin-style serialized allocator wrapper around a backing allocator. Bedrock defaults an unset backing allocator to heap instead of using Odin's ambient context allocator. |
| rollback stack allocator | `adapted` | `rollback_stack.h`, `rollback_stack.c` | Landed with Odin-style rollback/free collapse and singleton oversized blocks; Bedrock splits init into explicit buffered/dynamic entry points and reports invalid usage via statuses. |
| selected low-level `mem.odin` helpers | `adapted` | `mem/mem.h` | Header-only static-inline `br_mem_set`/`zero`/`copy` (memcpy, non-overlapping)/`move` (memmove, overlap-safe)/`compare` (view pair)/`compare_ptrs` (n bytes)/`check_zero` landed as thin `<string.h>` wrappers with standard-C-typed signatures. Copy/move use C-familiar names (`br_mem_copy` = memcpy, `br_mem_move` = memmove) rather than an Odin-style `copy`/`copy_non_overlapping` split, so the public vocabulary matches what C programmers expect. `compare` normalizes to -1/0/+1 with bytewise-then-length ordering, matching `br_bytes_compare`. `check_zero` collapses Odin's `check_zero`/`check_zero_ptr` (no slice type) and uses an in-bounds byte walk that avoids the over-read in Odin's word-aligned prologue. Excluded: `zero_explicit` (secure-zero primitive, deferred pending an explicit ask), `zero_item`/`zero_slice`/`simple_equal` (generics), and `ptr_offset`/`ptr_sub`/slice-from-ptr (`raw.odin` runtime-layout surface). |
| TLSF allocator | `deferred` | none | Odin has `tlsf/*`; Bedrock does not. |
| virtual temp / watermark helpers | `adapted` | `virtual_arena.h`, `virtual_arena.c` | Landed as explicit begin/end/ignore/check helpers that return statuses instead of Odin's assertion-based misuse handling. |
| virtual buffer-backed arena variant | `adapted` | `arena.h`, `arena.c` | Bedrock keeps fixed-buffer arenas in `br_arena` instead of duplicating Odin's `.Buffer` variant here. |
| file mapping / VM-backed file helpers | `excluded` | none | Cut July 19, 2026 — file APIs belong in a future `os` module, not `mem`. The path-based map/unmap surface (`br_vm_map_file`, `virtual/file.c`, and the per-OS native-handle mapping) had no consumers and was removed; only the pure VM primitives remain. |
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
| index / last_index / index_byte / last_index_byte / index_any / count | `done` | `bytes.h`, `bytes.c` | Implemented. Uses a first-byte + memcmp scan rather than Odin's Rabin-Karp; semantically identical, performance-only difference. |
| truncate_to_byte / trim_prefix / trim_suffix | `done` | `bytes.h`, `bytes.c` | Implemented. |
| join / concatenate / repeat | `adapted` | `bytes.h`, `bytes.c` | Implemented as explicit allocating helpers. |
| split / split_n / split_after / split_after_n | `adapted` | `bytes.h`, `bytes.c` | Implemented; empty-separator behavior now follows Odin's rune-aware semantics. |
| replace / replace_all / remove / remove_all | `adapted` | `bytes.h`, `bytes.c` | Implemented; empty-old replacement now follows Odin's rune-boundary semantics. |
| `bytes.Buffer` core | `adapted` | `byte_buffer.h`, `byte_buffer.c` | Init, reset, reserve, truncate, write, next, read, read_byte, unread_byte are implemented. |
| `bytes.Reader` core | `adapted` | `byte_reader.h`, `byte_reader.c` | Init, read, read_at, read_byte, unread_byte, seek are implemented. |
| rune_count / truncate_to_rune / contains_rune / index_rune | `planned` | none | Not implemented yet. |
| case conversion (`to_lower_ascii`, `to_upper_ascii`) | `adapted` | `bytes.h`, `bytes.c` | ASCII-only case mapping; bytes >= 0x80 pass through so UTF-8 stays valid. Odin `core/bytes` has no case-conversion procs, so this is a Bedrock addition, not a 1:1 port. Unicode-aware conversion is deferred (case tables), hence the `_ascii` suffix. |
| equal_fold | `planned` | none | Depends on higher Unicode case-folding support. |
| last_index_any | `planned` | none | Missing. |
| `bytes.Buffer` rune operations | `planned` | none | `read_rune`, `unread_rune`, `write_rune` not landed. |
| `bytes.Buffer` random access / stream helpers | `planned` | none | `read_at`, `write_at`, `write_to`, `read_from`, delimiter reads are not landed. |
| `bytes.Reader` rune operations | `planned` | none | `read_rune`, `unread_rune` not landed. |
| `bytes.Reader` `io` adapters | `adapted` | `bytes/reader.h`, `bytes/reader.c` | Exposed as a generic stream with read/read_at/seek/size/query support. |
| predicate / proc-based scans and trims | `planned` | none | `index_proc`, `trim_left_proc`, etc. not started. |
| trim cutset / trim_space / trim_null | `planned` | none | Not started. |
| split iterators / split_multi / fields | `adapted` | `strings.h`, `strings.c`, `bytes.h`, `bytes.c` | Allocation-free split iterator landed as a caller-owned cursor struct (`br_*_split_iter` + `_next` / `_split_after_iter_next`), plus a strings fields iterator. The iterator yields the split LIST element-for-element (keeps trailing empties and yields one empty field for empty-input-with-separator) — a documented deviation from Odin's `split_iterator`, which drops the trailing empty; `fields`/the fields iterator are the skip-empties path. `split_multi` not started. |
| partition / alias helpers | `planned` | none | Not started. |
| reverse / scrub / expand_tabs / justify | `excluded` | none | Struck July 19, 2026 pre-port (see cut list): niche text-processing surface. |

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
| cut / substring helpers | `adapted` | `strings.h`, `strings.c` | `cut`, `substring` (with a bool bounds flag), `prefix_length`, and `common_prefix` landed as rune-indexed sub-view helpers following Odin. String-only, matching Odin (no bytes mirror). |
| equal_fold | `planned` | none | Depends on higher Unicode case-folding support. |
| trim cutset / trim_space / trim_null | `adapted` | `strings.h`, `strings.c` | Landed (delegate to the bytes trim family). Cutset trimming is rune-aware (decodes the cutset as UTF-8 and matches runes at each edge, matching Odin and Go). `trim_space` is ASCII-whitespace-only — a documented deviation from Odin's Unicode `is_space`, deferred until the space tables land. |
| fields / fields_proc | `adapted` | `strings.h`, `strings.c` | `fields` landed: splits on runs of ASCII whitespace, no empty fields, into the existing view-list result. ASCII-only (bytes >= 0x80 are field content), a documented deviation from Odin's Unicode `is_space` fallback. `fields_proc` (predicate variant) is deferred with the rest of the `_proc` family. Mirrored in `bytes.h`/`bytes.c`. |
| conversion module (`to_lower`, `to_upper`, case conversion) | `adapted` | `strings.h`, `strings.c` | ASCII-only `to_lower_ascii`/`to_upper_ascii` landed (delegate to the bytes forms); non-ASCII bytes pass through untouched. Named with the `_ascii` suffix because Unicode-aware case conversion is deferred until the case tables land, which will take the unqualified names. |
| intern table | `planned` | none | Not landed. |
| ascii set helper | `planned` | none | Not landed. |
| partition | `planned` | none | Not landed. |
| reverse / scrub / expand_tabs / justify / edit distance / case munging | `excluded` | none | Struck July 19, 2026 pre-port (see cut list). |

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
| `Cond` | `adapted` | `sync/primitives.h`, `src/sync/primitives_internal.c`, `tests/test_sync.c` | Wait/signal/broadcast and timeout wait landed as a zero-value-ready wrapper over `br_atomic_cond`. |
| `Sema` | `adapted` | `sync/primitives.h`, `src/sync/primitives_internal.c`, `tests/test_sync.c` | Public semaphore landed as a zero-value-ready wrapper over `br_atomic_sema`, with an optional C reset/init helper for non-zero initial counts and timeout wait. |
| `sync_util` guard/lock aliases | `adapted` | `sync/sync_util.h` | Generic `lock`/`unlock`/`try_lock` style macros landed. Odin's function-style `guard` becomes a scoped block macro because C has no `defer`. |
| `Wait_Group` | `adapted` | `sync/extended.h`, `src/sync/extended.c`, `tests/test_sync.c` | Core add/done/wait and timeout wait landed. Bedrock tracks remaining duration across condition wait iterations so spurious wakeups do not restart the public timeout window. |
| `Barrier` | `adapted` | `sync/extended.h`, `src/sync/extended.c` | Core init/wait landed. Built on Bedrock's own mutex+cond rather than `pthread_barrier`, which is optional POSIX and absent on macOS (verified July 19, 2026) — portable by construction. |
| `Once` | `adapted` | `sync/extended.h`, `src/sync/extended.c` | Landed with a generic `void *` callback plus a no-data helper instead of Odin's overloaded proc family. |
| `Auto_Reset_Event` | `adapted` | `sync/extended.h`, `src/sync/extended.c`, `tests/test_sync.c` | Landed with Odin's signed status counter plus `Sema` storage, using `br_atomic_i32` for C data-race safety. |
| `Parker` | `adapted` | `sync/extended.h`, `src/sync/extended.c`, `tests/test_sync.c` | Landed with Odin's futex state machine. Bedrock's timeout variant returns `bool` and restores state on timeout so C callers can tell whether a token was consumed. |
| `One_Shot_Event` | `adapted` | `sync/extended.h`, `src/sync/extended.c`, `tests/test_sync.c` | Landed as Odin's futex-backed one-way broadcast event. |
| `Ticket_Mutex` | `adapted` | `sync/extended.h`, `src/sync/extended.c` | Lock/unlock landed with C atomics. |
| `atomic.odin` surface | `adapted` | `sync/atomic.h`, `src/sync/atomic.c`, `tests/test_sync_atomic.c` | Landed as a C11-atomic-backed layer with Bedrock names and memory-order aliases. Bedrock intentionally keeps C's compare-exchange `expected` pointer contract instead of emulating Odin's tuple-return API, and currently requires compiler/target support for C11 atomics. |
| `primitives_internal.odin` | `adapted` | `src/sync/primitives_internal.c` | Public primitive wrappers now live in an Odin-shaped internal bridge over the atomic/futex layer. Bedrock keeps C-style explicit `init`/`destroy` reset/no-op helpers. |
| `primitives_atomic.odin` | `adapted` | `sync/primitives_atomic.h`, `src/sync/primitives_atomic.c`, `tests/test_sync_futex.c` | `Atomic_Mutex`, `Atomic_RW_Mutex`, `Atomic_Recursive_Mutex`, `Atomic_Cond`, and `Atomic_Sema` landed on top of Bedrock futex, including atomic condition/semaphore timeout waits. `Atomic_Mutex` keeps Odin's three-state shape but uses Rust-style relaxed-load spinning and a CAS fast path. `Atomic_Recursive_Mutex` stores owner atomically to avoid C data races and follows documented recursive try-lock behavior rather than Odin's current same-owner `mutex_try_lock` branch. |
| per-OS primitive split (`linux`, `darwin`, `freebsd`, `netbsd`, `openbsd`, `haiku`, `wasm`) | `excluded` | none | Struck as a goal July 19, 2026 (cut list): a file-layout refactor with no new capability; thread ID dispatch stays consolidated in `src/sync/primitives.c`. |
| Futex public surface and backends | `adapted` | `sync/futex.h`, `src/sync/futex_*.c`, `tests/test_sync_futex.c` | Futex wait/timeout-wait/signal/broadcast landed for Linux, Windows, Darwin, FreeBSD, NetBSD, and OpenBSD; Linux is locally tested, other source ports still need target-specific verification. Windows runtime-resolves Odin's `RtlWaitOnAddress` path plus wake functions so users do not need extra sync import libraries, Darwin weak-imports newer `os_sync_*` APIs with `__ulock_*` fallback, and FreeBSD uses the native no-timeout `_umtx_op` form instead of Odin's 4-hour timeout loop. Haiku and WASM are still missing. |
| benaphores / recursive benaphores | `excluded` | none | Struck July 19, 2026 pre-port (cut list): redundant against the existing mutex/sema surface. |
| timeout-based waits | `adapted` | `sync/futex.h`, `sync/primitives_atomic.h`, `sync/primitives.h`, `sync/extended.h` | Timeout waits landed for futex, atomic cond/sema, public cond/sema, and wait groups. |
| `sync/chan` | `deferred` | none | Channels are a later step, not part of the initial blocking-primitives slice. |

Summary:
- Bedrock now has the `sync` foundation needed to return to `mem` later.
- The public API is close enough to integrate.
- Public primitives now use the atomic/futex internals and are zero-value-ready
  instead of storing pthread/Windows native objects.
- The extended-primitive surface is COMPLETE (reconciled July 19, 2026):
  all nine of Odin's extended primitives are landed (7) or deliberately
  excluded (benaphores ×2, per the cut list — no modern ecosystem ships one;
  `br_atomic_mutex` is the futex-era superset of what a benaphore optimizes).
- The remaining sync work is target-verifying the non-Linux futex backends and
  adding Haiku/WASM futex backends on demand. The per-OS primitive split was
  struck as a goal (cut list). No primitives are missing.

## `core/thread`

Current label: `v1`

Why this label:
- A thin, explicit OS-thread lifecycle wrapper landed: caller-allocated handle,
  create/create_ex/join/detach/yield over pthreads and Win32 `_beginthreadex`.
- The spawned thread never touches the handle after startup (exit codes ride the
  substrate's join channel, startup data goes through a creator-stack control
  block + `br_sema` handshake, and the self-join identity is written creator-side),
  so a detached handle is safe to drop while the thread runs.
- Lifecycle misuse is uniformly guarded by an atomic state machine, which is the
  module's value over the raw substrates.

Current Bedrock files:
- `include/bedrock/thread.h`
- `include/bedrock/thread/thread.h`
- `src/thread/thread_posix.c`
- `src/thread/thread_windows.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| create / start | `adapted` | `thread/thread.h`, `src/thread/*` | Single `br_thread_create[_ex]`; Odin's two-phase create/start gate (which existed for context injection) is deleted. A create-internal creator-stack handshake reappears purely for memory safety, never exposed. |
| join | `adapted` | `thread/thread.h`, `src/thread/*` | Joins and reads the int exit code from the substrate join channel. Double-join returns `INVALID_STATE`; self-join returns `INVALID_ARGUMENT` (deadlock avoidance) via creator-side identity comparison before the state transition. |
| detach | `adapted` | `thread/thread.h`, `src/thread/*` | Fire-and-forget; the handle may then leave scope safely. Double-detach / join-after-detach return `INVALID_STATE`. |
| yield | `adapted` | `thread/thread.h`, `src/thread/*` | `sched_yield` / `SwitchToThread`. |
| thread name | `adapted` | `src/thread/*` | Best-effort debug name, copied at create and applied from inside the trampoline (satisfies macOS's self-only naming); truncates, never fails a create. |
| exit code | `adapted` | `thread/thread.h`, `src/thread/*` | `int(*)(void*)` return, transported through the substrate (pthread retval round-trip / `GetExitCodeThread`), never the handle. Richer results go through the caller's `arg` struct. |
| `is_done` | `excluded` | none | Deliberate Odin deviation: polling completion invites busy-waits, is racy, and has no portable form. Callers join or set an atomic flag in their arg. |
| Windows backend | `adapted` | `src/thread/thread_windows.c` | Uses `_beginthreadex`, never `CreateThread` (deliberate Odin deviation: `CreateThread` skips per-thread CRT init that a C library and its callers require). Recorded in `tracking/odin-suspected-bugs.md`. |
| thread pools / TLS / cancel / priority | `excluded` | none | Out of scope for v1 (see spec Non-Goals); pools are a later module over threads + atomics + queues. |

Summary:
- The 24 raw-pthread test sites across `sync`, `sync_futex`, `virtual_mem`,
  `mutex_allocator`, and `tracking_allocator` now use `br_thread_*` and run
  platform-uniform (their `_WIN32` guards dropped, except `virtual_mem`'s
  narrowed guard that still gates a POSIX-only fork/SIGSEGV death test).
- Windows CI is the first exercise of the Win32 backend.

## `core/encoding`

Current label: `partial v1` (pilot wave in progress)

Current Bedrock files:
- `include/bedrock/encoding.h` (module umbrella)
- `include/bedrock/encoding/encoding.h` (shared decode-result types)
- `include/bedrock/encoding/hex.h`, `src/encoding/hex.c`
- `include/bedrock/encoding/base64.h`, `src/encoding/base64.c`
- `include/bedrock/encoding/endian.h` (header-only)
- `include/bedrock/encoding/varint.h`, `src/encoding/varint.c`
- `tests/test_hex.c`, `tests/test_base64.c`, `tests/test_endian.c`, `tests/test_varint.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| `encoding/hex` | `adapted` | `encoding/hex.h`, `src/encoding/hex.c` | Encode (lower/upper, allocating/into-buffer/writer), decode (allocating/into-buffer), decode_sequence. Deviations per spec: frees on error (fixes Odin's leak wart), byte `error_offset` reporting, `BR_STATUS_INVALID_ENCODING` for malformed input. |
| `encoding/endian` | `adapted` | `encoding/endian.h` | All 8 widths checked get/put over views plus unchecked raw-pointer forms. Deviations per spec: `br_endian_` prefix, shift-based host-agnostic assembly, memcpy float bit-casts, memcpy byte-order probe (no compiler extensions), no f16. Upstream has zero tests; Bedrock's suite is the first. |
| `encoding/base64` | `adapted` | `encoding/base64.h`, `src/encoding/base64.c` | Std/URL alphabets x padded/raw presets + a strict flag; encode (allocating/into-buffer/writer) and decode (allocating/into-buffer/writer). Deviations per spec: explicit presets + strict/padded controls (Odin exposes only a lenient decoder); drops Odin's dead `dst` decode param (caller buffers go through the into-buffer variant); no whitespace skipping (Go's decoder skips `\r\n`, Bedrock rejects any non-alphabet byte at its `error_offset`); strict rejects a non-canonical final quantum, lenient masks. RFC 4648 vectors. |
| `encoding/varint` | `adapted` | `encoding/varint.h`, `src/encoding/varint.c` | Unsigned + signed LEB128 over the u64/i64 ABI: encoded_len, encode (into-buffer), decode. Deviations per spec: u64/i64 canonical width (Odin uses u128/i128; max 10 bytes not 19); DWARF sign-extended signed LEB128 (NOT Go's zig-zag); own `br_uleb128_result`/`br_ileb128_result` structs (no `error_offset` — `size` is bytes consumed always); truncation -> `UNEXPECTED_EOF`, value-too-wide -> `INVALID_ENCODING`, short encode buffer -> `SHORT_BUFFER` (never truncates). Signed 10th-byte padding bits must match the sign (`INT64_MIN` fills all 10 bytes). Stream read/write variants deferred. |

## `core/path`

Current label: `slashpath v1`

Current Bedrock files:
- `include/bedrock/path.h` (module umbrella)
- `include/bedrock/path/slashpath.h`, `src/path/slashpath.c`
- `tests/test_slashpath.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| `path/slashpath` | `adapted` | `path/slashpath.h`, `src/path/slashpath.c` | Full surface (clean/dir/base/ext/name/split/split_elements/join/match). Deviations per spec/modules/path.md: rewrite-result aliasing (no clone when already clean, improving on Odin's Lazy_Buffer), match follows Go's malformed-pattern validation that Odin's port dropped (suspected upstream bug, verification pending), match errors are BR_STATUS_INVALID_ARGUMENT. Test suite ports Go's path vectors including all 56 match tests; upstream Odin has none. |
| `path/filepath` | `excluded` | none | Not requested; `br_path_` stays reserved. |

## `core/math`

Current label: `bits v1`

Current Bedrock files:
- `include/bedrock/math.h` (module umbrella)
- `include/bedrock/math/bits.h`, `src/math/bits.c`
- `tests/test_bits.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| `math/bits` | `adapted` | `math/bits.h`, `src/math/bits.c` | Full fixed-width surface (counts/scans/bit_width/rotate/reverse/byteswap/power-of-two/add-sub-mul-div/bitfields) on the three-tier scheme (C23 stdbit.h, compiler builtins, portable fallback) with a force-fallback differential harness. Deviations per spec/modules/math.md: total zero-input contracts (clz/ctz(0) = width), no standalone log2, bool carries, div returns a status and never panics, no u128/word-size variants, byte-order conversion lives in encoding/endian. The differential harness caught a signed-overflow UB in the portable ctz on its first run (fixed; recorded in the spec). |

## `core/math/rand`

Current label: `v1`

Landed as the standalone `rand` module (Odin groups it under `core/math`; Bedrock
gives it its own `bedrock/rand.h`).

Current Bedrock files:
- `include/bedrock/rand.h` (module umbrella)
- `include/bedrock/rand/rand.h`
- `src/rand/rand.c`
- `src/rand/entropy_linux.c`, `src/rand/entropy_darwin.c`,
  `src/rand/entropy_windows.c`, `src/rand/entropy_other.c`
- `tests/test_rand.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| generator + seeding | `adapted` | `rand/rand.h`, `src/rand/rand.c` | PCG64 DXSM ported byte-for-byte from Go v2's `pcg.go`; two-word `br_rand_seed` (Go `NewPCG`) plus `br_rand_seed_entropy`. Zero-value-ready: a zeroed `br_rand` equals `seed(0,0)`. |
| raw + bounded draws | `adapted` | `rand/rand.h`, `src/rand/rand.c` | `u64`/`u32` status-free; `u64_below` uses Lemire multiply-shift-reject with the guarded modulo and a power-of-two mask path; `i64_between` computed in unsigned to avoid signed-overflow UB at the extremes. |
| floats + shuffle | `adapted` | `rand/rand.h`, `src/rand/rand.c` | `f64`/`f32` in `[0,1)` by scaling top mantissa bits; Fisher-Yates `shuffle` over a caller swap callback (no element size, no alloc). |
| OS entropy | `adapted` | `src/rand/entropy_*.c` | `getrandom`/`getentropy`/`BCryptGenRandom` (runtime-resolved, no import lib), `NOT_SUPPORTED` elsewhere; whole-buffer-or-error, never a weak fallback seed. Mirrors the sync futex per-OS split. |

Deviations from Odin (per spec/modules/rand.md):
- PCG64 DXSM (not XSL-RR) ported from Go v2 — a machine-checkable reference
  stream — where Odin ships pcg + xoshiro256.
- Fixed increment, no settable streams (differential-locked to the reference).
- No single-u64 seed convenience; the two-word seed is the whole seeding API.
- No global generator / no ambient context (deviation from C `rand()` and Odin's
  context generator) — explicit `br_rand *` everywhere.
- Bounded draws return a defined value on an empty range (`u64_below(0)` = 0,
  `i64_between(lo>=hi)` = lo) where Go panics; the no-panic rule.
- Non-cryptographic by contract; the CSPRNG need is served by
  `br_rand_entropy_fill`, not the PRNG stream.

## `core/time`

Current Bedrock files:

- `include/bedrock/time.h`
- `include/bedrock/time/time.h`
- `src/time/time.c`
- `src/time/time_linux.c`
- `src/time/time_unix.c`
- `src/time/time_windows.c`
- `src/time/time_other.c`
- `tests/test_time.c`

| Odin feature/file | Status | Bedrock files | Notes |
| --- | --- | --- | --- |
| `Duration` and constants | `adapted` | `time/time.h` | Landed as `br_duration` with nanosecond precision and `BR_NANOSECOND` through `BR_HOUR`. |
| `Time` / `now` / `sleep` | `adapted` | `time/time.h`, `src/time/time_*.c` | Landed as Unix nanoseconds plus platform sleep. Windows follows Odin's millisecond `Sleep` behavior for now. |
| `Tick` / low-level perf helpers | `partial` | `time/time.h`, `src/time/time.c`, `src/time/time_*.c` | Monotonic tick, add/diff/since/lap landed. TSC/invariant-frequency helpers are deferred. |
| duration conversion helpers | `adapted` | `src/time/time.c` | Nanoseconds plus floating micro/milli/second/minute/hour conversions landed. |
| duration round/truncate helpers | `planned` | none | Small self-contained helpers, but not needed for sync timeout wiring. |
| stopwatch helpers | `planned` | none | Not needed for sync timeouts. |
| datetime/timezone/RFC3339/ISO8601 | `planned` | none | Larger independent slices; do not pull into timeout work. |

## `core/strconv`

Current label: `adapted`

Current Bedrock files:
- `include/bedrock/strconv.h` (module umbrella)
- `include/bedrock/strconv/strconv.h`
- `src/strconv/internal.h`, `src/strconv/decimal.c`, `src/strconv/parse.c`,
  `src/strconv/format.c`
- `tests/test_strconv.c`, `tests/data/{testfp,atof1k,ftoa1k}.txt`,
  `tests/data/LICENSE-go`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| decimal engine (`strconv/decimal`) | `adapted` | `src/strconv/decimal.c` | Fixed `[384]`-digit multiprecision decimal ported from Odin's `decimal` + `generic_float`, parameterized by `br__float_info` (f32/f64). No bignum/heap/tables. |
| int parse (`atoi`/`parse_int`/`parse_uint`) | `adapted` | `parse.c` | Strict `br_parse_i64`/`_u64` + `_prefix`; base 0/2..36; i32/u32 narrowing wrappers. Deviations: strict-by-default; base 2..36 superset over Odin's `assert(base<=16)`; base-0 recognizes `0x`/`0o`/`0b` only (Odin's `0d`/`0z` dropped); no underscore separators; bad base returns `INVALID_ARGUMENT` (no panic); overflow saturates with `OUT_OF_RANGE`; `br_parse_u64` rejects a sign. |
| float parse (`parse_f64`/`parse_f32`) | `adapted` | `parse.c`, `decimal.c` | Native f32 rounding via `decimal_to_float_bits(&d,&br__f32_info)` — NOT f64-then-narrow (fixes Odin's double-rounding bug, `tracking/odin-suspected-bugs.md`); verified by the `1.00000017881393432617187499` -> `0x3f800001` witness. Case-insensitive inf/infinity/nan. |
| int/bool/float format (`generic_ftoa`, `write_*`) | `adapted` | `format.c` | int (all bases), bool, and f32/f64 in shortest/decimal/exponent/general. Deviations: typed `br_float_format` enum + `_f32`/`_f64` pairs (not fmt byte + bit_size); never-truncate (`SHORT_BUFFER`, count 0); negative prec in non-shortest is `INVALID_ARGUMENT`; `*_SHORTEST_MAX` macros are shortest-only, other modes size via `br_format_f*_bound` (clamped so a pathological prec cannot overflow). No leading `+` on positive values. |
| `quote`/`unquote`, `Append*` family, f16 | `excluded`/`planned` | none | `Append*` dropped (into-buffer + builder cover it); quote/unquote deferred to the `ini` consumer; f16 excluded (no standard C type). Eisel-Lemire/Grisu3 fast paths deliberately declined for exact-shortest at minimal code cost. |
