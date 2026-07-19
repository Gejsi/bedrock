# bedrock

A C11 replacement for the parts of the C standard library that are awkward,
underspecified, or missing. Ported and adapted from [Odin](https://odin-lang.org)'s
core packages. No dependencies beyond libc.

The rules everything follows: allocation is explicit (pass an allocator, never
a hidden global), errors are status codes (the library never aborts your
program), and zero-initialized objects are valid wherever C allows it.

## A taste

```c
#include <bedrock.h>

int main(void) {
  /* A growing virtual-memory arena. {0} is a valid empty arena — the first
     allocation sets it up. */
  br_virtual_arena arena = {0};
  br_alloc_result mem = br_virtual_arena_alloc(&arena, 1024, 8);
  if (mem.status != BR_STATUS_OK) {
    return 1;
  }

  /* Split a string without allocating anything. */
  br_string_view line = br_string_view_from_cstr("point,3,-7");
  br_string_split_iter it = br_string_split_iter_make(line, BR_STR_LIT(","));
  br_string_view field;
  while (br_string_split_iter_next(&it, &field)) {
    /* field.data, field.len */
  }

  /* Hex-encode into an owned buffer, free it through the same allocator. */
  br_bytes_result hex =
    br_hex_encode(br_bytes_view_from_cstr("bedrock"), BR_HEX_LOWER, br_allocator_heap());
  if (hex.status == BR_STATUS_OK) {
    br_bytes_free(hex.value, br_allocator_heap());
  }

  br_virtual_arena_destroy(&arena);
  return 0;
}
```

## Build

```sh
make test                      # debug build, runs every test
make MODE=release              # optimized static library
make MODE=sanitize test        # ASan + UBSan
make MODE=thread-sanitize test # TSan
```

Sources and tests are auto-discovered; adding a file needs no Makefile edit.
CI runs the full matrix on Linux, macOS, and Windows.

## Using it in your project

Vendor the repository (copy it or add a git submodule), build, include, link:

```sh
make MODE=release
cc -I<bedrock>/include my_app.c <bedrock>/build/release/lib/libbedrock.a -pthread
```

Static linking is the supported model: the linker drops what you don't
reference, and part of the API is inline code that compiles into your program
anyway. Prebuilt per-platform archives are planned once versioned releases
begin. A dynamic library may follow as an optional, version-locked artifact —
bedrock does not promise a stable ABI across versions.

### Modules

| Module | What it gives you |
| --- | --- |
| `mem` | allocator interface, arenas (fixed, growing, virtual-memory), scratch/stack/buddy/tracking allocators, an adapter for foreign sizeless-free allocator callbacks, low-level helpers |
| `strings` / `bytes` | views vs owned data, search/split/trim/case/fields, builders (with number writing), readers, allocation-free iterators, C-string interop (`clone_to_cstr`) |
| `unicode` | strict UTF-8 encode/decode/validate, rune counting; lossless WTF-8 ↔ WTF-16 transcode for OS strings |
| `io` / `bufio` | generic stream interface, buffered readers and writers |
| `sync` | futex-backed mutexes, rw/recursive locks, condvars, semaphores, wait groups, barriers, once, parker — all zero-value ready |
| `thread` | create/join/detach/yield over OS threads; double-join, self-join, and detach misuse return status codes instead of undefined behavior |
| `time` | monotonic ticks, sleep, durations; civil dates and RFC 3339 timestamps with an overflow-safe epoch bridge |
| `strconv` | locale-free number parsing and formatting (`br_parse_i64`, `br_format_f64`, ...): strict by default, exact float round-trip |
| `rand` | seedable PCG64 generator (zero-value ready), unbiased bounded draws, shuffle, OS entropy (`br_rand_entropy_fill`) |
| `encoding` | hex, base64 (std/url, padded/strict), LEB128 varints, byte-order (endian) conversion |
| `path` | lexical slash-path handling (clean, join, split, glob match) |
| `math` | bit operations: clz/ctz/popcount, rotates, carries, wide multiply, checked divide |

`#include <bedrock.h>` pulls in everything; `#include <bedrock/mem.h>` pulls
one module. `bedrock/template/vec.h` is a typed-vector template you include
per element type.

### Smaller builds

Link the whole archive (the default; unreferenced objects cost nothing), or
compile only the `src/` subdirectories you need plus their dependencies:

| Module | Directly depends on |
| --- | --- |
| `types`, `base`, `time` | (none) |
| `mem` (arenas + core allocators) | (none beyond `base`) |
| `mem` (`mutex_allocator`, `tracking_allocator`) | `sync` |
| `sync` | `time` |
| `thread` | `sync` |
| `bytes` + `unicode` (compile together) | `mem`, `io` |
| `io` | `unicode` |
| `strings` | `unicode`, `io`, `mem` |
| `bufio` | `bytes`, `io`, `mem`, `strings` |
| `strconv` | `strings`, `bytes` |
| `rand` | `math` |
| `encoding` | `bytes`, `io` |
| `path` | `strings`, `unicode` |
| `math` | (none beyond `base`) |

"I just want an arena" costs `mem` + `base` and nothing else.

### Types

Public headers are spelled in standard C types (`size_t`, `uint32_t`, ...).
The short aliases (`u32`, `usize`, `f64`, ...) from `bedrock/types.h` are on
by default and used throughout the implementation; define
`BEDROCK_NO_SHORT_TYPES` before including bedrock if you don't want them.

## Development

Design notes live in `spec/`, port status in `tracking/`, stable decisions in
`decisions/`. Odin is pinned as the reference in `upstream/odin`, with Rust
and Go pinned as secondary references. `tracking/odin-suspected-bugs.md`
collects upstream bugs found while porting.
