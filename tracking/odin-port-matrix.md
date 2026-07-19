#Odin Port Matrix

This file tracks which Odin packages are candidates for Bedrock, which ones
need redesign, and which ones should stay out of scope.

Detailed status for the currently active module ports lives in
`tracking/odin-coverage-checklist.md`.

Priority (July 19, 2026): complete and audit the landed core
modules before expanding new module families. Encodings base64/varint and the
parser wave (csv/ini/scanner) are queued behind core completion.

| Odin package | Decision | Notes |
| --- | --- | --- |
| `core/unicode/utf8` | partial v1 | Self-contained UTF-8 encode/decode/validate/count foundation landed without vendor dependencies; higher Unicode tables and case folding are still separate work. |
| `core/bytes` | partial v1 | Slice/view model, buffer, reader, split, and replace/remove family landed, including Odin-style rune-aware empty-substring behavior; remaining gaps are mostly explicit rune helpers and some convenience APIs. |
| `core/strings` | partial v1 | String view/owned string layer, builder, reader, split, and replace/remove helpers landed with UTF-8-aware rune search/count behavior; broader conversion, iterator, and Unicode-table work still remains. |
| `core/io` | partial v1 | Odin-style single stream proc plus in-memory adapters landed, including generic byte/rune/exact-read-write/copy helpers and `read_at`/`write_at`/`size` fallbacks; buffered utilities still remain above it. |
| `core/bufio` | partial v1 | Buffered reader, writer, read-writer, transfer helpers, and lookahead reader landed on top of `io`; scanner and some convenience helpers still remain. |
| `core/mem` | partial v1 | Allocators, fixed arenas, scratch allocator, stack allocator, small stack allocator, dynamic arena allocator, buddy allocator, compat allocator, mutex allocator, rollback stack allocator, tracking allocator with Odin-style locking, and an Odin-shaped `virtual/*` split for cross-platform virtual memory, mutex-protected virtual growing/static arenas, temp helpers, trailing guard-page overflow protection, and path-based file mapping landed; specialized allocators still remain. |
| `core/encoding/base64` | v1 | Straightforward table-driven code. |
| `core/encoding/hex` | v1 | Straightforward. |
| `core/encoding/endian` | v1 | Straightforward. |
| `core/encoding/varint` | v1 | Straightforward. |
| `core/encoding/csv` | v1 | Reasonable parser target. |
| `core/encoding/ini` | v1 | Reasonable parser target. |
| `core/path/slashpath` | v1 | Good portable path layer. |
| `core/time` | partial v1 | Minimal duration/time/tick/sleep foundation landed to unblock sync timeouts. After the July 19, 2026 pre-port scrutiny: only the minimal datetime slice rfc3339 needs remains in scope; stopwatch is deferred on demand; timezone, iso8601, and TSC/perf are struck (see `tracking/cut-list.md`). |
| `core/time/rfc3339` | v1 | Useful focused formatter/parser; ports with only the minimal datetime slice it transitively needs (fixed-offset timestamps, no timezone database). |
| `core/math/bits` | v1 | Mostly portability shims and bit helpers. |
| `core/strconv` | v1 (landed July 19, 2026) | Locale-free int/bool/float parse and format over one Float_Info-parameterized decimal engine per spec/modules/strconv.md. Strict-by-default parse (`_prefix` permissive), native-f32 rounding (no f64 double-round), saturating `OUT_OF_RANGE`, never-truncate format. Validated against Go's Paxson vectors. |
| `core/container/*` | redesign | Keep the ideas; implement as generated typed containers. Demand-ranked coverage for the redesign (from the July 19, 2026 evidence survey): dynamic array, queue, priority_queue, small_array, bit_array are the common set; pick ONE balanced tree (Odin ships both avl and rbtree); lru/pool/handle_map opt-in; topological_sort/xar/intrusive specialist. |
| `core/sort` | redesign | Use erased generic algorithms plus optional typed sugar. |
| `core/thread` | v1 (promoted July 19, 2026) | Minimal explicit thread layer per spec/modules/threading.md — create/join/detach/yield over pthreads/Win32, user data by pointer, no context inheritance, pools later. Prerequisites now met (sync complete and audited); Bedrock's own tests become the first consumer, replacing their raw pthread guards. |
| `core/sync` | partial v1 | Core blocking primitives, public `Sema`, a useful first extended slice, `sync/atomic`, native thread IDs, Linux/Windows/Darwin/FreeBSD/NetBSD/OpenBSD futex wait/timeout/wake, `Atomic_Mutex`, `Atomic_RW_Mutex`, `Atomic_Recursive_Mutex`, `Atomic_Cond`, `Atomic_Sema`, public timeout waits, `Auto_Reset_Event`, `Parker`, and `One_Shot_Event` landed. Public primitives now delegate to the atomic/futex layer and are zero-value-ready; Haiku/WASM futex backends, missing extended primitives, and Odin's real per-OS primitive tree still remain. |
| `core/fmt` | exclude v1 | Too tied to `any`, RTTI, and formatter dispatch. |
| `core/encoding/json` | exclude v1 | Too RTTI-heavy for a clean first pass. |
| `core/encoding/xml` | exclude v1 | Large parser surface; not first-wave material. |
| `core/flags` | exclude v1 | Struct-tag and RTTI heavy. |
| `core/reflect` | exclude v1 | Language/runtime feature, not a library fit. |
| `core/os`, `core/sys`, `core/net` | exclude v1 | Too platform-heavy for initial scope. |
| `core/crypto/*` | exclude v1 | Large and security-sensitive. |
| `core/compress/*` | exclude v1 | Valuable, but not core bootstrap material. |
| `core/image/*` | exclude v1 | Large and specialized. |
| `base/runtime`, `base/builtin`, `base/intrinsics` | exclude | Compiler/runtime substrate, not library surface. |
| `core/c/libc` | reference only | Useful as compatibility/reference material, not the main target. |

## Complete core/ enumeration (July 19, 2026)

The rows above were a selection; the rows below complete the enumeration so
every Odin `core/` package has an explicit decision. Recommendations pending
the final ruling use the cut-list methodology; sizes are Odin source
lines at `2c25fb9`.

### PORT (accepted July 19, 2026)

| Package | Size | Rationale |
| --- | --- | --- |
| `core/strconv` | 3180 | Number/string parse and format — the most-reached-for stdlib facility C under-serves (strtol/snprintf are clumsy and locale-tainted). Was the highest-value un-ported package; LANDED July 19, 2026 (see the v1 row above). |
| `core/math/rand` | 2235 | Seedable PRNG — a genuine C gap (rand() is bad and global). Focused core: generator + int/float/range/shuffle; distributions on demand. |

### DEFER (gate on a concrete consumer)

| Package | Size | Rationale |
| --- | --- | --- |
| `core/slice` | 2167 | Generic slice algorithms; folds into the planned container/sort redesign rather than a standalone port. |
| `core/log` | 1130 | Useful; couples to output policy; add on demand. |
| `core/text/scanner` | 667 | Overlaps the planned bufio scanner; decide one home when the parser family lands. |
| `core/path/filepath` | subtree | OS-aware paths; needs the excluded os layer. `br_path_` stays reserved. |
| `core/unicode` tables | large | The property/case-fold tables several shipped deviations explicitly wait on. A dedicated future wave, not excluded. |
| `core/encoding/cbor` | 3806 | Real but niche binary serialization; below json in demand. |
| `core/encoding/uuid` | 1058 | Deferred July 19, 2026 (walked back from same-day PORT). Strongest deferred candidate: no canonical portable C answer (libuuid is a fragmented system dependency) and demand proven by Go/Rust, where uuid lives outside std yet tops import charts. Scoped v4 + v7 needs only rand + time; v4 randomness sourcing (PRNG vs OS entropy per RFC 9562) is the open design question when a concrete consumer revives it. |
| `core/encoding/base32` | 231 | Deferred July 19, 2026 (walked back from same-day PORT). Demand is a fraction of base64's (TOTP secrets, DNS-safe names); viable only as a near-free rider on base64's table-driven skeleton, so it is decided with base64, never standalone. |
| `core/unicode/utf16` | subtree | Windows/JS interop transcode; add when a consumer hits it. |

### SKIP (ruled July 19, 2026)

| Package | Rationale |
| --- | --- |
| `core/math` (float functions, 4471 lines) | libm already ships all of it and every C program links it; reimplementing adds nothing for Bedrock's goals. Struck outright. |
| `core/hash` (3629 lines) | The C ecosystem already provides these in C (zlib's crc32, the vendorable single-file xxHash); Odin reimplemented them mainly to avoid foreign linking, a motive that is moot in a C library. The future hash map uses a small internal hash function, not a public package. Struck July 19, 2026 (reversing the same-day PORT). |
| `core/text/regex` (4068 lines) | Genuine demand, but a correctness-and-fuzz mini-language of its own; the ecosystem answer is PCRE2, the same way json's answer is cJSON. Struck. |
| `core/text/i18n`, `text/match`, `text/edit`, `text/table` | Application/presentation concerns, not stdlib primitives. |
| `core/encoding/asn1`, `encoding/pem` | Crypto/PKI-adjacent; belong with the excluded crypto surface. |
| `core/encoding/entity`, `encoding/hxa` | Generated HTML-entity data (xml is excluded) and a niche 3D asset format. |
| `core/math/big`, `math/cmplx`, `math/linalg`, `math/noise`, `math/ease`, `math/fixed` | Specialist math (bignum, complex, graphics, animation, fixed-point). |
| `core/rexcode` | 186k lines of generated regex/Unicode data; not hand-portable surface. |
| `core/simd` | Compiler/architecture substrate, not portable C11 surface. |
| `core/nbio`, `core/dynlib`, `core/terminal` | Platform-heavy (event loops, dlopen, ANSI control) — same tier as excluded os/net. |
| `core/testing`, `core/prof`, `core/debug` | Tooling, not consumer-facing library surface. |
| `core/odin` | Odin's own AST/parser — meaningless in a C library. |
| `core/relative` | Odin-runtime pointer idiom with no C fit. |
