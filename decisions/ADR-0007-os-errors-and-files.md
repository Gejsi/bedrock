# ADR-0007: OS Errors and the First File Slice

## Status

Accepted (August 1, 2026).

## Decision

Bedrock OS operations report both:

- a portable `br_status` category for control flow
- the original platform error domain and numeric code when a native operation
  failed

```c
typedef enum br_error_domain {
  BR_ERROR_DOMAIN_NONE = 0,
  BR_ERROR_DOMAIN_POSIX_ERRNO,
  BR_ERROR_DOMAIN_WIN32
} br_error_domain;

typedef struct br_native_error {
  br_error_domain domain;
  uint32_t code;
} br_native_error;

typedef struct br_error {
  br_status status;
  br_native_error native;
} br_error;
```

Native errors are captured immediately after the failing operation. They are
carried through `br_stream` results and buffered adapters; no thread-local
"last error" or mutable file-side error slot is used.

The first `os` implementation is one complete file-handle slice:

- caller-owned `br_file`, inert when zero-initialized
- open, close, read, write, positioned read/write, seek, and size
- adaptation to the existing `br_stream` interface
- read, write, create, truncate, append, and atomic create-new options
- close-on-exec on POSIX and non-inheritable handles on Windows

The stream borrows its `br_file`. Closing or destroying the stream closes the
file but never frees caller storage.

## Error Categories

`br_status` remains the shared portable category enum. File-relevant categories
are appended without renumbering existing values. Unrecognized native failures
map to `BR_STATUS_IO_ERROR` while retaining their original native code.

`br_status_string` is allocation-free and returns a stable string for every
known category plus `"unknown status"` for values outside the enum.

## Paths

Path arguments are length-delimited byte views:

- POSIX treats them as opaque bytes and rejects only malformed views, interior
  NUL, and unrepresentable lengths.
- Windows requires well-formed WTF-8 and converts it losslessly to UTF-16.

This matches Zig, Go, and Rust and gives the existing WTF-8 module its intended
OS-path consumer. Odin's strict-UTF-8 Windows conversion is not copied because
it cannot represent all valid Windows names.

Temporary NUL-terminated path storage is internal to the call and uses the
process heap. It never escapes or creates persistent ownership. Requiring a
caller allocator solely for this native calling convention would add lifetime
surface without giving the caller meaningful ownership.

## File Ownership

`br_file` is caller-owned and must not be copied while open. Open requires an
inert initialized handle and leaves it inert on failure. Close invalidates the
handle even when the native close reports an error, preventing accidental reuse
of a descriptor or handle whose ownership is ambiguous.

The stream implements native positioned operations instead of the generic
seek/read/restore fallback. Append handles reject positioned writes because
they cannot honor both append-only and explicit-offset semantics.

Raw file streams do not advertise `FLUSH`: moving bytes from user-space buffers
is a buffered-writer concern, while durability is a separate future `sync`
operation.

## Reference Findings

- Odin provides a useful typed flag inventory and file-to-stream adapter, but
  its file wrappers allocate secretly, its Windows paths are strict UTF-8, and
  its stream adapter collapses OS errors.
- Zig uses opaque POSIX paths and WTF-8 Windows paths, but its portable layer
  discards native error codes.
- Go keeps native errors under wrappers and exposes a small portable set.
- Rust explicitly retains a raw OS code and derives a portable error kind.

Bedrock follows Rust's category-plus-native-code property, Zig/Rust/Go path
semantics, and its own caller-owned handle conventions.

## Deferred Surface

Path metadata, directory handles and iteration, remove/rename, environment,
arguments, standard streams, file cloning, durability sync, and process
spawning are not part of this slice. They require their own complete contracts;
their absence does not weaken the file-stream behavior implemented here.
