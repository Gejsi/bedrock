# Odin Suspected Bugs

Concise notes for issues found while porting Odin code to Bedrock.

All issues below were verified present in upstream Odin at `2c25fb9`
(July 19, 2026).

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
