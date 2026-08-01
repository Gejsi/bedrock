# OS

## Scope

The landed OS foundation is native file handles adapted to `br_stream`.
Directory operations, metadata, environment, arguments, standard streams, and
processes remain separate work.

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
inert even if the native close operation reports an error.

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

`create_permissions` is used only when POSIX creates a file and is filtered by
the process umask. The default is `0666`. Windows ignores this field because
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
`BR_STATUS_INVALID_ENCODING`.

## Stream Behavior

The borrowed file stream supports:

- `READ` when opened for reading
- `WRITE` when opened for writing
- `READ_AT` and `WRITE_AT` using native positioned operations
- `SEEK`
- `SIZE`
- `CLOSE`
- `DESTROY`
- `QUERY`

`WRITE_AT` on an append file returns `BR_STATUS_INVALID_STATE`; it must not
return `BR_STATUS_NOT_SUPPORTED`, because that would activate the generic
seek/write fallback.

A zero-length read or write returns success without a native call. A nonempty
read that reaches end of file with no bytes returns `BR_STATUS_EOF`. Partial
reads and writes retain their count and any accompanying native error.

POSIX operations retry `EINTR` except `close`, whose ownership is ambiguous
after interruption and is never retried. Each native call is capped at the
largest count its platform can represent.

The raw file stream does not support `FLUSH`. Buffered writers flush bytes to
it; durable storage synchronization will be a separate explicit operation.
