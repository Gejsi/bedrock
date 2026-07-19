# Foundation

## Project Goal

Build an Odin-inspired C library that is pleasant to consume from plain C while
staying honest about what C can and cannot express.

This is not a 1:1 port of Odin's `core/` or runtime. It is a selective rewrite
inspired by Odin's package design, naming discipline, and allocator-first
thinking.

## Non-Goals

- Porting Odin compiler/runtime internals
- Recreating Odin's implicit `context`
- Shipping a giant platform layer in v1
- Hiding portability problems behind undefined behavior or macro magic

## Language Baseline

The implementation baseline is C11.

The public API should remain usable without `_Generic` or other optional sugar.
If a compiler supports C11 generic selection, we may layer convenience macros on
top, but the real ABI must stay explicit and ordinary.

## Distribution Model

The source tree should stay modular:

- `include/bedrock/` for public modular headers
- `src/` for implementation
- `spec/`, `tracking/`, and `decisions/` for project memory

Release artifacts should be generated from that tree:

- `dist/bedrock.h`
- `dist/bedrock.c`

If a single-header distribution is wanted later, generate it from the same
source of truth with a `BEDROCK_IMPLEMENTATION` mode. Do not author the project
as one giant hand-maintained amalgamated header.

## Naming

- Canonical public ABI prefix: `br_`
- Macros: `BR_`
- Internal symbols: `br__`

The canonical ABI should stay prefixed because C has one global identifier
namespace and Bedrock will define many names that the standard library never
had: arenas, typed containers, string builders, path helpers, and similar
facilities.

However, that does not mean end users must always type the prefix directly.
There should eventually be two layers:

- canonical prefixed headers and symbols for the real implementation
- optional compatibility or alias headers for projects that want shorter names

Important distinction:

- exact libc replacements may expose standard names such as `malloc` or `memcpy`
  only when their semantics intentionally match the C standard
- new Bedrock-specific APIs should keep a namespace in the canonical ABI

That split keeps the implementation safe while still allowing a "this is my
stdlib" experience for application code.

### Scalar Types In The Public ABI

The public ABI is spelled entirely in standard C types: `size_t`, `ptrdiff_t`,
`uintptr_t`, `intptr_t`, the `uint8_t`/`int8_t` family, `float`, and `double`.
The short aliases (`usize`, `isize`, `uptr`, `iptr`, `u8`..`u64`, `i8`..`i64`,
`f32`, `f64`) are optional sugar defined in `include/bedrock/types.h` and enabled
by default. They are genuinely optional: because no public header depends on
them, defining `BEDROCK_NO_SHORT_TYPES` before including Bedrock disables the
aliases and the headers still compile. Implementation sources under `src/` may
keep using the aliases internally. This is enforced by
`tests/test_no_short_types.c`, which compiles `<bedrock.h>` with the aliases
disabled and runs in CI on every platform and mode.

## Context And Allocation Policy

Odin's implicit `context` does not translate well to C. Bedrock should use the
following policy:

- allocation is explicit at API boundaries
- long-lived objects store their allocator
- temporary allocation uses arenas and savepoints, not hidden thread-local state
- optional "context" objects may exist later, but only as explicit values passed
  by the caller

The default mental model should be:

- pass `br_allocator` by value into constructors, builders, and container init
  (the struct is two pointers; this matches the shipped ABI)
- pass a `br_arena *` when the lifetime is region-based
- pass a user state pointer when callbacks need extra context

## Error Model

Prefer explicit status returns and result structs over hidden global error state.

## Generic Strategy

Bedrock will use three layers for generic behavior:

1. Erased algorithms for operations that naturally work on raw memory.
2. Macro-generated typed containers for data structures.
3. Optional `_Generic` sugar for nicer call sites on compilers that support it.

This is described in detail in `spec/modules/generics.md`.

## Context Management In This Repo

Do not accumulate a single giant planning file.

Use:

- `spec/foundation.md` for project-wide rules
- `spec/modules/*.md` for module-level design
- `decisions/ADR-*.md` for stable decisions
- `tracking/odin-port-matrix.md` for source coverage and scope

This mirrors the good part of the `typescript-go` pattern: one small repo-wide
instruction file plus small task-specific context files.
