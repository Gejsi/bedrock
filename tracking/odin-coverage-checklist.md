# Odin Coverage Checklist

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

Current label: `bootstrap subset`

Why this label:
- Bedrock only has the allocator contract and the first arena implementation.
- Odin `core/mem` is much broader and includes specialized allocators and
  virtual-memory-backed infrastructure that Bedrock has not started.

Current Bedrock files:
- `include/bedrock/mem/alloc.h`
- `include/bedrock/mem/arena.h`
- `src/mem/alloc.c`
- `src/mem/arena.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| allocator dispatch model | `adapted` | `alloc.h`, `alloc.c` | Single allocator object with request/response dispatch, adapted to C. |
| basic heap allocator | `done` | `alloc.c` | Heap allocator works and is used by current modules. |
| null allocator | `done` | `alloc.c` | Implemented. |
| fail allocator | `done` | `alloc.c` | Implemented. |
| fixed-buffer arena | `adapted` | `arena.h`, `arena.c` | Implemented as the first arena shape. |
| arena mark / rewind | `done` | `arena.h`, `arena.c` | Implemented. |
| tracking allocator | `planned` | none | Called out in `spec/modules/memory.md`, not implemented. |
| mutex / locked allocator | `planned` | none | Useful later for shared allocators. |
| rollback stack allocator | `planned` | none | Not started. |
| raw memory helper layer | `planned` | none | Not started. |
| TLSF allocator | `deferred` | none | Odin has `tlsf/*`; Bedrock does not. |
| virtual memory API | `deferred` | none | Odin has `virtual/*`; Bedrock does not. |
| virtual arena | `deferred` | none | Explicitly deferred in the memory spec. |
| file mapping / VM-backed file helpers | `deferred` | none | Not started. |

Summary:
- `mem` is not a broad Odin port yet.
- It is the minimum allocator foundation needed to let other modules exist.

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
| generic `copy` / `copy_buffer` helpers | `adapted` | `io.h`, `io.c`, `tests/test_io.c` | Landed with explicit short-write detection. |
| close / flush / destroy lifecycle operations | `adapted` | `io.h`, `io.c` | Present in the generic stream API; current in-memory streams mostly report unsupported. |
| buffered IO / scanners / pipes | `adapted` | `bufio/*`, `tests/test_bufio.c` | Core buffered IO has moved into the new `bufio` module. |

Summary:
- `io` now exists as a real foundational module.
- Bedrock now follows Odin's single-stream direction more closely than before.
- The next growth area is `read_full` / `write_full` style helpers and then
  further `bufio` expansion.

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
- `include/bedrock/bufio/reader.h`
- `include/bedrock/bufio/writer.h`
- `include/bedrock/bufio/read_writer.h`
- `src/bufio/reader.c`
- `src/bufio/writer.c`
- `src/bufio/read_writer.c`
- `tests/test_bufio.c`

| Odin area | Status | Bedrock coverage | Notes |
| --- | --- | --- | --- |
| `bufio.Reader` core init/reset/destroy | `adapted` | `bufio/reader.h`, `bufio/reader.c` | Heap-backed and caller-buffer-backed init landed with explicit allocators. |
| `bufio.Reader` `peek` / `buffered` / `discard` | `done` | `bufio/reader.h`, `bufio/reader.c`, `test_bufio.c` | Landed with explicit `BR_STATUS_BUFFER_FULL` and `BR_STATUS_NO_PROGRESS`. |
| `bufio.Reader` `read` / `read_byte` / `unread_byte` | `done` | `bufio/reader.h`, `bufio/reader.c`, `test_bufio.c` | Landed and follows Odin's buffered-reader model. |
| `bufio.Reader` `read_rune` / `unread_rune` | `done` | `bufio/reader.h`, `bufio/reader.c`, `test_bufio.c` | Landed. |
| `bufio.Reader` `read_slice` / `read_bytes` / `read_string` | `adapted` | `bufio/reader.h`, `bufio/reader.c`, `test_bufio.c` | Landed with explicit owned result types for bytes and strings. |
| `bufio.Reader` stream adapter | `adapted` | `bufio/reader.h`, `bufio/reader.c` | Exposed as a generic read stream. |
| `bufio.Reader` `write_to` | `planned` | none | Not landed. |
| `bufio.Writer` core init/reset/destroy | `adapted` | `bufio/writer.h`, `bufio/writer.c` | Heap-backed and caller-buffer-backed init landed with explicit allocators. |
| `bufio.Writer` `flush` / `available` / `buffered` | `done` | `bufio/writer.h`, `bufio/writer.c`, `test_bufio.c` | Landed. |
| `bufio.Writer` `write` / `write_byte` / `write_rune` / `write_string` | `done` | `bufio/writer.h`, `bufio/writer.c`, `test_bufio.c` | Landed with explicit short-write detection. |
| `bufio.Writer` stream adapter | `adapted` | `bufio/writer.h`, `bufio/writer.c` | Exposed as a generic write/flush stream. |
| `bufio.Writer` `read_from` | `planned` | none | Not landed. |
| `bufio.Read_Writer` | `adapted` | `bufio/read_writer.h`, `bufio/read_writer.c`, `test_bufio.c` | Landed as a tiny combined adapter. |
| `lookahead_reader` | `planned` | none | Not started. |
| `scanner` | `planned` | none | Not started. |

Summary:
- `bufio` is now a real partial module instead of a roadmap placeholder.
- The safe next growth area is `writer.read_from`, `reader.write_to`, then the
  higher-level scanner/lookahead utilities.
