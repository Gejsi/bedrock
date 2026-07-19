# Bedrock

Odin-inspired C11 library meant to replace the awkward, underspecified, or
missing parts of the C standard library. `upstream/odin` (git submodule) is the
API and semantics reference; `upstream/rust` (sparse, `library/` only;
ADR-0002) and `upstream/go` (sparse, `src/` only; ADR-0005) are pinned
implementation references — Go is the design ancestor for the encodings,
slashpath, io/bufio, utf8, and bits surfaces and the source of their canonical
test vectors. Other runtimes (musl, glibc, ...) are fair references too — the
API contract stays Odin's.

## Read first

- `spec/foundation.md` — project-wide rules: naming, allocator policy, error model
- `spec/modules/<module>.md` — design notes for the module you are touching
- `tracking/odin-port-matrix.md` — the scope contract
- `tracking/odin-coverage-checklist.md` — per-area port status

## Build and test

- `make test` builds the library and runs every test binary (MODE=debug default)
- Modes: `MODE=debug`, `MODE=release`, `MODE=sanitize` (ASan+UBSan), `MODE=thread-sanitize` (TSan)
- Run `make MODE=sanitize test` before calling any change done; also
  `MODE=thread-sanitize` when touching `sync`, `time`, or allocator locking
- `make format` before committing; CI enforces `make check-format`
- Sources and tests are auto-discovered (`src/**/*.c`, `tests/*.c`); new files
  need no Makefile edits
- The build toolchain is C + make ONLY: host tools live in `tools/*.c` and are
  compiled by the Makefile. Never introduce script-language build dependencies
  (python, shell beyond POSIX make recipes, etc.) — maintainer directive

## Conventions

- C11; public headers must not require compiler extensions
- Type spelling is two-layered BY DESIGN: public headers under `include/` are
  spelled in standard C types (`size_t`, `uint32_t`, ...) — that is the ABI
  rule; implementation and test code (`src/`, `tests/`) PREFERS the short
  aliases (`usize`, `u32`, ...) — that is the maintainer's style
- Public header comments PRESENT the API to consumers: describe behavior and
  contracts only. Never reference Odin, Go, Rust, or porting provenance in
  them — a consumer does not care where the design came from. Provenance and
  deviation notes live in `spec/` and `tracking/`
- Public API naming favors what C developers already know over source-language
  naming when the two conflict (`br_mem_copy`/`br_mem_move` mirror
  memcpy/memmove, not Odin's copy/copy_non_overlapping)
- Prefixes: `br_` public ABI, `BR_` macros, `br__` internal symbols
- Errors are explicit `br_status` returns; library code never panics or aborts
- Allocation is explicit: pass `br_allocator` at API boundaries; no globals,
  no hidden ambient context
- Follow Odin semantics; record every intentional deviation in the module's
  `spec/modules/*.md` and `tracking/odin-coverage-checklist.md` in the same
  commit as the code
- Research briefs and API designs ALWAYS compare against the reference
  ecosystems — Odin (the contract), Go and Rust (pinned in `upstream/`), and
  where relevant glibc/musl or the C standard itself — stating what each does
  and which model Bedrock follows and why. "Checked X: not applicable because
  Y" counts; silence does not
- Suspected upstream Odin bugs go in `tracking/odin-suspected-bugs.md`
- File-local `static` helpers carry THEIR OWN module prefix (`br__hex_result`
  in hex.c, never a borrowed `br__bytes_result`): any unity or amalgamated
  build folds sources into one translation unit, where identical static names
  across files become redefinition errors — keep every file's statics unique
- Keep docs small and focused; never grow a single giant planning file

## Team workflow (agent teammates)

- The team lead owns `main`: only the lead merges, commits, and pushes
- Implementers work inside their assigned git worktree and touch only the
  files their task names
- Read `upstream/` from the main checkout at `/Users/gejsi/Desktop/bedrock`
  (submodules are not initialized inside worktrees)
- Keep your task status current; message the lead when blocked instead of
  going idle silently
- Commit style (lead only): short imperative subject, no body, no trailers
