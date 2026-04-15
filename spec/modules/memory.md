# Memory

## Status

`core:mem` is in scope for v1, but only the parts that fit cleanly into C
without hidden ambient context.

## Keep In V1

- allocator abstraction
- fixed-buffer arena allocator
- scratch allocator
- stack allocator
- small stack allocator
- cross-platform virtual memory reserve/commit/decommit/release/protect
- virtual static arenas
- virtual growing arenas
- arena savepoints / rewind markers
- virtual arena temp/watermark helpers
- trailing guard-page overflow protection for virtual arenas
- file mapping / unmapping
- rollback stack allocator
- tracking allocator
- null allocator
- panic/fail-fast allocator
- future debug wrappers such as guard allocators

## Defer

- TLSF and other specialized allocators
- platform-specific VM extras beyond the current trailing guard-page support

Those features are valuable, but they belong after the core VM-backed arena
story is stable.

## Proposed Core Allocator Shape

Bedrock should not expose `malloc`-shaped callbacks as the only abstraction.
Odin's single-dispatch allocator model is better, but the C API should use
plain request/response structs rather than language magic.

Example direction:

```c
typedef enum br_alloc_op {
    BR_ALLOC_OP_ALLOC,
    BR_ALLOC_OP_ALLOC_UNINIT,
    BR_ALLOC_OP_RESIZE,
    BR_ALLOC_OP_RESIZE_UNINIT,
    BR_ALLOC_OP_FREE,
    BR_ALLOC_OP_RESET
} br_alloc_op;

typedef struct br_alloc_request {
    br_alloc_op op;
    void *ptr;
    size_t old_size;
    size_t size;
    size_t alignment;
} br_alloc_request;

typedef struct br_alloc_result {
    void *ptr;
    size_t size;
    int status;
} br_alloc_result;

typedef br_alloc_result (*br_allocator_fn)(void *ctx, const br_alloc_request *req);

typedef struct br_allocator {
    br_allocator_fn fn;
    void *ctx;
} br_allocator;
```

The exact spellings can change, but the design intent should not:

- one allocator object
- one dispatch point
- explicit op and alignment
- no hidden global allocator lookup

## Arena Design

The first arena implementation should be a plain fixed-buffer bump allocator.

Needed operations:

- `br_arena_init`
- `br_arena_alloc`
- `br_arena_alloc_uninit`
- `br_arena_mark`
- `br_arena_rewind`
- `br_arena_reset`

Important semantics:

- alignment is explicit
- individual frees are unsupported
- rewind is only valid to a previously returned mark
- allocators are not thread-safe unless wrapped explicitly

## Scratch Design

Bedrock now also has a scratch allocator close to Odin's current shape.

- contiguous backing buffer allocated from a backup allocator
- last-allocation free/resize support
- oversized or non-fitting allocations spill to the backup allocator
- leaked backup allocations are tracked and cleaned up on reset/destroy

Important Bedrock-specific deviations from Odin for now:

- no ambient context allocator/logger; explicit `backup_allocator` defaults to
  `br_allocator_heap()` when unset
- no warning logger callback when scratch spills into the backup allocator
- misuse returns statuses instead of Odin's assertion/panic-heavy diagnostics

## Stack Design

Bedrock now also has a buffered stack allocator close to Odin's current shape.

- contiguous backing buffer supplied by the caller
- per-allocation headers stored immediately before user memory
- strict last-allocation free/resize semantics
- allocator adapter support for normal Bedrock allocation call sites

Important Bedrock-specific deviations from Odin for now:

- misuse returns statuses instead of Odin's panic-heavy diagnostics
- the allocator adapter currently targets Bedrock's alloc/free/resize/reset ABI,
  not Odin's richer query-features/query-info modes
- the resize path checks the current `prev_offset` so in-place resize follows
  the same last-allocation rule as free; this is documented because Odin's
  current resize code appears to compare against the header's previous offset
  instead

## Small Stack Design

Bedrock now also has a buffered small stack allocator close to Odin's current
shape.

- contiguous backing buffer supplied by the caller
- tiny per-allocation headers that only store padding
- out-of-order frees are allowed and rewind the running offset immediately
- later allocations can overwrite memory from allocations that used to follow
  the freed pointer

Important Bedrock-specific deviations from Odin for now:

- misuse returns statuses instead of Odin's panic-heavy diagnostics
- the allocator adapter currently targets Bedrock's alloc/free/resize/reset ABI,
  not Odin's richer query-features/query-info modes
- alignment still has to satisfy Bedrock's power-of-two allocator contract
  after the Odin-style clamp to the small-stack maximum

## Virtual Arena Design

Bedrock now also has a VM-backed arena layer. The intended v1 shape is:

- one cross-platform VM substrate with reserve/commit/decommit/release/protect
- one static virtual arena
- one growing virtual arena
- explicit allocator adapters and rewind markers

Important Bedrock-specific deviations from Odin for now:

- no buffer-backed variant in `virtual_arena`; fixed buffers stay in `br_arena`
- no built-in mutex; the arena is intentionally single-threaded for now
- overflow protection is currently exposed as an arena-level trailing guard page
  flag, not Odin's broader per-memory-block flag surface
- the VM backend now follows Odin's shared/per-OS split much more closely:
  `virtual_platform`, `virtual_posix`, `virtual_windows`, `virtual_linux`,
  `virtual_darwin`, `virtual_freebsd`, `virtual_netbsd`, `virtual_openbsd`,
  and `virtual_other`
- file mapping is still path-based only at the public API boundary; internally
  `virtual/file.c` now owns the high-level open/stat/map flow and the platform
  backends only perform native-handle mapping
- the public file-handle entry point remains a later `os/file` integration
  task
- `virtual/arena_util.odin` style typed convenience helpers are still absent;
  they should be added later as C-friendly inline or macro wrappers

## Tracking Allocator

Bedrock now also has a first tracking allocator layer. The intended v1 shape is:

- wrap an existing allocator
- track live allocations and cumulative totals
- keep a dense live-entry list for leak inspection
- use a private pointer index for fast lookup
- record bad frees for later inspection
- keep the wrapper explicit instead of relying on ambient context

Important Bedrock-specific deviations from Odin for now:

- no built-in mutex yet
- no exposed generic allocation map; Bedrock keeps a private pointer index
- no allocator feature queries, so `clear_on_reset` is an explicit policy flag
- no source-location tracking yet

## Rollback Stack Allocator

Bedrock now also has a rollback stack allocator layer. The intended v1 shape is:

- fixed head block plus optional dynamically chained blocks
- out-of-order frees that collapse freed tails
- in-place resize for the last allocation when possible
- allocator adapter support for normal Bedrock allocation call sites

Important Bedrock-specific deviations from Odin for now:

- initialization is split into explicit buffered and dynamic entry points
- invalid usage returns statuses instead of Odin's assertion-heavy diagnostics
- fallback resize copies `min(old_size, new_size)` bytes explicitly

## Low-Level Helper Scope

Bedrock should be selective about Odin's low-level `mem` helper surface.

- portable `mem.odin` helpers like byte set/zero/copy/compare/check-zero are
  reasonable targets
- `raw.odin` is mostly Odin-runtime-specific layout exposure and should not be
  treated as a direct Bedrock port target
- if Bedrock adds low-level helpers here later, they should stay focused on the
  portable subset rather than recreating Odin's `any`/slice/map runtime layouts

## Temporary Allocation

Odin gets excellent mileage from temporary allocators because the language helps
propagate them. C does not.

Bedrock should solve this with explicit arenas and savepoints instead:

- create a scratch arena per subsystem or frame
- pass the arena explicitly
- rewind at a clear ownership boundary

This gives most of the practical upside without any hidden thread-local rules.

## Thread Safety

Allocator objects should be non-thread-safe by default.

If callers want sharing, provide wrappers such as:

- `br_locked_allocator`
- per-thread arenas owned by user code

Do not make every allocator pay synchronization costs by default.
