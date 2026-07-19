#ifndef BEDROCK_STRINGS_BUILDER_H
#define BEDROCK_STRINGS_BUILDER_H

#include <bedrock/strconv/strconv.h>
#include <bedrock/strings/strings.h>
#include <bedrock/io/io.h>

BR_EXTERN_C_BEGIN

/*
A dynamic byte buffer for building UTF-8 strings.

Allocation and ownership are explicit. A builder is either heap-backed and
growable, or backed by a caller-provided fixed buffer.
*/
typedef struct br_string_builder {
  char *data;
  size_t len;
  size_t cap;
  br_allocator allocator;
  bool owns_storage;
} br_string_builder;

typedef br_io_result br_string_builder_io_result;

typedef struct br_string_builder_byte_result {
  uint8_t value;
  br_status status;
} br_string_builder_byte_result;

typedef struct br_string_builder_rune_result {
  br_rune value;
  size_t width;
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
                                               size_t initial_capacity,
                                               br_allocator allocator);

/*
Initialize a builder over caller-provided storage.

The builder does not own the storage and cannot grow past `backing_len`.
*/
void br_string_builder_init_with_backing(br_string_builder *builder,
                                         void *backing,
                                         size_t backing_len);

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
size_t br_string_builder_len(const br_string_builder *builder);

/*
Return the total byte capacity of the current backing storage.
*/
size_t br_string_builder_capacity(const br_string_builder *builder);

/*
Return the remaining writable space before another growth would be required.
*/
size_t br_string_builder_space(const br_string_builder *builder);

/*
Return the current contents as a non-owning string view.
*/
br_string_view br_string_builder_view(const br_string_builder *builder);

/*
Ensure that at least `additional` more bytes can be appended.
*/
br_status br_string_builder_reserve(br_string_builder *builder, size_t additional);

/*
Truncate the builder to exactly `n` bytes.
*/
br_status br_string_builder_truncate(br_string_builder *builder, size_t n);

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
br_status br_string_builder_write_byte(br_string_builder *builder, uint8_t byte_value);

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

/*
Append the decimal (or `base`) text of an integer, formatted directly into the
builder's storage. `base` outside 2..36 yields `BR_STATUS_INVALID_ARGUMENT`.
*/
br_string_builder_io_result
br_string_builder_write_int(br_string_builder *builder, int64_t value, int base);
br_string_builder_io_result
br_string_builder_write_uint(br_string_builder *builder, uint64_t value, int base);

/*
Append the text of a floating-point value, formatted directly into the builder's
storage. `fmt`/`prec` follow `br_format_f64`/`br_format_f32`. `write_f32` formats
at `float` precision rather than promoting to `double`, so `0.1f` renders as
"0.1" (its shortest float form), not the wider double expansion.
*/
br_string_builder_io_result br_string_builder_write_f64(br_string_builder *builder,
                                                        double value,
                                                        br_float_format fmt,
                                                        int prec);
br_string_builder_io_result
br_string_builder_write_f32(br_string_builder *builder, float value, br_float_format fmt, int prec);

/*
Expose this builder through the generic stream interface.
*/
br_stream br_string_builder_as_stream(br_string_builder *builder);

static inline br_writer br_string_builder_as_writer(br_string_builder *builder) {
  return br_string_builder_as_stream(builder);
}

BR_EXTERN_C_END

#endif
