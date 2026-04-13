#ifndef BEDROCK_BYTES_READER_H
#define BEDROCK_BYTES_READER_H

#include <bedrock/bytes/bytes.h>

BR_EXTERN_C_BEGIN

/*
A Reader is a read-only byte cursor over an existing byte slice.

This follows the shape of Odin's `bytes.Reader`: it does not own the source
memory, it tracks a current read index, and it supports random-access reads
without changing the main cursor.
*/
typedef struct br_byte_reader {
    br_bytes_view source;
    i64 index;
} br_byte_reader;

typedef struct br_byte_reader_io_result {
    usize count;
    br_status status;
} br_byte_reader_io_result;

typedef struct br_byte_reader_byte_result {
    u8 value;
    br_status status;
} br_byte_reader_byte_result;

typedef struct br_byte_reader_seek_result {
    i64 offset;
    br_status status;
} br_byte_reader_seek_result;

/*
Initialize a reader to consume `source`.

The reader does not copy or own `source`; the caller must keep the underlying
memory alive for the lifetime of the reader.
*/
void br_byte_reader_init(br_byte_reader *reader, br_bytes_view source);

/*
Reset a reader back to the beginning of its current source.
*/
void br_byte_reader_reset(br_byte_reader *reader);

/*
Return the unread portion of the source as a view.
*/
br_bytes_view br_byte_reader_view(const br_byte_reader *reader);

/*
Return the number of unread bytes remaining.

Like Odin's `reader_length`, this clamps at zero when the cursor is already at
or beyond the end of the source.
*/
usize br_byte_reader_len(const br_byte_reader *reader);

/*
Return the total byte size of the underlying source.
*/
usize br_byte_reader_size(const br_byte_reader *reader);

/*
Read into `dst` from the current cursor and advance the cursor by the number of
bytes copied.

This mirrors Odin's `reader_read`: reaching the end after a partial read does
not itself produce EOF. EOF is reported only when no bytes can be read.
*/
br_byte_reader_io_result br_byte_reader_read(br_byte_reader *reader, void *dst, usize dst_len);

/*
Read from an explicit `offset` without changing the main cursor.

This mirrors Odin's `reader_read_at`: a short read at the end of the source
returns the bytes that were copied and also reports EOF.
*/
br_byte_reader_io_result
br_byte_reader_read_at(const br_byte_reader *reader, void *dst, usize dst_len, i64 offset);

/*
Read one byte and advance the cursor by one.
*/
br_byte_reader_byte_result br_byte_reader_read_byte(br_byte_reader *reader);

/*
Move the cursor back by one byte.

This deliberately follows Odin's `reader_unread_byte` behavior: it is valid
whenever the current index is greater than zero, not only immediately after a
successful `read_byte`.
*/
br_status br_byte_reader_unread_byte(br_byte_reader *reader);

/*
Seek relative to the start, current cursor, or end of the source.

Like Odin's `reader_seek`, seeking past the end is allowed. Only negative final
offsets are rejected.
*/
br_byte_reader_seek_result
br_byte_reader_seek(br_byte_reader *reader, i64 offset, br_seek_from whence);

BR_EXTERN_C_END

#endif
