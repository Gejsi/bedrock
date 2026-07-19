# ADR-0006: Zig as a Fourth Reference Ecosystem

## Status

Accepted (July 19, 2026).

## Decision

Add Zig's standard library (`ziglang/zig`, `lib/std` only) as a fourth pinned
implementation reference under `upstream/zig`, alongside Odin (the API contract),
Go, and Rust. Sparse checkout limited to `lib/std`, matching the Go (`src/`) and
Rust (`library/`) narrowing in ADR-0002 and ADR-0005.

Source: `https://codeberg.org/ziglang/zig` — Zig moved its canonical repository
from GitHub to Codeberg; the submodule URL points at Codeberg accordingly.

## Rationale

Zig is a systems language with no runtime and a hand-written libc-independent
standard library — the closest ecosystem to Bedrock's own constraints (explicit
allocators passed everywhere, no hidden control flow, freestanding-capable).
That makes its std the most directly transferable design reference for the
platform-facing modules Bedrock is now entering:

- **os**: Zig's `lib/std/os` and `lib/std/fs` solve exactly the cross-platform
  file/env/args surface Bedrock's os wave targets, with an allocator-explicit
  API shape that maps to Bedrock more directly than Go's GC'd `os` or Odin's
  runtime-backed one.
- **Windows paths / Unicode**: Zig pioneered the mainstream use of **WTF-8**
  for OS-string handling (`lib/std/unicode.zig`), the design Rust also adopted
  (`library/core/src/wtf8.rs`) and Go implements in its Windows syscall layer.
  All three converged; Zig is where the pattern is most cleanly isolated.

The always-compare rule already spans Odin/Go/Rust; Zig joins for the
systems-without-runtime perspective the other three lack (Go has a runtime+GC,
Rust has a heavier std, Odin has an implicit context).

## Consequences

- Research briefs touching platform/OS/Unicode surfaces should consult Zig std
  alongside the existing three.
- Zig is a REFERENCE, not a contract: Odin still defines Bedrock's API shape.
  Zig informs implementation strategy (especially WTF-8 and the freestanding
  file API) where its constraints match Bedrock's.
- Pinned by submodule commit like the others; a pin bump is a deliberate,
  recorded act.
