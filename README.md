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
- modular source tree with generated stb-style release artifacts
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
- compatibility forwarding headers remain at the old flat include paths like `include/bedrock/bytes.h`
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

If a consumer does not want the short aliases like `u32` and `usize`, they can
define `BEDROCK_NO_SHORT_TYPES` before including Bedrock headers.
