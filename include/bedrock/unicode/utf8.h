#ifndef BEDROCK_UNICODE_UTF8_H
#define BEDROCK_UNICODE_UTF8_H

#include <bedrock/bytes/bytes.h>

BR_EXTERN_C_BEGIN

typedef i32 br_rune;

#define BR_RUNE_ERROR ((br_rune)0xfffd)
#define BR_RUNE_SELF ((br_rune)0x80)
#define BR_RUNE_BOM ((br_rune)0xfeff)
#define BR_RUNE_EOF ((br_rune) - 1)
#define BR_RUNE_MAX ((br_rune)0x10ffff)
#define BR_UTF8_MAX 4

typedef struct br_utf8_decode_result {
  br_rune value;
  usize width;
} br_utf8_decode_result;

typedef struct br_utf8_encode_result {
  u8 bytes[BR_UTF8_MAX];
  usize len;
} br_utf8_encode_result;

/*
Encode `value` as UTF-8.

This follows Odin's `encode_rune`: invalid values, including surrogate code
points and values above the Unicode maximum, are encoded as the replacement
rune.
*/
br_utf8_encode_result br_utf8_encode(br_rune value);

/*
Decode the first rune in `s`.

This follows Odin's `decode_rune`: invalid encodings decode as the replacement
rune with width 1, and an empty input decodes as the replacement rune with
width 0.
*/
br_utf8_decode_result br_utf8_decode(br_bytes_view s);

/*
Decode the last rune in `s`.

This follows Odin's `decode_last_rune`: invalid trailing encodings decode as
the replacement rune with width 1, and an empty input decodes as the
replacement rune with width 0.
*/
br_utf8_decode_result br_utf8_decode_last(br_bytes_view s);

/*
Report whether `value` is a valid Unicode scalar value.
*/
bool br_utf8_valid_rune(br_rune value);

/*
Report whether `s` is entirely valid UTF-8.
*/
bool br_utf8_valid(br_bytes_view s);

/*
Report whether `byte_value` can begin a UTF-8 encoded rune.
*/
bool br_utf8_rune_start(u8 byte_value);

/*
Count runes in `s`.

Like Odin's `rune_count`, invalid or truncated encodings count as width-1 error
runes rather than being skipped.
*/
usize br_utf8_rune_count(br_bytes_view s);

/*
Return the number of bytes required to encode `value`, or `-1` if `value` is
not a valid Unicode scalar value.
*/
i32 br_utf8_rune_size(br_rune value);

/*
Report whether `s` begins with a complete UTF-8 encoding.

Like Odin's `full_rune`, invalid encodings are considered complete because they
decode as a width-1 replacement rune.
*/
bool br_utf8_full_rune(br_bytes_view s);

BR_EXTERN_C_END

#endif
