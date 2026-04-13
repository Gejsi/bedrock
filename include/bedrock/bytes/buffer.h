#ifndef BEDROCK_BYTES_BUFFER_H
#define BEDROCK_BYTES_BUFFER_H

#include <bedrock/bytes/bytes.h>

BR_EXTERN_C_BEGIN

typedef struct br_byte_buffer {
    u8 *data;
    usize len;
    usize cap;
    usize off;
    br_allocator allocator;
    int can_unread_byte;
} br_byte_buffer;

typedef struct br_byte_buffer_io_result {
    usize count;
    br_status status;
} br_byte_buffer_io_result;

typedef struct br_byte_buffer_byte_result {
    u8 value;
    br_status status;
} br_byte_buffer_byte_result;

void br_byte_buffer_init(br_byte_buffer *buffer, br_allocator allocator);
br_status br_byte_buffer_init_copy(
    br_byte_buffer *buffer,
    br_bytes_view initial_data,
    br_allocator allocator
);
void br_byte_buffer_destroy(br_byte_buffer *buffer);
void br_byte_buffer_reset(br_byte_buffer *buffer);

bool br_byte_buffer_is_empty(const br_byte_buffer *buffer);
usize br_byte_buffer_len(const br_byte_buffer *buffer);
usize br_byte_buffer_capacity(const br_byte_buffer *buffer);
br_bytes_view br_byte_buffer_view(const br_byte_buffer *buffer);

br_status br_byte_buffer_reserve(br_byte_buffer *buffer, usize additional);
br_status br_byte_buffer_truncate(br_byte_buffer *buffer, usize n);

br_byte_buffer_io_result br_byte_buffer_write(br_byte_buffer *buffer, br_bytes_view src);
br_status br_byte_buffer_write_byte(br_byte_buffer *buffer, u8 byte_value);

br_bytes_view br_byte_buffer_next(br_byte_buffer *buffer, usize n);
br_byte_buffer_io_result br_byte_buffer_read(br_byte_buffer *buffer, void *dst, usize dst_len);
br_byte_buffer_byte_result br_byte_buffer_read_byte(br_byte_buffer *buffer);
br_status br_byte_buffer_unread_byte(br_byte_buffer *buffer);

BR_EXTERN_C_END

#endif
