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
| split / split_n / split_after / split_after_n | `adapted` | `bytes.h`, `bytes.c` | Implemented, but empty-separator behavior is byte-oriented rather than Odin's Unicode-aware nil behavior. |
| replace / replace_all / remove / remove_all | `adapted` | `bytes.h`, `bytes.c` | Implemented, but empty-old semantics are byte-oriented. |
| `bytes.Buffer` core | `adapted` | `byte_buffer.h`, `byte_buffer.c` | Init, reset, reserve, truncate, write, next, read, read_byte, unread_byte are implemented. |
| `bytes.Reader` core | `adapted` | `byte_reader.h`, `byte_reader.c` | Init, read, read_at, read_byte, unread_byte, seek are implemented. |
| rune_count / truncate_to_rune / contains_rune / index_rune | `planned` | none | Not implemented yet. |
| equal_fold | `planned` | none | Depends on higher Unicode case-folding support. |
| last_index_any | `planned` | none | Missing. |
| `bytes.Buffer` rune operations | `planned` | none | `read_rune`, `unread_rune`, `write_rune` not landed. |
| `bytes.Buffer` random access / stream helpers | `planned` | none | `read_at`, `write_at`, `write_to`, `read_from`, delimiter reads are not landed. |
| `bytes.Reader` rune operations | `planned` | none | `read_rune`, `unread_rune` not landed. |
| `bytes.Reader` `io` adapters | `planned` | none | Deferred until Bedrock `io` exists. |
| predicate / proc-based scans and trims | `planned` | none | `index_proc`, `trim_left_proc`, etc. not started. |
| trim cutset / trim_space / trim_null | `planned` | none | Not started. |
| split iterators / split_multi / fields | `planned` | none | Not started. |
| reverse / scrub / expand_tabs / partition / justify / alias helpers | `planned` | none | Not started. |

Summary:
- `bytes` is a real middle slice of the Odin module.
- It is not near parity.
- The missing work clusters around Unicode-aware helpers and `io` integration.

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
| contains_any / index_any / last_index / last_index_any / index_byte / last_index_byte | `planned` | none | Not landed. |
| prefix_length / common_prefix | `planned` | none | Not landed. |
| join / concatenate / count / repeat | `planned` | none | Not landed. |
| replace / remove family | `planned` | none | Not landed. |
| split family / line split / iterators | `planned` | none | Not landed. |
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
- It should not yet be described as a broad port of Odin `core/strings`.
- The next safe growth area is convenience helpers, then `io` adapters, then
  table-driven Unicode behavior.
