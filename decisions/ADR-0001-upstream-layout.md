# ADR-0001: Upstream Layout

## Status

Accepted.

## Decision

- Track Odin as a git submodule in `upstream/odin`.
- Do not track research-only repositories as submodules by default.
- Keep project memory split across small focused documents.

## Rationale

Odin is the primary design reference for this project, and pinning it as a
submodule gives a stable upstream commit while still allowing clean updates.

Large single-file context documents do not age well. Splitting context into
foundation, module notes, decisions, and tracking files keeps each document
small enough to be maintained.

## Consequences

- Update Odin with normal submodule workflows.
- Use `tracking/odin-port-matrix.md` to record scope changes.
- Add new stable design choices as ADRs instead of rewriting old notes.
