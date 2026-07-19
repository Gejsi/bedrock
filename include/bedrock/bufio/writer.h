#ifndef BEDROCK_BUFIO_WRITER_H
#define BEDROCK_BUFIO_WRITER_H

#include <bedrock/bufio/common.h>
#include <bedrock/strings/strings.h>

BR_EXTERN_C_BEGIN

typedef struct br_bufio_writer {
  uint8_t *buf;
  size_t cap;
  size_t n;
  br_stream sink;
  br_allocator allocator;
  br_status err;
  bool owns_storage;
  size_t max_consecutive_empty_writes;
} br_bufio_writer;

typedef br_io_result br_bufio_writer_io_result;

/*
Initialize a heap-backed buffered writer using the default buffer size.
*/
br_status br_bufio_writer_init(br_bufio_writer *writer, br_stream sink, br_allocator allocator);

/*
Initialize a heap-backed buffered writer with an explicit buffer size.
*/
br_status br_bufio_writer_init_with_size(br_bufio_writer *writer,
                                         br_stream sink,
                                         size_t size,
                                         br_allocator allocator);

/*
Initialize a buffered writer over caller-provided storage.
*/
br_status br_bufio_writer_init_with_buffer(br_bufio_writer *writer,
                                           br_stream sink,
                                           void *buffer,
                                           size_t buffer_len);
void br_bufio_writer_destroy(br_bufio_writer *writer);
void br_bufio_writer_reset(br_bufio_writer *writer, br_stream sink);

size_t br_bufio_writer_size(const br_bufio_writer *writer);
size_t br_bufio_writer_available(const br_bufio_writer *writer);
size_t br_bufio_writer_buffered(const br_bufio_writer *writer);

/*
Flush buffered bytes into the underlying sink.
*/
br_status br_bufio_writer_flush(br_bufio_writer *writer);

/*
Write `src` into the buffered writer.

This may bypass the internal buffer when the buffer is empty and `src` is larger
than the remaining buffer space.
*/
br_bufio_writer_io_result
br_bufio_writer_write(br_bufio_writer *writer, const void *src, size_t src_len);
br_status br_bufio_writer_write_byte(br_bufio_writer *writer, uint8_t value);
br_bufio_writer_io_result br_bufio_writer_write_rune(br_bufio_writer *writer, br_rune value);
br_bufio_writer_io_result br_bufio_writer_write_string(br_bufio_writer *writer, br_string_view s);

/*
Read from `source` into the buffered writer until EOF or error.

Bedrock currently always stages through the buffer because generic streams do
not expose a `READ_FROM` specialization mode yet.
*/
br_i64_result br_bufio_writer_read_from(br_bufio_writer *writer, br_stream source);

/*
Expose this buffered writer through the generic stream interface.
*/
br_stream br_bufio_writer_as_stream(br_bufio_writer *writer);

static inline br_writer br_bufio_writer_as_writer(br_bufio_writer *writer) {
  return br_bufio_writer_as_stream(writer);
}

static inline br_flusher br_bufio_writer_as_flusher(br_bufio_writer *writer) {
  return br_bufio_writer_as_stream(writer);
}

BR_EXTERN_C_END

#endif
