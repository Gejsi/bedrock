# Odin Suspected Bugs

Concise notes for issues found while porting Odin code to Bedrock.

All issues below were verified present in upstream Odin at `2c25fb9`
(July 19, 2026).

Footnote (dormant, not a numbered bug): Odin's `Memory_Block_Flag.Overflow_Protection`
path would VirtualProtect an uncommitted page on Windows
(`virtual.odin:90-108`), the same defect Bedrock fixed in its own guard-page
feature — but in Odin the flag is passed by no call site and the `protect`
return is ignored, so it can never manifest. Bedrock's overflow protection is
the reachable, error-checked completion of that dormant path.

Footnote (quirk, not a numbered bug): `core/strings` `_split_iterator`
(strings.odin:1072) ends with `ok = res != ""`, dropping a single TRAILING
empty field, while the `_split` LIST (:882) always keeps it —
`split("a,", ",")` is `["a", ""]` but iterating `"a,"` yields only `"a"`.
Middle empties are unaffected (the empty-check sits only in the
separator-not-found branch, so `"a,,b"` still iterates `a`, `""`, `b` with no
early termination or data loss). Internal list-vs-iterator inconsistency only.
Bedrock's split iterator keeps trailing empties, matching its own list and
Go's SplitSeq.

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
  alignment can return an under-aligned pointer (undefined behavior; potential
  fault for over-aligned types). Reachable via reset followed by a
  larger-alignment allocation; latent because most callers use one uniform
  alignment.
- Bedrock: re-aligns against the obtained block and re-checks the margin after
  cycling; returns a correctly aligned pointer or `BR_STATUS_OUT_OF_MEMORY`,
  never an under-aligned pointer (`src/mem/dynamic_arena.c`, documented
  in-code).

## `core/encoding/hex` decode leak on invalid input

- File: `core/encoding/hex/hex.odin`
- Area: `decode`
- Issue: `dst` is allocated before the parse loop; on an invalid character the
  proc returns `(dst, false)` without freeing it.
- Expected: free `dst` before returning failure, as base64's `decode` does.
- Effect: every failed decode leaks the allocation unless the caller frees a
  buffer it was just told is invalid.
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
- Effect: concurrent lock/unlock can race on `owner`; practical failures include stale same-owner reads causing a thread to skip the inner mutex and violate mutual exclusion.
- Bedrock: stores `owner` as an atomic thread id.

## `core/sync` parker timeout state

- File: `core/sync/extended.odin`
- Area: `park_with_timeout`
- Issue: timeout return can leave `Parker.state` as `PARKER_PARKED`.
- Expected: timeout should restore `PARKER_EMPTY` unless an `unpark` raced and installed `PARKER_NOTIFIED`.
- Effect: a later `park` before another `unpark` can underflow the futex state away from valid parker states.
- Bedrock: timeout wait returns `bool` and cleans up the state transition.
