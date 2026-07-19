# Living Cut List

Bedrock ports Odin's stdlib, but the maintainer's end goal explicitly includes a
post-port reckoning: "some of the stuff was just too much and i didn't really
care." This file is the standing input to that decision. It is NOT a set of
decisions — final CUT/KEEP/DEMOTE calls are the maintainer's at the end of the
port. It exists so that decision is easy and evidence-based instead of vibes.

Updated as modules land. Analysis by the consumer-advocate seat.

## How to read this

- **CUT** — remove; nothing a C dev reaches for is lost, or it fights Bedrock's
  own design.
- **DEMOTE** — keep in the tree but out of the "include and don't think" core:
  exclude from the default `dist/` amalgamation, available opt-in.
- **KEEP** — earns its place in the batteries-included core.
- **Confidence** — how sure the advocate is, independent of how the maintainer
  will weigh it.

Guiding tension: the maintainer values "include and don't think," so a fatter
library is fine *if every piece is obviously useful*. The enemy is not size; it
is choice-paralysis and museum pieces — code ported because it existed in Odin,
not because a C dev wants it.

## The framing fact

`mem` is ~5,900 lines, roughly **half of all of `src/`**. Bedrock currently
ships **13 allocators**. The daily-driver allocator experience Odin actually
optimizes for is: heap + a temp/scratch allocator + a tracking allocator in
debug + arenas. Everything past that is a specialist tool competing for the
same attention.

Decisive usage signal (verified July 19, 2026): grepping every candidate's
public prefix across `src/` and `tests/`, **no niche allocator is used anywhere
inside Bedrock itself** — each appears only in its own implementation plus its
own test (tracking_allocator is the one partial exception: also exercised by
test_dynamic_arena). These are leaf features, so keep/cut rests on plausible
external demand, judged per item.

## Summary table

| Candidate | One-liner | hdr+impl | rec | confidence |
| --- | --- | --- | --- | --- |
| small_stack | LIFO allocator with tiny headers; a micro-variant of `stack` | 307 | CUT | high |
| compat_allocator | wraps a parent so you can free/resize without passing old_size | 277 | CUT | high (philosophy call) |
| rollback_stack | stack allocator with savepoint rollback + oversized singletons | 683 | CUT | medium |
| buddy_allocator | power-of-two buddy allocator over a fixed buffer | 474 | DEMOTE | medium |
| lookahead_reader | reader that peeks N bytes ahead then consumes | 249 | DEMOTE | medium |
| varint (LEB128) | variable-length integer encode/decode | pilot | KEEP (narrow) | medium |
| scratch | temp/region allocator with backup + leak list | 507 | KEEP | high |
| tracking_allocator | debug leak/double-free tracker | 887 | KEEP (debug-only) | high |
| mutex_allocator | serialize any allocator behind a mutex | 83 | KEEP | high |
| stack | canonical LIFO allocator | 350 | KEEP | high |
| dynamic_arena | portable growing arena (heap-block chaining) | 519 | KEEP | high |

## CUT — high confidence

### small_stack (307 lines, 79 test)

- **What:** a stack (LIFO) allocator like `stack`, but with smaller per-alloc
  headers and out-of-order free.
- **Realistic user:** nobody in C deliberately picks "small stack" over
  "stack"; the header-size delta is an Odin micro-optimization, not a use case.
- **Cost:** 386 lines + a public type + a test, maintained forever.
- **Why cut:** shipping both `stack` and `small_stack` is the clearest
  redundancy in the library. Keep `stack` as the one canonical LIFO allocator.

### compat_allocator (277 lines, 163 test)

- **What:** wraps a parent allocator and records each allocation's size so
  callers can free/resize WITHOUT passing `old_size`.
- **Realistic user:** only code bridging to a malloc/free-style sizeless-free
  world. Bedrock's ABI deliberately carries `old_size` everywhere, so inside
  Bedrock's own philosophy this solves a problem the library chose not to have.
- **Cost:** 277 lines + a semantically subtle header-shifting resize path.
- **Why cut:** it's an escape hatch *against* foundation.md's explicit-size
  design. KEEP only if the maintainer wants an official malloc/free facade;
  otherwise it cuts cleanly. This is the one call that hinges on a philosophy
  decision, not a usage one.

## CUT — medium confidence

### rollback_stack (683 lines, 133 test — largest niche allocator)

- **What:** a stack allocator that also rolls back to a savepoint and serves
  oversized allocations as standalone singleton blocks.
- **Realistic user:** a narrow niche between "arena with mark/rewind" (already
  in Bedrock) and "stack." The rollback capability substantially overlaps
  arena savepoints.
- **Cost:** 683 lines — the biggest maintenance surface of the niche
  allocators, for the least differentiated capability.
- **Why cut:** highest cost-to-uniqueness ratio in the allocator set. If any
  niche allocator goes, this frees the most weight.

## DEMOTE — medium confidence

### buddy_allocator (474 lines, 101 test)

- **What:** a power-of-two buddy allocator with block splitting/coalescing over
  a fixed backing buffer.
- **Realistic user:** a real, recognizable structure people occasionally want
  for pool/subsystem memory with bounded fragmentation — but not daily-driver.
- **Why demote not cut:** unlike small_stack/rollback, "buddy allocator" is a
  thing C devs know and sometimes specifically seek. Keep it in the tree, out
  of the default dist. Demoting it also settles the TLSF question (below).

### lookahead_reader (249 lines, 94 test)

- **What:** a thin reader that buffers and peeks N bytes ahead, then consumes.
- **Realistic user:** parser authors — and the parser family IS coming
  (csv/ini/json). BUT `bufio.Reader` already ships peek/buffered/discard, so
  Bedrock has two peek stories.
- **Why demote:** decide, when the first parser lands, whether it builds on
  bufio.Reader.peek (then cut lookahead) or on the standalone (then narrow
  bufio's peek). Two ways to peek erodes "don't think." Revisit at first
  parser.

## KEEP — narrower than peers

### varint / LEB128 (encoding pilot)

- Genuinely useful (DWARF/WASM/protobuf-style formats) but a narrower audience
  than hex/base64/endian, which nearly everyone hits. The maintainer put it in
  the pilot deliberately, so keep — but it's the first encoding to demote if
  the pilot ever needs trimming.

## KEEP — high confidence

- **scratch (507):** the temp-allocator story foundation.md explicitly calls
  for. Core.
- **tracking_allocator (887, largest single allocator):** the debug
  leak/double-free tool; already used cross-module by test_dynamic_arena.
  Core, but a candidate to EXCLUDE FROM RELEASE dist (debug-build-only) to
  trim shipping weight without losing the capability.
- **mutex_allocator (83):** "make any allocator thread-safe." Instantly
  understandable, near-zero cost. Cheapest keep in the library.
- **stack (350):** the canonical LIFO allocator (keep this, cut small_stack).
- **dynamic_arena (519):** portable growing arena (heap-block chaining) vs
  virtual_arena's VM-backed growth (927). Two growing-arena stories is a mild
  smell, but portable-vs-VM is a legitimate split (no syscalls vs huge
  reserves). Keep both; note the overlap.

## Roadmap "do not port on spec"

- **TLSF allocator** (deferred in the port matrix): if buddy is only
  DEMOTE-worthy, TLSF — another specialist allocator — should be DO-NOT-PORT
  unless a concrete consumer appears. Recommend marking "on demand only."
- **Odin `raw.odin` runtime-layout surface:** the matrix already says don't
  broadly port. Endorsed.

## If the maintainer takes the high-confidence cuts

Cutting small_stack + compat + rollback removes ~1,267 impl+hdr lines and ~375
test lines, dropping the allocator count 13 -> 10 with zero capability a C dev
would miss. Demoting buddy + lookahead from the core dist trims another ~723
lines from the "don't think" surface while keeping them opt-in. That is the
biggest readability/dist win available, concentrated in the module that is
half the codebase.

## Decisions log (maintainer fills in at end of port)

| Candidate | Decision | Date | Note |
| --- | --- | --- | --- |
| small_stack | | | |
| compat_allocator | | | |
| rollback_stack | | | |
| buddy_allocator | | | |
| lookahead_reader | | | |
| varint | | | |
| tracking_allocator (debug-only dist?) | | | |
| TLSF (roadmap) | | | |
