# ADR-0002: Rust Reference

## Status

Accepted.

## Decision

- Track Rust as a pinned git submodule in `upstream/rust`.
- Treat Rust as a secondary implementation reference, not as an API or semantic
  source.
- Keep Odin as Bedrock's primary porting reference.

## Rationale

Odin remains the target for module shape and behavior. Rust's standard library
is useful for comparison when Bedrock has to own low-level runtime machinery in
C, especially futex-backed mutexes, condition variables, once primitives, and
thread parking.

Rust should be used to validate implementation strategy and contention behavior,
not to pull Bedrock away from Odin's API model.

## Consequences

- Use `upstream/rust` when comparing low-level synchronization/runtime designs.
- Prefer sparse local checkouts of Rust source paths when possible; the
  superproject pins the submodule commit, not a vendored copy.
- Document meaningful Rust-inspired deviations in the relevant module notes.
