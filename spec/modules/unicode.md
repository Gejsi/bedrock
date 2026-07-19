# Unicode

UTF-8 encode/decode/validate/count foundation ported from Odin
`core/unicode/utf8` (itself a Go `unicode/utf8` port). Higher Unicode tables
and case folding are separate future work.

## Status

Audited July 19, 2026 against Odin (contract), Go (ancestor), and Rust
`core::str` validation: correct on all six security-relevant dimensions
(overlong rejection, surrogate rejection, > U+10FFFF rejection, truncation,
U+FFFD error semantics, boundary/width functions).

## Error semantics

- Invalid input decodes to `(U+FFFD, width 1)` — the decoder resyncs by exactly
  ONE byte, matching Odin and Go. This deliberately differs from Unicode's
  "maximal subpart" recommendation (which Rust's `from_utf8_lossy` follows):
  a corrupt 3-byte sequence yields three U+FFFD here, one in Rust.
- U+FFFD is itself a valid rune; callers distinguish a decoded literal U+FFFD
  from an error by `width == 1` on multi-byte input positions.

## Intentional deviations from Odin

- `br_utf8_encode` validates the rune BEFORE size dispatch, so out-of-range
  runes encode as the 3-byte replacement `EF BF BD`. Odin's `encode_rune`
  sizes on the original value and emits an invalid 4-byte sequence for runes
  above U+10FFFF (see `tracking/odin-suspected-bugs.md`); Bedrock is
  deliberately not bug-compatible here.
- `decode_last`'s exhaustion clamp differs cosmetically from Odin
  (`max(start, limit)` vs `max(start, 0)`); the observable result is identical.

## Implementation notes

- Lead-byte classification is a branch chain (readable, predictable) rather
  than Odin/Go's 256-entry table. Two optional future optimizations, both
  gated on real benchmarks: an ASCII word-at-a-time fast path for
  `valid`/`rune_count` (the realistic throughput win; fuzz hard against the
  scalar path if added), and the 256-entry classify table.
- Adversarial test vectors (overlong 3/4-byte, `F4 90 80 80`, lone
  continuations, `F5`-`FF` leads, boundary runes, negative-rune encode) are
  tracked as follow-up test work.
