# ADR-0005: Go Reference

## Status

Accepted.

## Decision

- Track Go as a pinned git submodule in `upstream/go`, sparse-checked-out to
  `src/` only (the same cone pattern as `upstream/rust`'s `library/`).
- Treat Go as a pinned design-ancestry reference: Odin's `encoding/*`,
  `path/slashpath`, `io`/`bufio`, `unicode/utf8`, and `math/bits` surfaces are
  direct Go stdlib descendants, so Go is the authoritative source for their
  original semantics and — critically — their canonical test vectors.
- Odin remains Bedrock's API and semantics contract (ADR-0002's rule applies
  to Go as well: references validate implementations and supply test vectors;
  they do not pull Bedrock away from Odin's API model).

## Rationale

ADR-0001 keeps research-only repositories out of the submodule set by default,
and this repo previously kept such references as gitignored local checkouts
(`upstream/typescript-go/`). Go has graduated past research-only: three
research briefs (encodings, slashpath, utf8) had to caveat their Go
comparisons because web sources were unreliable to fetch, and multiple test
tasks want Go's own test files (`utf8_test.go`, `path` vectors) verbatim.
Pinning the submodule commit makes every Go line citation in `tracking/` and
`spec/` reproducible from a clone, exactly as ADR-0002 argued for Rust.

## Consequences

- Use `upstream/go/src/...` for design-ancestry comparisons and as the source
  of canonical test vectors; document meaningful Go-inspired deviations in the
  relevant module notes, as with Rust.
- Sparse-checkout gotcha for fresh clones: a submodule's `.git` is a FILE
  pointing into the superproject's `.git/modules/upstream/go/`, so the sparse
  spec lives at `.git/modules/upstream/go/info/sparse-checkout`, not inside
  `upstream/go/.git/`. Re-apply the `src/`-only cone when initializing the
  submodule on a new machine.
- Update Go with normal submodule workflows; a pin bump should come with a
  delta review of the referenced surfaces, mirroring the Odin pin-bump rule.
