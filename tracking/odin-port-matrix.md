#Odin Port Matrix

This file tracks which Odin packages are candidates for Bedrock, which ones
need redesign, and which ones should stay out of scope.

Detailed status for the currently active module ports lives in
`tracking/odin-coverage-checklist.md`.

| Odin package | Decision | Notes |
| --- | --- | --- |
| `core/unicode/utf8` | partial v1 | Self-contained UTF-8 encode/decode/validate/count foundation landed without vendor dependencies; higher Unicode tables and case folding are still separate work. |
| `core/bytes` | partial v1 | Slice/view model, buffer, reader, split, and replace/remove family landed, including Odin-style rune-aware empty-substring behavior; remaining gaps are mostly explicit rune helpers and some convenience APIs. |
| `core/strings` | partial v1 | String view/owned string layer, builder, reader, split, and replace/remove helpers landed with UTF-8-aware rune search/count behavior; broader conversion, iterator, and Unicode-table work still remains. |
| `core/io` | partial v1 | Odin-style single stream proc plus in-memory adapters landed, including generic byte/rune/exact-read-write/copy helpers and `read_at`/`write_at`/`size` fallbacks; buffered utilities still remain above it. |
| `core/bufio` | partial v1 | Buffered reader, writer, read-writer, transfer helpers, and lookahead reader landed on top of `io`; scanner and some convenience helpers still remain. |
| `core/mem` | partial v1 | Allocators, fixed arenas, scratch allocator, stack allocator, small stack allocator, dynamic arena allocator, buddy allocator, rollback stack allocator, tracking allocator, and an Odin-shaped `virtual/*` split for cross-platform virtual memory, virtual growing/static arenas, temp helpers, trailing guard-page overflow protection, and path-based file mapping landed; specialized allocators still remain. |
| `core/encoding/base64` | v1 | Straightforward table-driven code. |
| `core/encoding/hex` | v1 | Straightforward. |
| `core/encoding/endian` | v1 | Straightforward. |
| `core/encoding/varint` | v1 | Straightforward. |
| `core/encoding/csv` | v1 | Reasonable parser target. |
| `core/encoding/ini` | v1 | Reasonable parser target. |
| `core/path/slashpath` | v1 | Good portable path layer. |
| `core/time/rfc3339` | v1 | Useful focused formatter/parser. |
| `core/math/bits` | v1 | Mostly portability shims and bit helpers. |
| `core/container/*` | redesign | Keep the ideas; implement as generated typed containers. |
| `core/sort` | redesign | Use erased generic algorithms plus optional typed sugar. |
| `core/thread` | defer | Later module; no direct v1 port. |
| `core/sync` | defer | Useful later, but depends on the thread story. |
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
