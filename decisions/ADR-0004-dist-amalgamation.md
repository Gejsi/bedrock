# ADR-0004: Dist Amalgamation

## Status

Accepted.

## Decision

- Keep the modular tree (`include/bedrock/<module>/<file>.h`, `src/**/*.c`) as
  the single source of truth. Never hand-edit the amalgamation.
- Generate two release artifacts with `tools/amalgamate.py` (python3,
  stdlib-only):
  - `dist/bedrock.h` — declarations always visible; the implementation is
    folded in under `#ifdef BEDROCK_IMPLEMENTATION`.
  - `dist/bedrock.c` — a two-line translation unit that defines
    `BEDROCK_IMPLEMENTATION` and includes `bedrock.h`, for consumers who prefer
    a `.c`/`.h` pair over the single header.
- Discover the layout, never hardcode it: the declaration set comes from
  recursively expanding `include/bedrock.h`; the implementation set comes from
  globbing `src/**/*.c`. Adding or wiring a module needs no generator change.
- Commit the generated `dist/` and gate it in CI:
  - `make check-dist` regenerates and fails if the committed artifacts drifted.
  - `make dist-smoke` compiles the amalgamation with `-Wall -Wextra -Werror` in
    every consumption mode: two-file, single-header, declarations under
    `-DBEDROCK_NO_SHORT_TYPES`, and a two-translation-unit link (no duplicate
    symbols).

## Rationale

The distribution format is a consumer convenience; the modular tree is what the
project maintains. Generating from one source of truth keeps them from
diverging, and committing the output plus a `check-dist` gate makes staleness a
build failure rather than a silent drift.

Two invariants drive the generator:

- **H1 — feature-test macro hoisting.** A feature-test macro (`_GNU_SOURCE`,
  `_POSIX_C_SOURCE`, `WIN32_LEAN_AND_MEAN`, `_WIN32_WINNT`) only takes effect if
  it precedes the first system `#include` in its translation unit. Folding every
  source into one unit means those macros must be stripped from the bodies and
  re-emitted once, before any include, at the top of the implementation section.
- **H2 — internal-symbol collisions** were resolved in the modular tree before
  this generator landed, so concatenating sources introduces no clashes.

Folding all sources into one translation unit is safe because the per-OS files
(`virtual_*`, `futex_*`, `time_*`) each guard their body with a platform `#if`,
so exactly one implementation is active per platform — the same property the
per-file build already relies on. `src/mem/virtual/internal.h` is inlined once
via a shared visited set.

An amalgamation-only hazard surfaced during smoke testing and is worth
recording: the per-file build sets `_POSIX_C_SOURCE` only in `time_unix.c`, but
hoisting it to the whole unit makes it apply to the Darwin backends too, and
strict `_POSIX_C_SOURCE` hides the BSD extensions they need (`MAP_ANONYMOUS`,
`pthread_threadid_np`). The generator therefore also emits `_DARWIN_C_SOURCE`
(inert on non-Apple platforms) even though no source defines it. This is the
kind of interaction that only appears once translation units are merged.

## Consequences

- Regenerating `dist/` after any `include/` or `src/` change is part of the
  merge checklist; `check-dist` enforces it. When a new module is wired into
  `bedrock.h` and its sources land under `src/`, the next regeneration folds it
  in automatically.
- The generated files are excluded from `make check-format`: they are build
  output, not authored code.
- Future work: the living cut list (`tracking/cut-list.md`) floats DEMOTE
  candidates — modules kept in the tree but excluded from the default `dist/`
  (e.g. debug-only `tracking_allocator`, niche `buddy_allocator`). The generator
  folds everything today; selective exclusion is deferred until the maintainer
  makes those CUT/KEEP/DEMOTE calls, at which point a filter can key off the
  cut list rather than a hardcoded set.
