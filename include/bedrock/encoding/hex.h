#ifndef BEDROCK_ENCODING_HEX_H
#define BEDROCK_ENCODING_HEX_H

#include <bedrock/encoding/encoding.h>
#include <bedrock/io/io.h>

BR_EXTERN_C_BEGIN

/*
Letter case for hex encoding. Decoding is always case-insensitive.
*/
typedef enum br_hex_case { BR_HEX_LOWER = 0, BR_HEX_UPPER } br_hex_case;

/*
Number of encoded bytes produced for `src_len` input bytes (`src_len * 2`).
*/
static inline size_t br_hex_encoded_len(size_t src_len) {
  return src_len * 2u;
}

/*
Number of decoded bytes produced by a well-formed `encoded_len`-byte input
(`encoded_len / 2`). Odd inputs are malformed; the decoders report that.
*/
static inline size_t br_hex_decoded_len(size_t encoded_len) {
  return encoded_len / 2u;
}

/*
Encode `src` as a hex sequence into a freshly allocated buffer.

The returned `value` is owned by the caller and must be freed with
`br_bytes_free`. Encoding cannot fail on content; only `BR_STATUS_OUT_OF_MEMORY`
(allocation failure) is possible. Empty input yields empty owned bytes with
`BR_STATUS_OK`.
*/
br_bytes_result br_hex_encode(br_bytes_view src, br_hex_case letter_case, br_allocator allocator);

/*
Encode `src` as a hex sequence into the caller buffer `dst` of capacity
`dst_cap`.

Returns the number of bytes written in `count`. `BR_STATUS_SHORT_BUFFER` if
`dst_cap` is smaller than `br_hex_encoded_len(src.len)`; `BR_STATUS_INVALID_ARGUMENT`
if `dst` is NULL while output is required.
*/
br_io_result
br_hex_encode_into(br_bytes_view src, br_hex_case letter_case, uint8_t *dst, size_t dst_cap);

/*
Encode `src` as a hex sequence to the writer `w`.

Returns the number of bytes written in `count`. Propagates the writer's status
on a short or failed write.
*/
br_io_result br_hex_encode_to_writer(br_bytes_view src, br_hex_case letter_case, br_writer w);

/*
Decode the hex sequence `src` into a freshly allocated buffer.

The returned `value` is owned by the caller and must be freed with
`br_bytes_free`. Decoding is case-insensitive. Odd-length input is malformed:
`BR_STATUS_INVALID_ENCODING` with `error_offset = src.len - 1`. Any byte outside
`[0-9a-fA-F]` yields `BR_STATUS_INVALID_ENCODING` with `error_offset` at that
byte's index. On any failure the scratch buffer is freed before returning, so
`value` is always empty when `status` is not `BR_STATUS_OK`.
*/
br_decode_result br_hex_decode(br_bytes_view src, br_allocator allocator);

/*
Decode the hex sequence `src` into the caller buffer `dst` of capacity
`dst_cap`.

Returns the number of bytes written in `count`. Malformed input is reported the
same way as `br_hex_decode` (odd length, or a bad byte, both
`BR_STATUS_INVALID_ENCODING` with `error_offset`). `BR_STATUS_SHORT_BUFFER` if
`dst_cap` is smaller than `br_hex_decoded_len(src.len)`;
`BR_STATUS_INVALID_ARGUMENT` if `dst` is NULL while output is required.
*/
br_decode_into_result br_hex_decode_into(br_bytes_view src, uint8_t *dst, size_t dst_cap);

/*
Decode one byte from a two-character hex sequence, optionally prefixed with
`0x` or `0X`, e.g. `"23"`, `"0x23"`, or `"0X23"` all decode to `0x23`.

Returns the decoded byte in `value`. `BR_STATUS_INVALID_ENCODING` if, after
stripping any prefix, the input is not exactly two `[0-9a-fA-F]` characters.
*/
br_io_byte_result br_hex_decode_sequence(br_bytes_view seq);

BR_EXTERN_C_END

#endif
