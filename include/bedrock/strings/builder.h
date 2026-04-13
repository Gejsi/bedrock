#ifndef BEDROCK_STRINGS_BUILDER_H
#define BEDROCK_STRINGS_BUILDER_H

#include <bedrock/strings/strings.h>

BR_EXTERN_C_BEGIN

/*
A dynamic byte buffer for building UTF-8 strings.

This follows the broad role of Odin's `strings.Builder`, but the C version
keeps allocation and ownership explicit. A builder is either heap-backed and
growable, or backed by a caller-provided fixed buffer.
*/
typedef struct br_string_builder {
  char *data;
  usize len;
  usize cap;
  br_allocator allocator;
  int owns_storage;
} br_string_builder;

typedef struct br_string_builder_io_result {
  usize count;
  br_status status;
} br_string_builder_io_result;

typedef struct br_string_builder_byte_result {
  u8 value;
  br_status status;
} br_string_builder_byte_result;

typedef struct br_string_builder_rune_result {
  br_rune value;
  usize width;
  br_status status;
} br_string_builder_rune_result;

/*
Initialize an empty heap-backed builder.
*/
void br_string_builder_init(br_string_builder *builder, br_allocator allocator);

/*
Initialize a heap-backed builder with at least `initial_capacity` bytes.
*/
br_status br_string_builder_init_with_capacity(br_string_builder *builder,
                                               usize initial_capacity,
                                               br_allocator allocator);

/*
Initialize a builder over caller-provided storage.

This follows the intent of Odin's `builder_from_bytes`: the builder does not
own the storage and cannot grow past `backing_len`.
*/
void br_string_builder_init_with_backing(br_string_builder *builder,
                                         void *backing,
                                         usize backing_len);

/*
Release any owned storage and reset the builder to empty.
*/
void br_string_builder_destroy(br_string_builder *builder);

/*
Clear the builder contents without releasing capacity.
*/
void br_string_builder_reset(br_string_builder *builder);

/*
Return whether the builder currently holds no bytes.
*/
bool br_string_builder_is_empty(const br_string_builder *builder);

/*
Return the current byte length of the builder contents.
*/
usize br_string_builder_len(const br_string_builder *builder);

/*
Return the total byte capacity of the current backing storage.
*/
usize br_string_builder_capacity(const br_string_builder *builder);

/*
Return the remaining writable space before another growth would be required.
*/
usize br_string_builder_space(const br_string_builder *builder);

/*
Return the current contents as a non-owning string view.
*/
br_string_view br_string_builder_view(const br_string_builder *builder);

/*
Ensure that at least `additional` more bytes can be appended.
*/
br_status br_string_builder_reserve(br_string_builder *builder, usize additional);

/*
Truncate the builder to exactly `n` bytes.
*/
br_status br_string_builder_truncate(br_string_builder *builder, usize n);

/*
Clone the current builder contents into an owned string allocation.
*/
br_string_result br_string_builder_clone(const br_string_builder *builder, br_allocator allocator);

/*
Append `src` to the builder.
*/
br_string_builder_io_result br_string_builder_write(br_string_builder *builder, br_string_view src);

/*
Append one byte to the builder.
*/
br_status br_string_builder_write_byte(br_string_builder *builder, u8 byte_value);

/*
Append one rune to the builder encoded as UTF-8.
*/
br_string_builder_io_result br_string_builder_write_rune(br_string_builder *builder, br_rune value);

/*
Pop and return the last byte from the builder.
*/
br_string_builder_byte_result br_string_builder_pop_byte(br_string_builder *builder);

/*
Pop and return the last UTF-8 rune from the builder.

Invalid trailing encodings follow the UTF-8 decoder behavior and pop as a
replacement rune of width 1.
*/
br_string_builder_rune_result br_string_builder_pop_rune(br_string_builder *builder);

BR_EXTERN_C_END

#endif
