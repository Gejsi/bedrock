#ifndef BEDROCK_BYTES_BUFFER_H
#define BEDROCK_BYTES_BUFFER_H

#include <bedrock/bytes/bytes.h>
#include <bedrock/io/io.h>

BR_EXTERN_C_BEGIN

typedef struct br_byte_buffer {
  uint8_t *data;
  size_t len;
  size_t cap;
  size_t off;
  br_allocator allocator;
  bool can_unread_byte;
} br_byte_buffer;

typedef br_io_result br_byte_buffer_io_result;

typedef struct br_byte_buffer_byte_result {
  uint8_t value;
  br_status status;
} br_byte_buffer_byte_result;

void br_byte_buffer_init(br_byte_buffer *buffer, br_allocator allocator);
br_status br_byte_buffer_init_copy(br_byte_buffer *buffer,
                                   br_bytes_view initial_data,
                                   br_allocator allocator);
void br_byte_buffer_destroy(br_byte_buffer *buffer);
void br_byte_buffer_reset(br_byte_buffer *buffer);

bool br_byte_buffer_is_empty(const br_byte_buffer *buffer);
size_t br_byte_buffer_len(const br_byte_buffer *buffer);
size_t br_byte_buffer_capacity(const br_byte_buffer *buffer);
br_bytes_view br_byte_buffer_view(const br_byte_buffer *buffer);

br_status br_byte_buffer_reserve(br_byte_buffer *buffer, size_t additional);
br_status br_byte_buffer_truncate(br_byte_buffer *buffer, size_t n);

br_byte_buffer_io_result br_byte_buffer_write(br_byte_buffer *buffer, br_bytes_view src);
br_status br_byte_buffer_write_byte(br_byte_buffer *buffer, uint8_t byte_value);

br_bytes_view br_byte_buffer_next(br_byte_buffer *buffer, size_t n);
br_byte_buffer_io_result br_byte_buffer_read(br_byte_buffer *buffer, void *dst, size_t dst_len);
br_byte_buffer_byte_result br_byte_buffer_read_byte(br_byte_buffer *buffer);
br_status br_byte_buffer_unread_byte(br_byte_buffer *buffer);

/*
Expose this buffer through the generic stream interface.
*/
br_stream br_byte_buffer_as_stream(br_byte_buffer *buffer);

static inline br_reader br_byte_buffer_as_reader(br_byte_buffer *buffer) {
  return br_byte_buffer_as_stream(buffer);
}

static inline br_writer br_byte_buffer_as_writer(br_byte_buffer *buffer) {
  return br_byte_buffer_as_stream(buffer);
}

BR_EXTERN_C_END

#endif
