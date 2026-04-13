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

- allocator ABI in `include/bedrock/alloc.h` and `src/alloc.c`
- fixed-buffer arena in `include/bedrock/arena.h` and `src/arena.c`
- typed vector template in `include/bedrock/template/vec.h`
- smoke test in `tests/test_bootstrap.c`

Build and test:

```sh
make test
```
