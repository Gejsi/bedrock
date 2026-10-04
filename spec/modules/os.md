# OS

## Scope

The landed OS foundation is native file handles and borrowed process standard
streams adapted to `br_stream`. Directory operations, metadata, environment,
arguments, and processes remain separate work.

## Errors

`br_error` contains a portable `br_status` plus an optional native error:

```c
typedef struct br_error {
  br_status status;
  br_native_error native;
} br_error;
```

`BR_STATUS_OK` always carries `BR_ERROR_DOMAIN_NONE` and code zero. Errors
created by Bedrock validation also have no native code. A failed POSIX or
Windows operation records `errno` or `GetLastError()` immediately and maps it
to the closest portable category. Unmapped codes use `BR_STATUS_IO_ERROR`.

Stream count/value results retain their existing `status` field and add
`native_error`. Constructors taking only a status clear the native error;
error-aware constructors preserve it. Buffering stores both fields whenever an
underlying stream error becomes sticky.

## File Handle

```c
typedef struct br_file {
  /* Opaque implementation fields. Do not inspect or copy an open handle. */
  uintptr_t handle;
  uintptr_t positioned_handle;
  uint32_t flags;
} br_file;

#define BR_FILE_INIT ...

typedef struct br_file_open_options {
  br_file_open_flags flags;
  uint32_t create_permissions;
} br_file_open_options;
```

The handle is caller-owned. Initialize it with `BR_FILE_INIT`, open it once,
then close it before reuse. `br_file_as_stream` returns a borrowed adapter that
is valid only while the file object remains alive and open.

Opening into a live object and closing an inert object return
`BR_STATUS_INVALID_STATE`. Open failure leaves the object inert. Close makes it
inert even if a native close operation reports an error. POSIX owns one file
descriptor. Windows may own a second non-inheritable handle dedicated to
positioned I/O; close releases both.

## Open Options

Supported flags:

- `BR_FILE_OPEN_READ`
- `BR_FILE_OPEN_WRITE`
- `BR_FILE_OPEN_CREATE`
- `BR_FILE_OPEN_TRUNCATE`
- `BR_FILE_OPEN_APPEND`
- `BR_FILE_OPEN_CREATE_NEW`

At least one of read or write is required. Append and truncate require write.
Create-new means atomic create-and-fail-if-present and implies create.

`create_permissions` must fit in the portable POSIX mode-bit mask `07777`. It is
used only when POSIX creates a file and is filtered by the process umask. The
default is `0666`. Windows validates but otherwise ignores this field because
its access-control model is not representable as POSIX mode bits.

POSIX descriptors are close-on-exec. Windows handles are non-inheritable and
share read, write, and delete access so ordinary rename and replacement
behavior matches modern Windows library conventions.

## Paths

Paths use `br_string_view`.

On POSIX, every byte except NUL is accepted; no UTF-8 validation occurs. On
Windows, the bytes must be well-formed WTF-8 and are converted losslessly to a
NUL-terminated UTF-16 path. Malformed views and interior NUL return
`BR_STATUS_INVALID_ARGUMENT`; malformed WTF-8 returns
`BR_STATUS_INVALID_ENCODING`. Ordinary Windows paths that would reach the
legacy path limit are normalized and made absolute, then receive the `\\?\`
drive prefix or `\\?\UNC\` network prefix. Existing extended and device
namespace paths are preserved.

## Stream Behavior

The borrowed file stream supports:

- `READ` when opened for reading
- `WRITE` when opened for writing
- `READ_AT` and `WRITE_AT` using native positioned operations
- `WRITE_TO` when opened for reading, for another native file stream
- `READ_FROM` when opened for writing, for another native file stream
- `SEEK`
- `SIZE`
- `CLOSE`
- `DESTROY`
- `QUERY`

`WRITE_AT` on an append file returns `BR_STATUS_INVALID_STATE`; it must not
return `BR_STATUS_NOT_SUPPORTED`, because that would activate the generic
seek/write fallback.

A Windows file uses a separate `FILE_FLAG_OVERLAPPED` handle for positioned
operations. Each call has its own event-backed `OVERLAPPED` request, so
`READ_AT` and `WRITE_AT` do not alter the sequential stream cursor and
independent positioned calls can overlap safely.

A zero-length read or write returns success without a native call. A nonempty
read that reaches end of file with no bytes returns `BR_STATUS_EOF`. Partial
reads and writes retain their count and any accompanying native error.

POSIX operations retry `EINTR` except `close`, whose ownership is ambiguous
after interruption and is never retried. Each native call is capped at the
largest count its platform can represent.

The raw file stream does not support `FLUSH`. Buffered writers flush bytes to
it; durable storage synchronization will be a separate explicit operation.

## File Transfers

File transfer modes use the existing `br_io_transfer_request { br_stream peer; }`.
The request has its exact typed size; `offset` and `whence` are unused. A file
recognizes another file stream by its procedure before interpreting the peer
context. Other stream types return `NOT_SUPPORTED` with zero progress and no I/O,
allowing their existing transfer implementation or ordinary copying to run.
The public file layout and stream protocol do not change.

Both files must be live and direction-compatible. A live file lacking the access
required for the requested transfer mode returns `NOT_SUPPORTED`. Within a
supported mode, an invalid request or incompatible native peer access returns
`INVALID_ARGUMENT`; an inert native peer returns `INVALID_STATE`.
Direct self-copy and independent handles identifying the same native file,
including hard links, are rejected before data or cursor movement.

A transfer copies the source's remaining bytes to the destination's current
sequential cursor. It does not truncate an existing destination, copy metadata,
reopen paths, take ownership, or close either file. Positioned operations remain
independent. Append destinations use the existing native append writes, even
when their sequential cursor was explicitly moved earlier.

Eligible Linux regular-file pairs use bounded `copy_file_range` operations.
Each successful native step advances both cursors by its accepted byte count.
Unsupported kernel/filesystem combinations continue with ordinary native
read/write calls at those cursors. A zero native return is confirmed by ordinary
reading, avoiding false EOF from filesystems that cannot provide the native
copy. macOS and Windows use their existing native read/write continuation.
Their data-copy and storage-offload APIs require additional handling to
preserve this contract, so acceleration on those platforms remains separate.

Native progress and subsequent buffered progress share one `INT64_MAX` count
limit. Interrupted POSIX operations retry; legal short writes are completed;
zero successful progress terminates. EOF completes successfully. Errors retain
the cumulative accepted-output count and exact native domain/code. As with
ordinary buffered copying, a failed write can leave the source ahead of the
accepted destination bytes.

Safe continuation is an internal explicit outcome, so it never escapes as a
public unsupported result after progress. For a recognized file pair, an
operational `NOT_SUPPORTED`, including an identity-query failure before data
I/O, is reported as `IO_ERROR` with its native detail. It cannot trigger an
outer transfer probe that bypasses validation or reconsumes input.
`br_copy_buffer` continues to use its caller-provided storage without probing
the file transfer modes.

## Standard Streams

```c
br_reader br_stdin(void);
br_writer br_stdout(void);
br_writer br_stderr(void);
```

These accessors return borrowed, unbuffered byte streams. The values are
copyable, allocate no storage, and never own the process descriptor or handle.
`CLOSE` and `FLUSH` are unsupported; `DESTROY` succeeds without changing the
underlying process handle.

Each operation resolves the current process standard handle. POSIX uses
descriptors 0, 1, and 2, while Windows calls `GetStdHandle` for every read or
write. Redirection completed through `dup2` or `SetStdHandle` after obtaining a
Bedrock stream is therefore observed.

Standard input supports read; standard output and standard error support
write. They do not expose seek, size, or positioned operations when redirected
to a file because their contract remains that of a process standard stream.

The streams are byte-transparent and do not perform Windows console encoding
conversion. A future terminal-text adapter may add explicit UTF-8/UTF-16
conversion with caller-owned state. Bedrock also leaves POSIX `SIGPIPE` policy
to the application.
