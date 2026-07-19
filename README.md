# Bedrock

Bedrock is an Odin-inspired C library project aimed at replacing the parts of the
C standard library that are awkward, underspecified, or missing entirely.

The repo is in bootstrap mode. Odin is pinned as the primary reference
submodule in `upstream/odin`, Rust is pinned as a secondary implementation
reference in `upstream/rust`, the design work is intentionally split into small
documents instead of one giant context file, and the first C slice is already
buildable.

Primary docs:

- `spec/foundation.md`
- `spec/modules/memory.md`
- `spec/modules/generics.md`
- `spec/modules/threading.md`
- `tracking/odin-port-matrix.md`
- `decisions/ADR-0001-upstream-layout.md`
- `decisions/ADR-0002-rust-reference.md`

Current direction:

- explicit allocators instead of hidden ambient context
- modular source tree consumed by vendoring and static linking
- honest C APIs where Odin semantics do not map cleanly

Current implemented bootstrap:

- central short-type aliases in `include/bedrock/types.h`
- allocator ABI in `include/bedrock/mem/alloc.h` and `src/mem/alloc.c`
- fixed-buffer arena in `include/bedrock/mem/arena.h` and `src/mem/arena.c`
- mutex allocator wrapper in `include/bedrock/mem/mutex_allocator.h` and `src/mem/mutex_allocator.c`
- UTF-8 foundation in `include/bedrock/unicode/utf8.h` and `src/unicode/utf8.c`
- `bytes` slice/view helpers in `include/bedrock/bytes/bytes.h` and `src/bytes/bytes.c`
- `bytes.Buffer`-style mutable storage in `include/bedrock/bytes/buffer.h` and `src/bytes/buffer.c`
- `bytes.Reader`-style read-only cursor in `include/bedrock/bytes/reader.h` and `src/bytes/reader.c`
- `strings` slice/view helpers in `include/bedrock/strings/strings.h` and `src/strings/strings.c`
- `strings.Builder`-style mutable storage in `include/bedrock/strings/builder.h` and `src/strings/builder.c`
- `strings.Reader`-style read-only cursor in `include/bedrock/strings/reader.h` and `src/strings/reader.c`
- typed vector template in `include/bedrock/template/vec.h`
- a three-tier include layout: canonical per-file headers at
  `include/bedrock/<module>/<file>.h`, one convenience umbrella per module at
  `include/bedrock/<module>.h` (e.g. `bedrock/mem.h`, `bedrock/strings.h`), and
  the master `include/bedrock.h` that pulls in every module umbrella
  (`bedrock/template/vec.h` stays a direct include because it is a macro template)
- smoke and module tests live under `tests/`

Build and test:

```sh
make test
```

Build modes:

```sh
make MODE=debug test
make MODE=release test
make MODE=sanitize test
```

Convenience targets:

```sh
make debug
make release
make sanitize
```

The `Makefile` discovers `src/**/*.c` and `tests/**/*.c` automatically, so new
modules and tests do not need to be hardcoded into the build.

## Using Bedrock

Bedrock is consumed as vendored source with static linking — the right default
for a foundation library: the linker drops objects you do not reference, and a
meaningful part of the API is inline code that compiles into your program
anyway. There are no prebuilt artifacts yet. Once the library cuts versioned
releases, the plan is raylib-style per-platform archives (`include/` +
`libbedrock.a`, checksummed) on the releases page, with dynamic builds possibly
following as optional, version-locked artifacts — Bedrock does not promise a
stable ABI across versions, so any future `.so`/`.dll` is pinned to its
release rather than swappable underneath your program.

1. Vendor the repository into your project — copy the tree or add it as a git
   submodule.
2. Build the static library: `make` (or `make MODE=release`) produces
   `build/<mode>/lib/libbedrock.a`.
3. Compile your code against the headers with `-I<bedrock>/include` and link the
   archive:

   ```sh
   cc -I<bedrock>/include my_app.c <bedrock>/build/release/lib/libbedrock.a -pthread
   ```

   (`-pthread` is needed because the `sync` and `time` modules use it.)

Linking the whole `libbedrock.a` is the zero-thought default: the linker drops
objects you do not reference. If you want a smaller build, compile only the
`src/` subdirectories you use. A module needs itself plus everything it depends
on (transitively). Verified direct dependencies:

| Module | Directly depends on |
| --- | --- |
| `types`, `base` | (none) |
| `time` | (none) |
| `unicode` | `bytes` |
| `bytes` | `unicode`, `mem`, `io` |
| `io` | `unicode` |
| `mem` (arenas + core allocators) | (none beyond `base`) |
| `mem` (`mutex_allocator`, `tracking_allocator`) | `sync` (which pulls `time`) |
| `sync` | `time` |
| `strings` | `unicode`, `io` |
| `bufio` | `bytes`, `io`, `mem`, `strings` |
| `encoding` | `bytes`, `io` |
| `path` | `strings`, `unicode` |

`bytes` and `unicode` are mutually dependent (a rune-aware `bytes` layer over a
`bytes`-view UTF-8 decoder), so they always compile together. Within `mem`,
only `mutex_allocator` and `tracking_allocator` need `sync` (for their embedded
mutex); the other nine allocators — including every arena — link against
`base` alone, so "I just want an arena" costs nothing beyond `mem` itself.
When in doubt, link the whole archive.

The public ABI is spelled entirely in standard C types (`size_t`, `uint32_t`,
`int64_t`, `double`, ...). The short aliases like `u32` and `usize` are optional
sugar layered on top in `include/bedrock/types.h`. A consumer that does not want
them can define `BEDROCK_NO_SHORT_TYPES` before including Bedrock headers; the
headers still compile because they never depend on the aliases. This is
supported and CI-tested by `tests/test_no_short_types.c`, which builds against
`<bedrock.h>` with the aliases disabled.
