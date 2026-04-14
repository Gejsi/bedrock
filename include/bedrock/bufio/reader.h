#ifndef BEDROCK_BUFIO_READER_H
#define BEDROCK_BUFIO_READER_H

#include <bedrock/bufio/common.h>
#include <bedrock/bytes/buffer.h>
#include <bedrock/strings/strings.h>

BR_EXTERN_C_BEGIN

typedef struct br_bufio_reader {
  u8 *buf;
  usize cap;
  usize r;
  usize w;
  br_stream source;
  br_allocator allocator;
  br_status err;
  bool owns_storage;
  isize last_byte;
  isize last_rune_size;
  usize max_consecutive_empty_reads;
} br_bufio_reader;

typedef br_io_result br_bufio_reader_io_result;
typedef br_io_byte_result br_bufio_reader_byte_result;
typedef br_io_rune_result br_bufio_reader_rune_result;

typedef struct br_bufio_reader_peek_result {
  br_bytes_view value;
  br_status status;
} br_bufio_reader_peek_result;

typedef br_bufio_reader_peek_result br_bufio_reader_slice_result;

/*
Initialize a heap-backed buffered reader using the default buffer size.
*/
br_status br_bufio_reader_init(br_bufio_reader *reader, br_stream source, br_allocator allocator);

/*
Initialize a heap-backed buffered reader with an explicit buffer size.
*/
br_status br_bufio_reader_init_with_size(br_bufio_reader *reader,
                                         br_stream source,
                                         usize size,
                                         br_allocator allocator);

/*
Initialize a buffered reader over caller-provided storage.
*/
br_status br_bufio_reader_init_with_buffer(br_bufio_reader *reader,
                                           br_stream source,
                                           void *buffer,
                                           usize buffer_len);
void br_bufio_reader_destroy(br_bufio_reader *reader);
void br_bufio_reader_reset(br_bufio_reader *reader, br_stream source);

usize br_bufio_reader_size(const br_bufio_reader *reader);
usize br_bufio_reader_buffered(const br_bufio_reader *reader);

/*
Return the next `n` bytes without advancing the reader.

Like Odin's `bufio.reader_peek`, the returned view becomes invalid after the
next buffered reader operation. If fewer than `n` bytes are returned, `status`
explains why the result is short.
*/
br_bufio_reader_peek_result br_bufio_reader_peek(br_bufio_reader *reader, usize n);

/*
Skip the next `n` bytes and report how many bytes were actually discarded.
*/
br_bufio_reader_io_result br_bufio_reader_discard(br_bufio_reader *reader, usize n);

/*
Read into `dst`.

This follows Odin's `bufio.reader_read`: bytes come from at most one read of
the underlying stream, so a successful short read is allowed.
*/
br_bufio_reader_io_result br_bufio_reader_read(br_bufio_reader *reader, void *dst, usize dst_len);
br_bufio_reader_byte_result br_bufio_reader_read_byte(br_bufio_reader *reader);
br_status br_bufio_reader_unread_byte(br_bufio_reader *reader);

/*
Read one UTF-8 rune from the buffered reader.

Invalid encodings consume one byte and return `BR_RUNE_ERROR` with width 1,
matching Odin's `bufio.reader_read_rune`.
*/
br_bufio_reader_rune_result br_bufio_reader_read_rune(br_bufio_reader *reader);
br_status br_bufio_reader_unread_rune(br_bufio_reader *reader);

/*
Read until `delim` is found and return a view into the internal buffer.

Like Odin's `bufio.reader_read_slice`, the returned view is invalidated by the
next buffered reader operation. `BR_STATUS_BUFFER_FULL` means the current
buffer filled before the delimiter was found.
*/
br_bufio_reader_slice_result br_bufio_reader_read_slice(br_bufio_reader *reader, u8 delim);

/*
Read until `delim` is found and return an owned byte slice.
*/
br_bytes_result
br_bufio_reader_read_bytes(br_bufio_reader *reader, u8 delim, br_allocator allocator);

/*
Read until `delim` is found and return an owned string.
*/
br_string_result
br_bufio_reader_read_string(br_bufio_reader *reader, u8 delim, br_allocator allocator);

/*
Expose this buffered reader through the generic stream interface.
*/
br_stream br_bufio_reader_as_stream(br_bufio_reader *reader);

static inline br_reader br_bufio_reader_as_reader(br_bufio_reader *reader) {
  return br_bufio_reader_as_stream(reader);
}

BR_EXTERN_C_END

#endif
