#ifndef BEDROCK_STRINGS_READER_H
#define BEDROCK_STRINGS_READER_H

#include <bedrock/strings/strings.h>

BR_EXTERN_C_BEGIN

/*
A Reader is a read-only byte and rune cursor over an existing UTF-8 string.

This follows the shape of Odin's `strings.Reader`: it does not own the source
memory, it tracks a current byte index, and it supports both byte-wise and
rune-wise reads over the same input.
*/
typedef struct br_string_reader {
  br_string_view source;
  i64 index;
  i64 prev_rune;
} br_string_reader;

typedef struct br_string_reader_io_result {
  usize count;
  br_status status;
} br_string_reader_io_result;

typedef struct br_string_reader_byte_result {
  u8 value;
  br_status status;
} br_string_reader_byte_result;

typedef struct br_string_reader_rune_result {
  br_rune value;
  usize width;
  br_status status;
} br_string_reader_rune_result;

typedef struct br_string_reader_seek_result {
  i64 offset;
  br_status status;
} br_string_reader_seek_result;

/*
Initialize a reader to consume `source`.

The reader does not copy or own `source`; the caller must keep the underlying
memory alive for the lifetime of the reader.
*/
void br_string_reader_init(br_string_reader *reader, br_string_view source);

/*
Reset a reader back to the beginning of its current source.
*/
void br_string_reader_reset(br_string_reader *reader);

/*
Return the unread portion of the source as a view.
*/
br_string_view br_string_reader_view(const br_string_reader *reader);

/*
Return the number of unread bytes remaining.

Like Odin's `reader_length`, this clamps at zero when the cursor is already at
or beyond the end of the source.
*/
usize br_string_reader_len(const br_string_reader *reader);

/*
Return the total byte size of the underlying source.
*/
usize br_string_reader_size(const br_string_reader *reader);

/*
Read into `dst` from the current cursor and advance the cursor by the number of
bytes copied.

This mirrors Odin's `reader_read`: reaching the end after a partial read does
not itself produce EOF. EOF is reported only when no bytes can be read.
*/
br_string_reader_io_result
br_string_reader_read(br_string_reader *reader, void *dst, usize dst_len);

/*
Read from an explicit `offset` without changing the main cursor.

This mirrors Odin's `reader_read_at`: a short read at the end of the source
returns the bytes that were copied and also reports EOF.
*/
br_string_reader_io_result
br_string_reader_read_at(const br_string_reader *reader, void *dst, usize dst_len, i64 offset);

/*
Read one byte and advance the cursor by one.
*/
br_string_reader_byte_result br_string_reader_read_byte(br_string_reader *reader);

/*
Move the cursor back by one byte.

This follows Odin's `reader_unread_byte`: it is valid whenever the current
index is greater than zero.
*/
br_status br_string_reader_unread_byte(br_string_reader *reader);

/*
Read one UTF-8 rune and advance the cursor by the encoded rune width.

Invalid encodings follow the underlying UTF-8 decoder behavior and are returned
as the replacement rune with width 1.
*/
br_string_reader_rune_result br_string_reader_read_rune(br_string_reader *reader);

/*
Move the cursor back to the start of the last rune returned by `read_rune`.

Like Odin's `reader_unread_rune`, this is only valid immediately after a
successful `read_rune` that has not been invalidated by another operation.
*/
br_status br_string_reader_unread_rune(br_string_reader *reader);

/*
Seek relative to the start, current cursor, or end of the source.

Like Odin's `reader_seek`, seeking past the end is allowed. Only negative final
offsets are rejected.
*/
br_string_reader_seek_result
br_string_reader_seek(br_string_reader *reader, i64 offset, br_seek_from whence);

BR_EXTERN_C_END

#endif
