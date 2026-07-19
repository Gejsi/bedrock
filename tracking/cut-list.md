# Cut List

Living draft of what Bedrock could remove or demote after the Odin port.
Recommendations come from the consumer-advocate review seat; **decisions are
the maintainer's** and are recorded in the decisions log at the bottom.

Framing facts (verified July 19, 2026):

- `mem` is ~5,900 lines, roughly half of all of `src/`. The allocator set, not
  the modules a C developer reaches for daily, is the library's center of mass.
- None of the niche allocators below is used anywhere inside Bedrock itself —
  each appears only in its own source file and its own test. The lone
  exception: `tracking_allocator` is also consumed by `test_dynamic_arena`.
- The Odin daily-driver experience is heap + scratch/temp + tracking-in-debug +
  arenas. Bedrock currently ships thirteen allocators.

## Ranked recommendations

| Candidate | What it is | Realistic user | Lines (impl+test) | Rec | Confidence |
| --- | --- | --- | --- | --- | --- |
| small_stack | `stack` with smaller headers + out-of-order free | nobody picks it over `stack` deliberately | 307+79 | CUT | high |
| compat_allocator | wrapper that remembers sizes so callers can free without `old_size` | only a malloc/free facade; fights the explicit-`old_size` ABI | 277+163 | CUT | high* |
| rollback_stack | stack + savepoint rollback + oversized singleton blocks | thin niche between arena mark/rewind and stack | 683+133 | CUT | medium |
| buddy_allocator | power-of-two buddy with split/coalesce | recognized structure, occasionally sought for bounded fragmentation | 474+101 | DEMOTE (opt-in, out of default dist) | medium |
| bufio lookahead_reader | peek-N-then-consume reader | parser authors — but `bufio.Reader` already has peek/discard | 249+94 | DEMOTE, reconcile when first parser lands | medium |
| varint (LEB128) | integer varint codec | format authors (DWARF/WASM/protobuf-adjacent); narrower than hex/base64/endian | pilot | KEEP, first to demote if trimming | medium |
| scratch | the temp-allocator story foundation.md calls for | everyone | 507 | KEEP | high |
| tracking_allocator | debug leak/double-free tracking | everyone in debug; consider excluding from release dist | 887 | KEEP (debug-focused) | high |
| mutex_allocator | make any allocator thread-safe | obvious, tiny | 83 | KEEP | high |
| stack | the canonical LIFO allocator | yes (keep exactly one LIFO story) | 350 | KEEP | high |
| dynamic_arena | portable heap-block growing arena | legit split vs VM-backed `virtual_arena` (no syscalls) | 519 | KEEP, overlap noted | high |

*compat_allocator is the one philosophy call: keep only if an official
malloc/free-shaped facade is wanted; otherwise it cuts clean.

## Notes

- Cutting the three high-confidence rows removes ~1,270 implementation lines
  and drops the allocator count 13 -> 10 without losing a capability a C
  developer would miss; demoting buddy + lookahead trims another ~720 lines
  from the "include and don't think" core while keeping them opt-in.
- Two-of-everything smells: `stack` vs `small_stack` (resolve by cutting
  small_stack); `dynamic_arena` vs `virtual_arena` (both justified: portable vs
  VM-backed); `bufio.Reader.peek` vs `lookahead_reader` (resolve when the
  first parser — csv or ini — picks its dependency).
- TLSF (currently "deferred" in the port matrix): if buddy is only
  demote-worthy, another specialized allocator should be DO-NOT-PORT unless a
  concrete consumer appears. Treat as on-demand only.
- Odin `raw.odin` runtime-layout surface: matrix already excludes a broad
  port; endorsed.

## Decisions log (maintainer)

| Date | Candidate | Decision | Notes |
| --- | --- | --- | --- |
| | | | |
