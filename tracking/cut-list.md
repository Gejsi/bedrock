# Living Cut List

Bedrock ports Odin's stdlib, but the project's end goal explicitly includes a
post-port reckoning: parts that prove unnecessary get cut. This file is the
standing input to that decision. It is NOT a set of
decisions — final CUT/KEEP/DEMOTE calls land in the decisions log at the end of the
port. It exists so that decision is easy and evidence-based instead of vibes.

Updated as modules land. Analysis by the consumer-advocate seat.

## How to read this

- **CUT** — remove; nothing a C dev reaches for is lost, or it fights Bedrock's
  own design.
- **DEMOTE** — keep in the tree but out of the include-and-don't-think core:
  documented as opt-in/non-default (and excluded from any future bundled
  distribution, should one return).
- **KEEP** — earns its place in the batteries-included core.
- **Confidence** — how sure the advocate is, independent of how the project
  will weigh it.

Guiding tension: the project values include-and-don't-think ergonomics, so a fatter
library is fine *if every piece is obviously useful*. The enemy is not size; it
is choice-paralysis and museum pieces — code ported because it existed in Odin,
not because a C dev wants it.

Guiding philosophy (recorded July 19, 2026): **code is a liability.**
Cutting is good, and the same logic applies BEFORE porting — a package that
isn't worth porting should not be ported at all. This list therefore covers
both landed code (CUT/KEEP/DEMOTE) and, as a pre-port scrutiny section, the
remaining roadmap (PORT/SKIP/DEFER).

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
  design. KEEP only if the project wants an official malloc/free facade;
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
  bufio's peek). Two ways to peek erodes the don't-make-me-think goal. Revisit at first
  parser.
- **Update (July 19, 2026): the deferred test has now run, three times.** The
  wave-3 parser research traced all three parsers at the source level: csv
  builds on bufio.Reader read_slice/read_rune, the scanner does its own
  buffering over a raw reader, and ini iterates a string — NONE consumes
  lookahead_reader. Recommendation upgraded DEMOTE -> **CUT** (bufio.Reader
  peek + the future scanner cover every peek story). Decision
  pending in the log below.

## KEEP — narrower than peers

### varint / LEB128 (encoding pilot)

- Genuinely useful (DWARF/WASM/protobuf-style formats) but a narrower audience
  than hex/base64/endian, which nearly everyone hits. It was put in
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

## Sync extended primitives (demand-audited July 19, 2026)

Audited with the same steelman-keep-first, living-idiom-vs-relic method as the
allocators, plus the decisive in-repo consumer grep (all seven ARE exercised by
test_sync.c — unlike the niche allocators, none is consumer-less).

| Primitive | Rec | Confidence | One-line reason |
| --- | --- | --- | --- |
| Wait_Group | KEEP | high | canonical fan-out/fan-in (Go); pairs with br_thread |
| Once | KEEP | high | universal lazy-init idiom — Go/Rust/pthread all ship it |
| Parker | KEEP | high | Rust's core block primitive; composed-upon; carries the parker-timeout bug fix |
| Barrier | KEEP | med-high | Rust ships it; br_barrier is portable where pthread_barrier does not exist (absent on macOS, empirically verified) |
| One_Shot_Event | KEEP | med | common one-way latch/broadcast pattern; futex-backed |
| Auto_Reset_Event | KEEP | med | Win32-idiom but landed/tested/small; the swing item — first to DEMOTE if the sync surface is ever trimmed |
| Ticket_Mutex | DEMOTE | med | fairness is real-but-niche; no mainstream stdlib ships a fair mutex; keep as the documented specialist mutex, out of the default mental model |

Net: five keeps, two demote-class. The extended sync surface is mostly living
idioms with in-repo consumers; the genuinely dead weight in the library sits on
the allocator side (see the summary table above), where three candidates have
zero consumers AND a superseding Bedrock alternative.

## Roadmap "do not port on spec"

- **TLSF allocator** (deferred in the port matrix): if buddy is only
  DEMOTE-worthy, TLSF — another specialist allocator — should be DO-NOT-PORT
  unless a concrete consumer appears. Recommend marking "on demand only."
- **Odin `raw.odin` runtime-layout surface:** the matrix already says don't
  broadly port. Endorsed.

## If the project takes the high-confidence cuts

Cutting small_stack + compat + rollback removes ~1,267 impl+hdr lines and ~375
test lines, dropping the allocator count 13 -> 10 with zero capability a C dev
would miss. Demoting buddy + lookahead from the core dist trims another ~723
lines from the default surface while keeping them opt-in. That is the
biggest readability/dist win available, concentrated in the module that is
half the codebase.

## Pre-port scrutiny (roadmap, not-yet-ported)

"Code is a liability" applied BEFORE porting: if a row isn't worth porting,
strike it from the matrix now rather than port-then-cut. Sizes are Odin source
lines @2c25fb9. Recommendations: PORT (worth it), DEFER (only on concrete
demand), SKIP (strike from matrix).

### Matrix v1 rows not yet ported

| Row | One-liner | Realistic user | Rec | Confidence |
| --- | --- | --- | --- | --- |
| slashpath | pure lexical `/`-path clean/join/split/match | anyone handling URL-ish or virtual paths | PORT (designed) | high |
| csv (~16k) | RFC 4180 reader/writer | config/data-import code — very common | PORT | high |
| ini (~5k) | INI config reader | config loading — common, small | PORT | high |
| rfc3339 (~9k + drags datetime ~20k) | timestamp parse/format | logs, APIs, config timestamps | PORT (rfc3339 + minimal datetime ONLY) | medium |
| time: datetime (~20k) | calendar date/time record + civil math | date arithmetic | DEFER (except the slice rfc3339 needs) | medium |
| time: timezone (~41k) | IANA tz database + TZif parser + Windows tz | tz-aware conversion | SKIP | high |
| time: iso8601 (~5k) | ISO-8601 parse/format | overlaps rfc3339 | SKIP | medium |
| time: stopwatch | start/stop accumulator over tick | trivial timing | DEFER (tiny, on demand) | medium |
| time: TSC/perf (~7k + per-OS) | raw CPU timestamp counter | micro-benchmarking | SKIP | high |

Notes:

- **rfc3339 is the subtle one.** It imports `core:time/datetime`, so
  "rfc3339-only" really means rfc3339 plus the civil-date primitives it
  transitively calls (date/ordinal conversion, validation) — but NOT the
  timezone tarpit (rfc3339 handles offsets as fixed `+HH:MM`, no IANA db).
  Recommendation: port rfc3339 + only that datetime slice; defer the rest of
  datetime; skip timezone entirely.
- **timezone = hard SKIP.** ~41k lines including a bespoke TZif binary parser
  and a 16k Windows-tz mapping. OS tz databases are a versioning/maintenance
  tarpit with near-zero demand in a stdlib replacement's first life.
- **iso8601 SKIP** as duplicative of rfc3339 (port one timestamp format; keep
  the stricter, machine-oriented one).
- **TSC/perf SKIP:** per-arch asm, niche; `br_tick` already covers ordinary
  timing.

### Checklist `planned` rows inside landed modules

| Row | One-liner | Realistic user | Rec | Confidence |
| --- | --- | --- | --- | --- |
| bytes/strings: case conversion | ASCII+Unicode case ops | extremely common | PORT (ASCII now; Unicode tables deferred) | high |
| strings: cut / substring helpers | small ergonomic slicers | common | PORT | high |
| bytes/strings: fields / fields_proc | whitespace/predicate tokenize | common in parsing | PORT | high |
| bytes/strings: split iterators | allocation-free split loop | common; avoids list alloc | PORT | high |
| bytes/strings: trim cutset / space / null | trim by set/space | very common | PORT | high |
| bytes/strings: index_proc / trim_*_proc | predicate scans | niche vs cutset trims | DEFER | medium |
| strings: intern table | dedup strings into a pool | compilers/symbol tables | DEFER | medium |
| strings: snake/kebab case munging | identifier case conversion | codegen niche | SKIP | medium |
| bytes/strings: reverse | reverse bytes/runes | rare, trivial to inline | SKIP | medium |
| bytes/strings: scrub | replace invalid UTF-8 runs | overlaps to_valid_utf8; niche | SKIP | medium |
| bytes/strings: expand_tabs | tabs -> spaces | terminal pretty-print niche | SKIP | high |
| bytes/strings: justify | pad string to width | a fmt concern; fmt is excluded | SKIP | high |
| bytes/strings: levenshtein | edit distance | app-level, not stdlib | SKIP | medium |
| bufio: scanner | tokenized line/split reader | line-by-line input — common | PORT | high |
| sync: benaphores | cheap counting lock | redundant vs mutex/sema | SKIP | medium |
| sync: per-OS primitive split | Odin's per-OS file layout | refactor, no new capability | SKIP (as a goal) | high |
| sync: Haiku / WASM futexes | niche-target backends | nobody targets these in v1 | DEFER (on demand) | high |
| mem: low-level set/zero/copy/compare | portable mem helpers | used everywhere | PORT | high |

### Net

Hard SKIPs strike the timezone tarpit (~41k), iso8601, TSC/perf, the string
oddity cluster (justify, expand_tabs, scrub, snake/kebab, reverse,
levenshtein), sync benaphores, and the per-OS-split-as-a-goal. The rfc3339
"port the timestamp piece + minimal datetime slice" recommendation is the one
judgment call needing the project's explicit eye.

## Pre-port scrutiny — supporting evidence (verified)

### Two flavors of SKIP

- **SKIP — no in-library demand:** justify, expand_tabs, scrub, levenshtein,
  TSC/perf, iso8601, timezone-DB. None of the ubiquitous batteries-included C
  libs (stb, klib, sds) ship these; sds — the de-facto C string library — does
  split/join/trim/cat/tolower and stops. Strong signal C devs don't reach to a
  stdlib for these at all.
- **SKIP — real demand, owned by a named dedicated lib:** json (cJSON / jsmn /
  yyjson dominate). csv and ini have the same shape (libcsv, inih), but stay
  PORT per the project's accepted decision: small, dependency-free, and the
  "don't make me fetch a parser" story fits a stdlib replacement. If the
  project ever cuts deeper, "SKIP and point at inih/libcsv" is an
  evidence-backed fallback, not a gut call.

### rfc3339 minimal datetime closure (the concrete porter scope)

Traced at source level: rfc3339 always leaves `DateTime.tz` nil (fixed ±HH:MM
offsets are a plain integer; no tz database touched). The irreducible port is
the calendrical core only: Date/Time/DateTime/Delta/Ordinal/Error types +
bounds; date<->ordinal (Reingold-Dershowitz proleptic Gregorian) +
normalize_delta + divmod helpers; components_to_{date,time,datetime};
add_delta_to_datetime; subtract_datetimes; the validate family; is_leap_year.
DEFER: ordinal_to_datetime, day_of_week/day_number, last_day_of_month,
year_range, add_days_to_date, the gcd/lcm/interval_mod toolkit. SKIP: all of
timezone/ and every TZ_* type. This closure IS the wave-3 rfc3339 porter task
boundary.

## Decisions log

| Candidate | Decision | Date | Note |
| --- | --- | --- | --- |
| mem virtual file mapping API | CUT | 2026-07-19 | file APIs do not belong in the memory module; returns with a future os module if ever |
| time/timezone | SKIP | 2026-07-19 | pre-port strike; tz databases out of scope |
| time/iso8601 | SKIP | 2026-07-19 | rfc3339 is the one timestamp format |
| time/TSC-perf | SKIP | 2026-07-19 | br_tick suffices for ordinary timing |
| string oddities (justify, expand_tabs, scrub, snake/kebab, reverse, levenshtein) | SKIP | 2026-07-19 | pre-port strike |
| sync benaphores | SKIP | 2026-07-19 | redundant vs mutex/sema |
| sync per-OS primitive split | SKIP (as goal) | 2026-07-19 | capability already exists |
| rfc3339 + minimal datetime slice | PORT | 2026-07-19 | slice being mapped; datetime remainder DEFER |
| strconv, math/rand, uuid, base32 | PORT | 2026-07-19 | full-enumeration additions; strconv is the priority (checklist already depends on it) |
| core/hash | SKIP | 2026-07-19 | reversed same-day PORT: zlib crc32 / xxHash are already C; Odin's reimplementation motive (avoid foreign linking) is moot in a C library; hashmap needs are internal |
| encoding/uuid, encoding/base32 | DEFER | 2026-07-19 | walked back from same-day PORT: uuid waits on a concrete consumer (strongest deferred candidate — fragmented C ecosystem answer, proven Go/Rust demand); base32 at most rides base64, never standalone |
| core/thread | PORT (promoted from defer) | 2026-07-19 | threading prioritized; scoped per spec/modules/threading.md |
| math core float functions | SKIP | 2026-07-19 | libm is the answer; struck outright |
| text/regex | SKIP | 2026-07-19 | PCRE2 is the answer, as cJSON is for json |
| time/stopwatch | DEFER | 2026-07-19 | on concrete demand |
| niche-allocator cut executions | ON HOLD | 2026-07-19 | landed allocators stay in the tree; a use-case audit (steelman-the-keep-first) runs before any cut ruling |
| containers + sort redesign chapter | ORDERED LAST | 2026-07-19 | containers are the hardest, highest-caveat C surface — the chapter closes the port; nothing jumps it into an earlier slot |
| sync extended primitives | AUDIT ORDERED | 2026-07-19 | per-primitive living-idiom-vs-relic audit (ticket mutex, auto-reset event, one-shot event, public parker, barrier) before any trim ruling |
| demand audit (allocators + sync) | COMPLETE | 2026-07-19 | steelman-first evidence table recorded above: allocator side 3 CUT + 1 DEMOTE + lookahead CUT; sync side 5 KEEP + 2 DEMOTE-class. Execution rulings still pending. |
| small_stack | | | |
| compat_allocator | | | |
| rollback_stack | | | |
| buddy_allocator | | | |
| lookahead_reader | | | |
| varint | | | |
| tracking_allocator (debug-only dist?) | | | |
| TLSF (roadmap) | | | |
