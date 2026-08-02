#ifndef BEDROCK_IO_IO_H
#define BEDROCK_IO_IO_H

#include <bedrock/base.h>
#include <bedrock/unicode/utf8.h>

BR_EXTERN_C_BEGIN

/*
Shared byte-count plus status result used by Bedrock IO operations.
*/
typedef struct br_io_result {
  size_t count;
  br_status status;
  br_native_error native_error;
} br_io_result;

/*
Shared 64-bit integer plus status result used by stream procedures.
*/
typedef struct br_i64_result {
  int64_t value;
  br_status status;
  br_native_error native_error;
} br_i64_result;

/*
Shared seek result used by Bedrock IO operations.
*/
typedef struct br_io_seek_result {
  int64_t offset;
  br_status status;
  br_native_error native_error;
} br_io_seek_result;

/*
Shared size result used by Bedrock IO operations.
*/
typedef struct br_io_size_result {
  int64_t size;
  br_status status;
  br_native_error native_error;
} br_io_size_result;

typedef struct br_io_byte_result {
  uint8_t value;
  br_status status;
  br_native_error native_error;
} br_io_byte_result;

typedef struct br_io_rune_result {
  br_rune value;
  size_t width;
  br_status status;
  br_native_error native_error;
} br_io_rune_result;

/*
Seek origin shared by byte and string readers and generic streams.
*/
typedef enum br_seek_from {
  BR_SEEK_FROM_START = 0,
  BR_SEEK_FROM_CURRENT,
  BR_SEEK_FROM_END
} br_seek_from;

/*
Stream operations use a single stream-proc shape. Unsupported modes return
`BR_STATUS_NOT_SUPPORTED`.
*/
typedef enum br_io_mode {
  BR_IO_MODE_CLOSE = 0,
  BR_IO_MODE_FLUSH,
  BR_IO_MODE_READ,
  BR_IO_MODE_READ_AT,
  BR_IO_MODE_WRITE,
  BR_IO_MODE_WRITE_AT,
  BR_IO_MODE_SEEK,
  BR_IO_MODE_SIZE,
  BR_IO_MODE_DESTROY,
  BR_IO_MODE_QUERY,
  /*
  Transfer all remaining bytes directly to/from the peer in the
  `br_io_transfer_request` stored in `data`. `data_len` is
  `sizeof(br_io_transfer_request)`; `offset` and `whence` are unused. A handler
  must not retain the request after returning. `BR_STATUS_OK` means the input
  was fully drained. A handler may return `BR_STATUS_NOT_SUPPORTED` only before
  making progress.
  */
  BR_IO_MODE_WRITE_TO,
  BR_IO_MODE_READ_FROM,
  BR_IO_MODE_COUNT
} br_io_mode;

typedef uint64_t br_io_mode_set;
BR_STATIC_ASSERT(BR_IO_MODE_COUNT <= 64, "br_io_mode_set must fit all io mode bits");

typedef struct br_io_query_result {
  br_io_mode_set modes;
  br_status status;
  br_native_error native_error;
} br_io_query_result;

typedef br_i64_result (*br_stream_proc)(
  void *context, br_io_mode mode, void *data, size_t data_len, int64_t offset, br_seek_from whence);

typedef struct br_stream {
  br_stream_proc procedure;
  void *context;
} br_stream;

/*
Payload for `BR_IO_MODE_WRITE_TO` and `BR_IO_MODE_READ_FROM`.
*/
typedef struct br_io_transfer_request {
  br_stream peer;
} br_io_transfer_request;

typedef br_stream br_reader;
typedef br_stream br_writer;
typedef br_stream br_closer;
typedef br_stream br_flusher;
typedef br_stream br_seeker;
typedef br_stream br_reader_at;
typedef br_stream br_writer_at;

static inline br_io_result br_io_result_make(size_t count, br_status status) {
  br_io_result result;

  result.count = count;
  result.status = status;
  result.native_error = BR_NATIVE_ERROR_NONE;
  return result;
}

static inline br_io_result br_io_result_make_error(size_t count, br_error error) {
  br_io_result result;

  result.count = count;
  result.status = error.status;
  result.native_error = error.native;
  return result;
}

static inline br_i64_result br_i64_result_make(int64_t value, br_status status) {
  br_i64_result result;

  result.value = value;
  result.status = status;
  result.native_error = BR_NATIVE_ERROR_NONE;
  return result;
}

static inline br_i64_result br_i64_result_make_error(int64_t value, br_error error) {
  br_i64_result result;

  result.value = value;
  result.status = error.status;
  result.native_error = error.native;
  return result;
}

static inline br_io_seek_result br_io_seek_result_make(int64_t offset, br_status status) {
  br_io_seek_result result;

  result.offset = offset;
  result.status = status;
  result.native_error = BR_NATIVE_ERROR_NONE;
  return result;
}

static inline br_io_seek_result br_io_seek_result_make_error(int64_t offset, br_error error) {
  br_io_seek_result result;

  result.offset = offset;
  result.status = error.status;
  result.native_error = error.native;
  return result;
}

static inline br_io_size_result br_io_size_result_make(int64_t size, br_status status) {
  br_io_size_result result;

  result.size = size;
  result.status = status;
  result.native_error = BR_NATIVE_ERROR_NONE;
  return result;
}

static inline br_io_size_result br_io_size_result_make_error(int64_t size, br_error error) {
  br_io_size_result result;

  result.size = size;
  result.status = error.status;
  result.native_error = error.native;
  return result;
}

static inline br_io_byte_result br_io_byte_result_make(uint8_t value, br_status status) {
  br_io_byte_result result;

  result.value = value;
  result.status = status;
  result.native_error = BR_NATIVE_ERROR_NONE;
  return result;
}

static inline br_io_byte_result br_io_byte_result_make_error(uint8_t value, br_error error) {
  br_io_byte_result result;

  result.value = value;
  result.status = error.status;
  result.native_error = error.native;
  return result;
}

static inline br_io_rune_result
br_io_rune_result_make(br_rune value, size_t width, br_status status) {
  br_io_rune_result result;

  result.value = value;
  result.width = width;
  result.status = status;
  result.native_error = BR_NATIVE_ERROR_NONE;
  return result;
}

static inline br_io_rune_result
br_io_rune_result_make_error(br_rune value, size_t width, br_error error) {
  br_io_rune_result result;

  result.value = value;
  result.width = width;
  result.status = error.status;
  result.native_error = error.native;
  return result;
}

static inline br_io_query_result br_io_query_result_make(br_io_mode_set modes, br_status status) {
  br_io_query_result result;

  result.modes = modes;
  result.status = status;
  result.native_error = BR_NATIVE_ERROR_NONE;
  return result;
}

static inline br_io_query_result br_io_query_result_make_error(br_io_mode_set modes,
                                                               br_error error) {
  br_io_query_result result;

  result.modes = modes;
  result.status = error.status;
  result.native_error = error.native;
  return result;
}

static inline br_error br_io_error_make(br_status status, br_native_error native_error) {
  br_error error;

  error.status = status;
  error.native = status == BR_STATUS_OK ? BR_NATIVE_ERROR_NONE : native_error;
  return error;
}

static inline br_io_mode_set br_io_mode_bit(br_io_mode mode) {
  return ((br_io_mode_set)1u) << (unsigned)mode;
}

static inline br_i64_result br_stream_query_utility(br_io_mode_set modes) {
  return br_i64_result_make((int64_t)modes, BR_STATUS_OK);
}

static inline br_stream br_stream_make(void *context, br_stream_proc procedure) {
  br_stream stream;

  stream.procedure = procedure;
  stream.context = context;
  return stream;
}

static inline bool br_stream_is_valid(br_stream stream) {
  return stream.procedure != NULL;
}

static inline br_reader br_reader_make(void *context, br_stream_proc procedure) {
  return br_stream_make(context, procedure);
}

static inline br_writer br_writer_make(void *context, br_stream_proc procedure) {
  return br_stream_make(context, procedure);
}

static inline br_seeker br_seeker_make(void *context, br_stream_proc procedure) {
  return br_stream_make(context, procedure);
}

static inline bool br_reader_is_valid(br_reader reader) {
  return br_stream_is_valid(reader);
}

static inline bool br_writer_is_valid(br_writer writer) {
  return br_stream_is_valid(writer);
}

static inline bool br_seeker_is_valid(br_seeker seeker) {
  return br_stream_is_valid(seeker);
}

/*
Read bytes using a generic stream.
*/
br_io_result br_read(br_stream stream, void *dst, size_t dst_len);

/*
Read until at least `min_len` bytes have been copied into `dst`.

If `dst_len` is smaller than `min_len`, `BR_STATUS_SHORT_BUFFER` is returned.
If EOF happens after some bytes but before `min_len` bytes are read,
`BR_STATUS_UNEXPECTED_EOF` is returned.
*/
br_io_result br_read_at_least(br_stream stream, void *dst, size_t dst_len, size_t min_len);

/*
Read exactly `dst_len` bytes into `dst`.
*/
br_io_result br_read_full(br_stream stream, void *dst, size_t dst_len);

/*
Write bytes using a generic stream.

A successful call may accept fewer than `src_len` bytes. Use `br_write_full`
when the whole input must be accepted before returning. A failing call may
still report `count > 0`; those bytes were accepted before the reported error.
*/
br_io_result br_write(br_stream stream, const void *src, size_t src_len);

/*
Write until at least `min_len` bytes from `src` have been accepted.

If `src_len` is smaller than `min_len`, `BR_STATUS_SHORT_BUFFER` is returned.
*/
br_io_result br_write_at_least(br_stream stream, const void *src, size_t src_len, size_t min_len);

/*
Write exactly `src_len` bytes from `src`.
*/
br_io_result br_write_full(br_stream stream, const void *src, size_t src_len);

/*
Read from an explicit offset. If the stream does not implement `READ_AT`,
Bedrock falls back to `SEEK + READ + SEEK`.
*/
br_io_result br_read_at(br_stream stream, void *dst, size_t dst_len, int64_t offset);

/*
Write to an explicit offset. If the stream does not implement `WRITE_AT`,
Bedrock falls back to `SEEK + WRITE + SEEK`.
*/
br_io_result br_write_at(br_stream stream, const void *src, size_t src_len, int64_t offset);

/*
Seek using a generic stream.
*/
br_io_seek_result br_seek(br_stream stream, int64_t offset, br_seek_from whence);

/*
Close or flush a generic stream.

`br_destroy` attempts to flush and close the stream before dispatching its
destroy operation, even when an earlier operation fails. Unsupported flush or
close modes are ignored. It returns the first other flush/close failure, or the
destroy operation's error when neither failed.
*/
br_error br_close(br_stream stream);
br_error br_flush(br_stream stream);
br_error br_destroy(br_stream stream);

/*
Return the size of a generic stream. If `SIZE` is unsupported and `SEEK`
exists, Bedrock falls back to querying the current offset and end offset.
*/
br_io_size_result br_size(br_stream stream);

/*
Query supported stream modes.
*/
br_io_query_result br_query(br_stream stream);

/*
Read and return one byte from a generic stream.
*/
br_io_byte_result br_read_byte(br_stream stream);

/*
Write one byte to a generic stream.
*/
br_error br_write_byte(br_stream stream, uint8_t value);

/*
Read and decode one UTF-8 rune from a generic stream.

Successful short reads are accumulated until the expected UTF-8 width is
available. A zero-byte successful read reports `BR_STATUS_NO_PROGRESS`.
The reported width is the number of bytes consumed from the stream. For
malformed multi-byte prefixes this may be larger than the decoder's logical
replacement-rune width, because generic streams cannot necessarily unread or
peek ahead.
*/
br_io_rune_result br_read_rune(br_stream stream);

/*
Write one rune to a generic stream encoded as UTF-8.
*/
br_io_result br_write_rune(br_stream stream, br_rune value);

/*
Copy all remaining bytes from `src` to `dst`.

This first tries the source's `WRITE_TO` operation, then the destination's
`READ_FROM` operation, and otherwise uses an internal stack buffer. EOF from
the source is treated as a successful end-of-copy. `src` and `dst` must not be
the same stream or use storage that aliases unsafely.
*/
br_i64_result br_copy(br_stream dst, br_stream src);

/*
Copy all remaining bytes from `src` to `dst` using caller-provided scratch
storage. `buffer` must be non-NULL and `buffer_len` must be greater than zero.
This function always uses `buffer`; it does not try direct-transfer modes. EOF
from the source is treated as a successful end-of-copy. `src` and `dst` must
not be the same stream or use storage that aliases unsafely.
*/
br_i64_result br_copy_buffer(br_stream dst, br_stream src, void *buffer, size_t buffer_len);

/*
Compatibility wrappers around the old split-trait helper names.
*/
static inline br_io_result br_reader_read(br_reader reader, void *dst, size_t dst_len) {
  return br_read(reader, dst, dst_len);
}

static inline br_io_result br_writer_write(br_writer writer, const void *src, size_t src_len) {
  return br_write(writer, src, src_len);
}

static inline br_io_seek_result
br_seeker_seek(br_seeker seeker, int64_t offset, br_seek_from whence) {
  return br_seek(seeker, offset, whence);
}

BR_EXTERN_C_END

#endif
