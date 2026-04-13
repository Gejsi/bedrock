# Bedrock

Bedrock is an Odin-inspired C library project aimed at replacing the parts of the
C standard library that are awkward, underspecified, or missing entirely.

The repo is in bootstrap mode. Odin is pinned as a reference submodule in
`upstream/odin`, the design work is intentionally split into small documents
instead of one giant context file, and the first C slice is already buildable.

Primary docs:

- `spec/foundation.md`
- `spec/modules/memory.md`
- `spec/modules/generics.md`
- `spec/modules/threading.md`
- `tracking/odin-port-matrix.md`
- `decisions/ADR-0001-upstream-layout.md`

Current direction:

- explicit allocators instead of hidden ambient context
- modular source tree with generated stb-style release artifacts
- honest C APIs where Odin semantics do not map cleanly

Current implemented bootstrap:

- central short-type aliases in `include/bedrock/types.h`
- allocator ABI in `include/bedrock/alloc.h` and `src/alloc.c`
- fixed-buffer arena in `include/bedrock/arena.h` and `src/arena.c`
- `bytes` slice/view helpers in `include/bedrock/bytes.h` and `src/bytes.c`
- `bytes.Buffer`-style mutable storage in `include/bedrock/byte_buffer.h` and `src/byte_buffer.c`
- typed vector template in `include/bedrock/template/vec.h`
- smoke tests in `tests/test_bootstrap.c`, `tests/test_bytes.c`, and `tests/test_byte_buffer.c`

Build and test:

```sh
make test
```

Build modes:

```sh
make MODE=debug test
make MODE=release test
make MODE=asan test
make MODE=ubsan test
```

Convenience targets:

```sh
make debug
make release
make asan
make ubsan
```

The `Makefile` discovers `src/**/*.c` and `tests/**/*.c` automatically, so new
modules and tests do not need to be hardcoded into the build.

If a consumer does not want the short aliases like `u32` and `usize`, they can
define `BEDROCK_NO_SHORT_TYPES` before including Bedrock headers.
