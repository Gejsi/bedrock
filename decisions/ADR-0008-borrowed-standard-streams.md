# ADR-0008: Borrowed Standard Byte Streams

## Status

Accepted (August 1, 2026).

## Decision

Bedrock exposes the process standard handles as borrowed, unbuffered streams:

```c
br_reader br_stdin(void);
br_writer br_stdout(void);
br_writer br_stderr(void);
```

The returned `br_stream` values are copyable and allocate no storage. They do
not own the underlying descriptor or handle: `CLOSE` is unsupported and
`DESTROY` is a no-op success. `FLUSH` is unsupported because this layer has no
user-space buffer; a `br_bufio_writer` supplies meaningful buffered flushing.

Each operation resolves the current process standard handle. POSIX operations
use descriptors 0, 1, and 2, so `dup2` redirection is observed. Windows calls
`GetStdHandle` for every operation, so `SetStdHandle` replacement is observed.
Native errors are retained through the shared OS error mapper.

Standard input advertises only read, destroy, and query. Standard output and
standard error advertise only write, destroy, and query. Seek, size,
positioned I/O, and ownership-changing operations are not exposed even when a
standard handle currently refers to a regular file.

## Byte Semantics

Standard streams are byte-transparent. POSIX uses `read` and `write`; Windows
uses `ReadFile` and `WriteFile`. Bedrock does not silently convert UTF-8 to or
from a Windows console's UTF-16 representation.

Console text conversion requires caller-owned state for partial UTF-8,
surrogate handling, and synchronization. It belongs in a future explicit
terminal-text adapter. Redirected files and pipes must remain byte-transparent.

## Process Policy

Bedrock does not install or alter a process-wide `SIGPIPE` handler. A POSIX
write to a closed pipe may therefore terminate a process whose signal policy
allows the default action. Applications that need status-return behavior must
set their own signal policy.

Replacing a process standard handle concurrently with active I/O remains a
process-level race. Bedrock observes completed redirection but does not
serialize external handle replacement.

## Reference Findings

- Odin and Go expose mutable global standard-file objects, which conflicts with
  Bedrock's no-mutable-globals rule.
- Rust's POSIX standard streams deliberately avoid owning descriptors 0, 1,
  and 2, and its Windows implementation resolves current handles dynamically.
- Zig exposes raw standard handles, but those values do not preserve borrowed
  ownership through a generic close-capable stream interface.

Bedrock keeps Rust's borrowed ownership and dynamic Windows lookup while using
its existing mode-query stream contract.
