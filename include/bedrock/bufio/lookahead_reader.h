#ifndef BEDROCK_BUFIO_LOOKAHEAD_READER_H
#define BEDROCK_BUFIO_LOOKAHEAD_READER_H

#include <bedrock/bufio/common.h>
#include <bedrock/bytes/bytes.h>
#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_bufio_lookahead_reader {
  u8 *buf;
  usize cap;
  usize n;
  br_stream source;
  br_allocator allocator;
  bool owns_storage;
} br_bufio_lookahead_reader;

typedef struct br_bufio_lookahead_reader_peek_result {
  br_bytes_view value;
  br_status status;
} br_bufio_lookahead_reader_peek_result;

/*
Initialize a heap-backed lookahead reader using the default buffer size.

Unlike `br_bufio_reader`, the lookahead reader keeps the exact requested
capacity instead of clamping it to a minimum.
*/
br_status br_bufio_lookahead_reader_init(br_bufio_lookahead_reader *reader,
                                         br_stream source,
                                         br_allocator allocator);

/*
Initialize a heap-backed lookahead reader with an exact buffer size.
*/
br_status br_bufio_lookahead_reader_init_with_size(br_bufio_lookahead_reader *reader,
                                                   br_stream source,
                                                   usize size,
                                                   br_allocator allocator);

/*
Initialize a lookahead reader over caller-provided storage.
*/
br_status br_bufio_lookahead_reader_init_with_buffer(br_bufio_lookahead_reader *reader,
                                                     br_stream source,
                                                     void *buffer,
                                                     usize buffer_len);

void br_bufio_lookahead_reader_destroy(br_bufio_lookahead_reader *reader);
void br_bufio_lookahead_reader_reset(br_bufio_lookahead_reader *reader, br_stream source);

/*
Return the currently populated lookahead bytes.
*/
br_bytes_view br_bufio_lookahead_reader_buffer(const br_bufio_lookahead_reader *reader);

/*
Return a view holding at most `n` bytes.

If the underlying stream ends before `n` bytes are available, the returned
status is `BR_STATUS_EOF`, matching Odin's lookahead reader semantics.
`BR_STATUS_BUFFER_FULL` means `n` exceeds the fixed lookahead capacity.
*/
br_bufio_lookahead_reader_peek_result
br_bufio_lookahead_reader_peek(br_bufio_lookahead_reader *reader, usize n);

/*
Populate and return the full lookahead buffer.
*/
br_bufio_lookahead_reader_peek_result
br_bufio_lookahead_reader_peek_all(br_bufio_lookahead_reader *reader);

/*
Drop the first `n` populated bytes.
*/
br_status br_bufio_lookahead_reader_consume(br_bufio_lookahead_reader *reader, usize n);

/*
Drop all populated bytes.
*/
br_status br_bufio_lookahead_reader_consume_all(br_bufio_lookahead_reader *reader);

BR_EXTERN_C_END

#endif
