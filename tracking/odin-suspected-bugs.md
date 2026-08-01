# Odin Suspected Bugs

Concise notes for issues found while porting Odin code to Bedrock.

All source-level implementation issues below are present in upstream Odin at
`2c25fb9` (July 19, 2026). An independent review on August 2, 2026 classified
the 17 claims as 12 confirmed, 4 partial, 0 refuted, and 1 not executable on
the available macOS host. "Partial" means the implementation issue exists but
the original consequence or scope was overstated.

Footnote (dormant, not a numbered bug): Odin's `Memory_Block_Flag.Overflow_Protection`
path would VirtualProtect an uncommitted page on Windows
(`virtual.odin:90-108`), the same defect Bedrock fixed in its own guard-page
feature. No in-tree Odin caller passes the flag, so the defect is dormant in
Odin's own call graph, but `memory_block_alloc` is public and external callers
can request it. The `protect` result is ignored. This was source-verified and
target-checked, but not executed on Windows. Bedrock's overflow protection is
the reachable, error-checked completion of that path.

Footnote (quirk, not a numbered bug): `core/strings` `_split_iterator`
(strings.odin:1072) ends with `ok = res != ""`, dropping a single TRAILING
empty field, while the `_split` LIST (:882) always keeps it —
`split("a,", ",")` is `["a", ""]` but iterating `"a,"` yields only `"a"`.
It also drops an empty input's sole field, and the shared helper affects the
split-after and line iterator variants. Middle empties are unaffected (the
empty-check sits only in the separator-not-found branch, so `"a,,b"` still
iterates `a`, `""`, `b` with no early termination or data loss). Internal
list-vs-iterator inconsistency only. Bedrock's split iterator keeps trailing
empties, matching its own list and Go's SplitSeq.

Footnote (context-dependent, not a numbered bug): Odin's `thread_windows.odin`
creates threads with raw `CreateThread` (:70) rather than `_beginthreadex`.
When embedding CRT-dependent C code, `_beginthreadex` is the conservative
interoperability choice. The Odin implementation fact was source-verified and
target-checked, but no concrete Odin runtime failure or universal CRT
leak/corruption consequence was established on the available macOS host.
Bedrock's thread port deliberately uses `_beginthreadex` on Windows.

## `core/mem` check_zero_ptr reads out of bounds

- File: `core/mem/mem.odin`
- Area: `check_zero_ptr`, word-alignment path (:292-310)
- Issue: the prologue loop `for b in start..<start_aligned` (:296) reads the
  range `[start, start_aligned)` with no clamp to `end`. For a small unaligned
  length that does not reach the next `align_of(uintptr)` boundary,
  `align_forward` rounds `start_aligned` past `end`. With an 8-byte word, a
  3-byte range at alignment residue 1 reads 4 bytes past `end`; the epilogue
  also starts 1 byte before `start`. The failing residues satisfy
  `0 < start % 8 < 8-len`: residues 1-4 for length 3, 1-2 for length 5, and 1
  for length 6. Length 7 stays in bounds. The `{1,2,4,8}` fast-path switch
  hides the most common small sizes.
- Expected: the word-alignment strategy is only valid when the region spans an
  aligned word; small regions need a clamped or plain byte loop.
- Effect: out-of-bounds read on both sides of the buffer — can fault against a
  guard page and trips AddressSanitizer. Memory safety, not conformance.
- Bedrock: `br_mem_check_zero` (mem helpers port) uses a straight in-bounds
  loop that never reads outside `[start, end)`, with the len=3-at-odd-address
  repro as a regression test.

## `core/path/slashpath` match under-reports malformed patterns

- File: `core/path/slashpath/match.odin`
- Area: `match_chunk` early return plus `match` missing trailing validation
- Issue: `match_chunk` returns immediately when the name is exhausted
  (match.odin:106-108) instead of continuing to validate the chunk's syntax
  (Go keeps a `failed` flag and still runs `getEsc`, match.go:122-192); and
  `match` lacks Go's post-loop that validates the remaining pattern via
  `matchChunk(chunk, "")` (match.go:79-84).
- Expected: malformed patterns report a syntax error regardless of whether the
  name happens to be consumed first.
- Effect: `match("a[", "a")` returns `(false, .None)` where Go returns
  `(false, ErrBadPattern)`; same for a lone trailing `\` against an empty name
  and for `"a/b["`. A spec-conformance gap (missing error), not memory safety.
- Bedrock: the slashpath port follows Go (validates chunks to completion and
  checks the trailing pattern) and documents the deviation in
  `spec/modules/path.md`; Go's 56 match vectors pass, including the
  ErrBadPattern rows.



## `core/unicode/utf8` encode of out-of-range runes

- File: `core/unicode/utf8/utf8.odin`
- Area: `encode_rune`
- Issue: for `i > 0x10ffff` the proc replaces `r` with U+FFFD but then sizes
  the encoding using the ORIGINAL `i` (`if i <= 1<<16-1`), so it takes the
  4-byte branch and emits `F0 8F BF BD`.
- Expected: dispatch on the replaced rune (U+FFFD fits 3 bytes: `EF BF BD`).
  Go's `AppendRune` gets this right via fallthrough into the 3-byte case; the
  fallthrough semantics were lost in the port. Surrogates are unaffected
  (they are <= 0xFFFF and take the 3-byte branch).
- Effect: encoding any rune above U+10FFFF produces an invalid UTF-8 sequence
  (F0 requires a second byte >= 0x90) that the package's own decoder rejects.
- Bedrock: `br_utf8_encode` validates first and encodes the replacement rune,
  emitting `EF BF BD`; locked by `tests/test_utf8.c`.

## `core/mem` dynamic arena out-band ignores `minimum_alignment`

- File: `core/mem/allocators.odin`
- Area: `dynamic_arena_alloc_bytes_non_zeroed` (out-band branch)
- Issue: the doc comment states "All allocations will be aligned at a minimum
  to a boundary specified by `minimum_alignment`" (allocators.odin:1656-1657),
  but the out-band branch forwards only the raw request alignment
  (allocators.odin:1803), never applying `max(a.minimum_alignment, alignment)`
  the way the in-band path does (allocators.odin:1809).
- Expected: floor the out-band alignment by `minimum_alignment` too.
- Effect: an out-band (large) allocation can come back aligned below the
  arena's configured minimum, contradicting the documented contract.
- Bedrock: floors the out-band path by `minimum_alignment`.

## `core/mem` dynamic arena reused-block under-alignment

- File: `core/mem/allocators.odin`
- Area: `dynamic_arena_alloc_bytes_non_zeroed` in-band path plus
  `_dynamic_arena_cycle_new_block`
- Issue: after cycling a new current block, the alloc path sets
  `margin = 0; memory = a.current_pos` unconditionally (allocators.odin
  :1824-1825), assuming the block base satisfies
  `max(minimum_alignment, alignment)`. Fresh blocks are allocated at that
  alignment (:1733), but blocks reused from `unused_blocks` (:1726) were
  created for a possibly-smaller alignment and are neither re-aligned nor
  checked.
- Expected: re-align the bump pointer against the block actually obtained and
  recompute the margin after cycling.
- Effect: an allocation whose alignment exceeds the reused block's original
  alignment can return a pointer that violates the requested alignment. Using
  that pointer for an over-aligned typed access can then be invalid. Reachable
  via reset followed by a larger-alignment allocation; latent because most
  callers use one uniform alignment.
- Bedrock: re-aligns against the obtained block and re-checks the margin after
  cycling; returns a correctly aligned pointer or `BR_STATUS_OUT_OF_MEMORY`,
  never an under-aligned pointer (`src/mem/dynamic_arena.c`, documented
  in-code).

## `core/encoding/hex` decode returns an allocation on invalid input

- File: `core/encoding/hex/hex.odin`
- Area: `decode`
- Issue: `dst` is allocated before the parse loop; on an invalid character the
  proc returns `(dst, false)` without freeing it.
- Expected: free `dst` before returning failure, as base64's `decode` does.
- Effect: an invalid digit in an even-length input returns a non-nil partial
  allocation with `ok=false`, so callers must still free it. The allocation is
  not unconditionally lost, and odd-length failures allocate nothing.
- Bedrock: pilot port frees on error and offers caller-buffer decoding.

## `core/encoding/base64` dead decode parameter

- File: `core/encoding/base64/base64.odin`
- Area: `decode`
- Issue: the `dst: []byte = nil` parameter is never referenced in the body;
  `decode` always allocates.
- Expected: honor the caller buffer or remove the parameter.
- Effect: callers passing a scratch buffer silently get a fresh allocation.
- Bedrock: drops the parameter; caller buffers go through the explicit
  into-buffer variant.

## `core/mem` stack allocator resize

- File: `core/mem/allocators.odin`
- Area: `stack_resize_bytes_non_zeroed`
- Issue: in-place resize checks `old_offset != header.prev_offset`.
- Expected: compare against the stack's current `prev_offset`, same as the last-allocation rule used by `stack_free`.
- Effect: in-place resize appears to work only for the first stack allocation; later last allocations fall back to allocate/copy.
- Bedrock: uses `stack->prev_offset`.

## `core/sync` atomic recursive mutex try-lock

- File: `core/sync/primitives_atomic.odin`
- Area: `atomic_recursive_mutex_try_lock`
- Issue: same-owner branch calls `mutex_try_lock(&m.mutex)`.
- Expected: if current thread already owns the recursive mutex, increment recursion and return `true`.
- Effect: recursive `try_lock` can return `false` for the owning thread because the inner normal mutex is already locked.
- Bedrock: increments recursion and returns `true`.

## `core/sync` atomic recursive mutex owner race

- File: `core/sync/primitives_atomic.odin`
- Area: `Atomic_Recursive_Mutex.owner`
- Issue: `owner` is a plain `int`, but `atomic_recursive_mutex_lock` reads it before acquiring the inner mutex while unlock writes it before releasing the inner mutex.
- Expected: the pre-lock ownership check should use atomic load/store or another synchronization mechanism.
- Effect: concurrent lock/unlock has unsynchronized conflicting accesses to
  `owner`, so the implementation does not establish a valid ownership check.
  A stale-read mutual-exclusion failure is a possible consequence, not an
  observed one.
- Bedrock: stores `owner` as an atomic thread id.

## `core/sync` parker timeout state

- File: `core/sync/extended.odin`
- Area: `park_with_timeout`
- Issue: timeout return can leave `Parker.state` as `PARKER_PARKED`.
- Expected: timeout should restore `PARKER_EMPTY` unless an `unpark` raced and installed `PARKER_NOTIFIED`.
- Effect: a later `park` before another `unpark` can underflow the futex state away from valid parker states.
- Bedrock: timeout wait returns `bool` and cleans up the state transition.

## `core/strconv` parse_f32 double-rounds through f64

- File: `core/strconv/strconv.odin`
- Area: `parse_f32` (:748-751), `parse_f32_prefix` (:819)
- Issue: parses at f64 precision and then narrows (`f32(parse_f64(s))`) — two
  consecutive roundings (decimal→f64, then f64→f32) instead of one
  correctly-rounded decimal→f32. Go's `atof32` runs the entire conversion with
  float32 parameters (`floatBits(&float32info)`, internal/strconv/atof.go
  :574-628) specifically to avoid this double-rounding.
- Expected: round decimal→f32 in a single step. Odin's own decimal engine is
  `Float_Info`-parameterized and already supports it
  (`decimal_to_float_bits(&d, &_f32_info)`); the cast shortcut bypasses that.
- Effect: up to 1 ULP error for inputs near an f32 rounding boundary. Witness
  (machine-checked against `strtof` as the correctly-rounded oracle):
  `parse_f32("1.00000017881393432617187499")` yields bits `0x3f800002`
  (1.00000024) where the correctly rounded f32 is `0x3f800001` (1.00000012);
  `"1.0000000596046448"` errs 1 ULP in the other direction. Precision bug, not
  a crash.
- Bedrock: `br_parse_f32` rounds natively at f32 precision through the shared
  parameterized decimal engine (never f64-then-narrow), gated by the Paxson
  f32 vectors (`spec/modules/strconv.md`).

## `core/time` RFC 3339 parser accepts exactly two fractional digits

- File: `core/time/rfc3339.odin`
- Area: timestamp parse, fractional-second component
- Issue: the parser consumes exactly TWO fractional digits (hundredths),
  computing nanoseconds as `10_000_000 * hundredths`. A one-digit fraction
  fails. For a longer valid fraction, it returns a prefix parse with
  `consumed` stopping after the first two digits rather than consuming the
  complete timestamp.
- Expected: RFC 3339 permits arbitrary fractional precision
  (`time-secfrac = "." 1*DIGIT`); a nanosecond-resolution DateTime should
  parse up to nine digits.
- Effect: callers that require a complete timestamp must compare `consumed`
  with the input length. A caller that ignores short consumption can accept
  only the `.12` prefix of `.123456789`; the parser itself does not silently
  consume and discard the remaining digits.
- Bedrock: `br_rfc3339_parse` reads up to nine fractional digits into the
  nanosecond field and accepts-but-ignores digits beyond nanosecond
  resolution (documented), per `spec/modules/time.md`.

## `core/bytes` buffer self-append uses a stale aliased source

- File: `core/bytes/buffer.odin`
- Area: `_buffer_grow` plus `buffer_write` (:108-135, :166-172)
- Issue: `buffer_write(b, buffer_to_bytes(b))` passes a slice backed by
  `b.buf`. When the append must grow the dynamic array, `_buffer_grow` can
  reallocate `b.buf` before `buffer_write` copies from the source slice. The
  source still points into the freed allocation.
- Expected: detect a source slice that aliases `b.buf` and rebase it after
  compaction/reallocation, or preserve the source before growing.
- Effect: a natural self-append can read a stale source after compaction or
  reallocation. With a 64-byte buffer after consuming one byte, the 63-byte
  unread view is invalidated by growth and the observed appended bytes are
  incorrect.
- Bedrock: `br_byte_buffer_write` detects views into its own allocation,
  validates that they refer to initialized bytes, and rebases unread views
  after compaction or reallocation. The self-append case is covered by
  `tests/test_byte_buffer.c`.
