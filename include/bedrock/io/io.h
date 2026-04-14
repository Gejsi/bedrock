#ifndef BEDROCK_IO_IO_H
#define BEDROCK_IO_IO_H

#include <bedrock/base.h>
#include <bedrock/unicode/utf8.h>

BR_EXTERN_C_BEGIN

/*
Shared byte-count plus status result used by Bedrock IO operations.
*/
typedef struct br_io_result {
  usize count;
  br_status status;
} br_io_result;

/*
Shared 64-bit integer plus status result used by stream procedures.
*/
typedef struct br_i64_result {
  i64 value;
  br_status status;
} br_i64_result;

/*
Shared seek result used by Bedrock IO operations.
*/
typedef struct br_io_seek_result {
  i64 offset;
  br_status status;
} br_io_seek_result;

/*
Shared size result used by Bedrock IO operations.
*/
typedef struct br_io_size_result {
  i64 size;
  br_status status;
} br_io_size_result;

typedef struct br_io_byte_result {
  u8 value;
  br_status status;
} br_io_byte_result;

typedef struct br_io_rune_result {
  br_rune value;
  usize width;
  br_status status;
} br_io_rune_result;

/*
Seek origin shared by byte and string readers and generic streams.
*/
typedef enum br_seek_from {
  BR_SEEK_FROM_START = 0,
  BR_SEEK_FROM_CURRENT = 1,
  BR_SEEK_FROM_END = 2
} br_seek_from;

/*
Stream operations follow Odin's single stream proc shape. Unsupported modes
return `BR_STATUS_NOT_SUPPORTED`.
*/
typedef enum br_io_mode {
  BR_IO_MODE_CLOSE = 0,
  BR_IO_MODE_FLUSH = 1,
  BR_IO_MODE_READ = 2,
  BR_IO_MODE_READ_AT = 3,
  BR_IO_MODE_WRITE = 4,
  BR_IO_MODE_WRITE_AT = 5,
  BR_IO_MODE_SEEK = 6,
  BR_IO_MODE_SIZE = 7,
  BR_IO_MODE_DESTROY = 8,
  BR_IO_MODE_QUERY = 9,
  BR_IO_MODE_COUNT = 10
} br_io_mode;

typedef u64 br_io_mode_set;
BR_STATIC_ASSERT(BR_IO_MODE_COUNT <= 64, "br_io_mode_set must fit all io mode bits");

typedef struct br_io_query_result {
  br_io_mode_set modes;
  br_status status;
} br_io_query_result;

typedef br_i64_result (*br_stream_proc)(
  void *context, br_io_mode mode, void *data, usize data_len, i64 offset, br_seek_from whence);

typedef struct br_stream {
  br_stream_proc procedure;
  void *context;
} br_stream;

typedef br_stream br_reader;
typedef br_stream br_writer;
typedef br_stream br_closer;
typedef br_stream br_flusher;
typedef br_stream br_seeker;
typedef br_stream br_reader_at;
typedef br_stream br_writer_at;

static inline br_io_result br_io_result_make(usize count, br_status status) {
  br_io_result result;

  result.count = count;
  result.status = status;
  return result;
}

static inline br_i64_result br_i64_result_make(i64 value, br_status status) {
  br_i64_result result;

  result.value = value;
  result.status = status;
  return result;
}

static inline br_io_seek_result br_io_seek_result_make(i64 offset, br_status status) {
  br_io_seek_result result;

  result.offset = offset;
  result.status = status;
  return result;
}

static inline br_io_size_result br_io_size_result_make(i64 size, br_status status) {
  br_io_size_result result;

  result.size = size;
  result.status = status;
  return result;
}

static inline br_io_byte_result br_io_byte_result_make(u8 value, br_status status) {
  br_io_byte_result result;

  result.value = value;
  result.status = status;
  return result;
}

static inline br_io_rune_result
br_io_rune_result_make(br_rune value, usize width, br_status status) {
  br_io_rune_result result;

  result.value = value;
  result.width = width;
  result.status = status;
  return result;
}

static inline br_io_query_result br_io_query_result_make(br_io_mode_set modes, br_status status) {
  br_io_query_result result;

  result.modes = modes;
  result.status = status;
  return result;
}

static inline br_io_mode_set br_io_mode_bit(br_io_mode mode) {
  return ((br_io_mode_set)1u) << (unsigned)mode;
}

static inline br_i64_result br_stream_query_utility(br_io_mode_set modes) {
  return br_i64_result_make((i64)modes, BR_STATUS_OK);
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
br_io_result br_read(br_stream stream, void *dst, usize dst_len);

/*
Read until at least `min_len` bytes have been copied into `dst`.

If `dst_len` is smaller than `min_len`, `BR_STATUS_SHORT_BUFFER` is returned.
If EOF happens after some bytes but before `min_len` bytes are read,
`BR_STATUS_UNEXPECTED_EOF` is returned.
*/
br_io_result br_read_at_least(br_stream stream, void *dst, usize dst_len, usize min_len);

/*
Read exactly `dst_len` bytes into `dst`.
*/
br_io_result br_read_full(br_stream stream, void *dst, usize dst_len);

/*
Write bytes using a generic stream.
*/
br_io_result br_write(br_stream stream, const void *src, usize src_len);

/*
Write until at least `min_len` bytes from `src` have been accepted.

If `src_len` is smaller than `min_len`, `BR_STATUS_SHORT_BUFFER` is returned.
*/
br_io_result br_write_at_least(br_stream stream, const void *src, usize src_len, usize min_len);

/*
Write exactly `src_len` bytes from `src`.
*/
br_io_result br_write_full(br_stream stream, const void *src, usize src_len);

/*
Read from an explicit offset. If the stream does not implement `READ_AT`,
Bedrock falls back to `SEEK + READ + SEEK`.
*/
br_io_result br_read_at(br_stream stream, void *dst, usize dst_len, i64 offset);

/*
Write to an explicit offset. If the stream does not implement `WRITE_AT`,
Bedrock falls back to `SEEK + WRITE + SEEK`.
*/
br_io_result br_write_at(br_stream stream, const void *src, usize src_len, i64 offset);

/*
Seek using a generic stream.
*/
br_io_seek_result br_seek(br_stream stream, i64 offset, br_seek_from whence);

/*
Close, flush, or destroy a generic stream.
*/
br_status br_close(br_stream stream);
br_status br_flush(br_stream stream);
br_status br_destroy(br_stream stream);

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
br_status br_write_byte(br_stream stream, u8 value);

/*
Read and decode one UTF-8 rune from a generic stream.

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
Copy all remaining bytes from `src` to `dst` through an internal stack buffer.
EOF from the source is treated as a successful end-of-copy.
*/
br_i64_result br_copy(br_stream dst, br_stream src);

/*
Copy all remaining bytes from `src` to `dst` using caller-provided scratch
storage. `buffer` must be non-NULL and `buffer_len` must be greater than zero.
EOF from the source is treated as a successful end-of-copy.
*/
br_i64_result br_copy_buffer(br_stream dst, br_stream src, void *buffer, usize buffer_len);

/*
Compatibility wrappers around the old split-trait helper names.
*/
static inline br_io_result br_reader_read(br_reader reader, void *dst, usize dst_len) {
  return br_read(reader, dst, dst_len);
}

static inline br_io_result br_writer_write(br_writer writer, const void *src, usize src_len) {
  return br_write(writer, src, src_len);
}

static inline br_io_seek_result br_seeker_seek(br_seeker seeker, i64 offset, br_seek_from whence) {
  return br_seek(seeker, offset, whence);
}

BR_EXTERN_C_END

#endif
