# Odin Suspected Bugs

Concise notes for issues found while porting Odin code to Bedrock.

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
