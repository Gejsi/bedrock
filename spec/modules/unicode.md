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

## WTF-8 (OS strings)

A permissive sibling of the strict UTF-8 codec above, for OS-string duty on
Windows: WTF-8 is UTF-8 generalized to admit lone (unpaired) UTF-16 surrogates
as their natural 3-byte sequences, so a Windows UTF-16 path — which may contain
unpaired surrogates — round-trips losslessly through a byte string. TYPELESS:
WTF-8 data travels in ordinary `br_bytes_view`/`br_bytes` by convention; there
is NO `OsStr`-style wrapper type (the one structural guarantee such a type would
enforce is provided by `br_wtf8_concat` instead — see below).

### Relationship to the strict codec

`br_utf8_*` is UNCHANGED and stays strict-Unicode-scalar-only — its presented
contract admits no surrogates, and utf8.h makes no mention of WTF-8. WTF-8 is a
SEPARATE public surface (`include/bedrock/unicode/wtf8.h`,
`src/unicode/wtf8.c`) that reuses the strict codec's internal byte-layout
arithmetic but relaxes exactly one rule: the surrogate ban. There is no mode
flag on the strict hot path; the strict and permissive behaviors are simply
which surface you call. The proof obligation is that the existing utf8 test
suite passes untouched — the WTF-8 addition must not alter strict decode/encode
behavior in any way (a prominent regression guard in the testing section).

### What WTF-8 accepts and rejects

WTF-8 is "the strict UTF-8 rules minus the surrogate ban, plus a well-formedness
rule that a surrogate pair must be the single 4-byte astral form, never two
3-byte halves." Concretely, `br_wtf8_valid` REJECTS, exactly as strict UTF-8
does:

- truncated sequences (a lead byte without its full continuation bytes);
- overlong encodings (any non-shortest form, e.g. U+0000 as `C0 80`);
- code points above U+10FFFF;
- malformed continuation bytes (a required `80`-`BF` byte that isn't, or a
  continuation byte with no lead).

And the one WTF-8-specific rejection:

- an adjacent high-then-low surrogate encoded as TWO 3-byte sequences is
  ill-formed; a real pair must be the 4-byte astral encoding.

It ACCEPTS beyond strict UTF-8 exactly one thing: a LONE surrogate
(U+D800-U+DFFF, unpaired) as a 3-byte sequence.

`br_wtf8_valid` is the WELL-FORMED check (it enforces the no-pair-as-two-halves
rule). It is the default and the only validator in v1; see "Deferred" for why
the looser "generalized" validator is not shipped.

### API

```c
/* Well-formed WTF-8 check (rejects the ill-formed adjacent-surrogate-pair form). */
bool br_wtf8_valid(br_bytes_view s);

/* Native-endian UTF-16 code units (Windows WCHAR). */
typedef struct br_wtf16_view { const uint16_t *data; size_t len; } br_wtf16_view;

/* Two-pass sizing + convert-into-caller-buffer, matching the encoding-module
   idioms. The _len sizer returns the exact output size; the transcode returns
   the count written and BR_STATUS_SHORT_BUFFER (nothing written) if dst is too
   small -- never a partial/truncated transcode. */
size_t       br_wtf8_from_wtf16_len(br_wtf16_view s);          /* bytes needed */
br_io_result br_wtf8_from_wtf16(br_wtf16_view s, uint8_t *dst, size_t dst_cap);
size_t       br_wtf16_from_wtf8_len(br_bytes_view s);          /* u16 units needed */
br_io_result br_wtf16_from_wtf8(br_bytes_view s, uint16_t *dst, size_t dst_cap);

/* Well-formed join: if `a` ends in a lone high surrogate and `b` begins with a
   lone low surrogate, the two halves re-encode as one 4-byte astral scalar at
   the seam (rather than leaving two 3-byte lone surrogates, which would be
   ill-formed WTF-8). Otherwise a plain byte concatenation. Owned result. */
br_bytes_result br_wtf8_concat(br_bytes_view a, br_bytes_view b, br_allocator allocator);
```

For the `br_io_result` count: bytes for the `->wtf8` direction, u16 units for
the `->wtf16` direction.

### The property: lossless round-trip

WTF-16 -> WTF-8 -> WTF-16 is BIT-EXACT for any `uint16_t` sequence, including
lone high surrogates, lone low surrogates, and adjacent-but-unpaired surrogates.
This is the reason the module exists: it is the Windows path round-trip that a
lossy transcoder corrupts.

### Fast paths

Two zero-copy shortcuts, since OS paths are overwhelmingly ASCII/BMP:

- A pure-ASCII prefix (all bytes `< 0x80`) needs no rune assembly; `valid` and
  the `_len` sizers scan for the first `>= 0x80` byte and only engage the
  multibyte path after it, so an all-ASCII input validates and sizes in one
  linear pass.
- WTF-8 containing no surrogates and no invalid sequences IS valid UTF-8 (the
  byte representation is identical for all non-surrogate content), so strict
  UTF-8 bytes are already valid WTF-8 with zero transformation. The surrogate
  machinery stays off the hot path for the common case.

### Intentional deviation from Odin

Odin's `core/unicode/utf16` is LOSSY-strict: it maps unpaired surrogates to
U+FFFD, which would corrupt Windows path round-trips (Odin's own `os` sidesteps
its `utf16` by calling Win32 directly). Even Win32's `MultiByteToWideChar` is
lossy by default. Bedrock therefore ports Zig's LOSSLESS WTF-8 model for
OS-string duty rather than Odin's `utf16` — WTF-8 is the genuine answer for
representing OS strings faithfully, not a Zig quirk. Strict UTF-16
display-transcode (Odin's actual `utf16` use case, where U+FFFD replacement is
acceptable) is NOT in this module's scope.

### Scope boundaries

- `br_wtf16_view` is NATIVE-endian `uint16_t` (Windows WCHAR is native
  little-endian on the only target that produces these). Reading foreign-endian
  UTF-16 is a byte-order concern the caller/os layer handles before transcoding;
  out of scope here.
- NT `\\?\` long-path prefixing (normalize + absolutize + prefix past MAX_PATH)
  is NOT in this module — it is the `os` Windows backend's job (ruled). This
  module is pure codec: units in, bytes out, no path semantics.

### Deferred: the generalized validator

A looser `br_wtf8_valid_general` (accepting even the ill-formed
adjacent-pair-as-two-halves form, matching Zig's `wtf8ValidateSlice`) is NOT
shipped in v1. Producer analysis: a correct `br_wtf16_from_wtf8`/`_from_wtf16`
transcode never emits the ill-formed form (an adjacent high+low in the input is
a legitimate astral pair and transcodes to the 4-byte encoding), and
`br_wtf8_concat` exists precisely so that nobody produces it by naive byte
joining. So generalized-but-not-well-formed WTF-8 has no producer inside a
correct pipeline; a general validator's consumer is hypothetical, and
hypothetical consumers do not earn public surface. Zig shipping only the general
form is a curiosity, not a template — the default-name-is-the-safe-form rule
points the other way. If a real consumer appears (e.g. validating
externally-sourced WTF-8 before fix-up), add it then, with this analysis as the
pre-thought design.

### Testing

- LOSSLESS ROUND-TRIP (the property): every `uint16_t` sequence class — pure
  BMP, astral pair (-> 4-byte), lone high surrogate, lone low surrogate,
  adjacent-but-unpaired surrogates — survives WTF-16 -> WTF-8 -> WTF-16
  bit-exact. This is the test Odin's lossy `utf16` fails.
- WELL-FORMED VALIDITY: a high+low pair encoded as two 3-byte sequences is
  INVALID; a lone-surrogate 3-byte sequence is VALID; a surrogate pair as the
  4-byte form is VALID. Plus the strict-UTF-8 rejections still fire (truncated,
  overlong, > U+10FFFF, bad continuation).
- CONCAT SEAM: `a` ending in a high surrogate + `b` starting with a low
  surrogate combine into one 4-byte astral scalar (result well-formed, not two
  lone halves); non-adjacent-surrogate concats are plain byte joins.
- Sizers exact vs actual transcode output; `SHORT_BUFFER` never-truncate; empty
  inputs.
- STRICT CODEC UNTOUCHED: the existing utf8 test suite passes unchanged (the
  regression guard that the WTF-8 addition did not leak into strict
  decode/encode).
- ASan across all transcode buffer paths.
