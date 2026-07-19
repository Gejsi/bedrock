#ifndef BEDROCK_ENCODING_BASE64_H
#define BEDROCK_ENCODING_BASE64_H

#include <bedrock/encoding/encoding.h>
#include <bedrock/io/io.h>

BR_EXTERN_C_BEGIN

/*
Which 64-character alphabet a base64 encoding uses. The standard alphabet ends
in `+` and `/`; the URL-safe alphabet uses `-` and `_` so the output is safe in
URLs and filenames without escaping.
*/
typedef enum br_base64_alphabet { BR_BASE64_STD = 0, BR_BASE64_URL } br_base64_alphabet;

/*
A base64 encoding: an alphabet plus two behavior flags.

`padded` controls trailing `=`: when set, encode pads the output to a multiple
of four characters and decode requires that exact padding; when clear (a "raw"
encoding) encode emits no padding and decode forbids `=`.

`strict` controls decode canonicality: when set, decode rejects a final quantum
whose unused low bits are non-zero (so each input has one canonical encoding);
when clear, those bits are masked and the input is accepted.
*/
typedef struct br_base64_encoding {
  br_base64_alphabet alphabet;
  bool padded;
  bool strict;
} br_base64_encoding;

/* Standard alphabet, padded, non-strict. */
static inline br_base64_encoding br_base64_std(void) {
  br_base64_encoding enc;
  enc.alphabet = BR_BASE64_STD;
  enc.padded = true;
  enc.strict = false;
  return enc;
}

/* URL-safe alphabet, padded, non-strict. */
static inline br_base64_encoding br_base64_url(void) {
  br_base64_encoding enc;
  enc.alphabet = BR_BASE64_URL;
  enc.padded = true;
  enc.strict = false;
  return enc;
}

/* Standard alphabet, unpadded, non-strict. */
static inline br_base64_encoding br_base64_raw_std(void) {
  br_base64_encoding enc;
  enc.alphabet = BR_BASE64_STD;
  enc.padded = false;
  enc.strict = false;
  return enc;
}

/* URL-safe alphabet, unpadded, non-strict. */
static inline br_base64_encoding br_base64_raw_url(void) {
  br_base64_encoding enc;
  enc.alphabet = BR_BASE64_URL;
  enc.padded = false;
  enc.strict = false;
  return enc;
}

/*
Number of encoded characters produced for `src_len` input bytes: a padded
encoding rounds up to a multiple of four; a raw encoding emits only the
characters the final partial group needs.
*/
size_t br_base64_encoded_len(br_base64_encoding enc, size_t src_len);

/*
Exact number of decoded bytes a well-formed `src` yields under `enc`, or 0 when
`src`'s length cannot be valid (a padded length not a multiple of four, or a raw
length congruent to 1 mod 4). A return of 0 is also correct for empty input; the
decoders themselves validate content and report malformed input.
*/
size_t br_base64_decoded_len(br_base64_encoding enc, br_bytes_view src);

/*
Encode `src` into a freshly allocated buffer owned by the caller (free with
`br_bytes_free`). Encoding cannot fail on content; only
`BR_STATUS_OUT_OF_MEMORY` is possible. Empty input yields empty owned bytes.
*/
br_bytes_result br_base64_encode(br_base64_encoding enc, br_bytes_view src, br_allocator allocator);

/*
Encode `src` into the caller buffer `dst` of capacity `dst_cap`, returning the
count written. `BR_STATUS_SHORT_BUFFER` (count 0, nothing written) if `dst_cap`
is smaller than `br_base64_encoded_len(enc, src.len)`.
*/
br_io_result
br_base64_encode_into(br_base64_encoding enc, br_bytes_view src, uint8_t *dst, size_t dst_cap);

/*
Encode `src` to the writer `w`, returning the count written and propagating the
writer's status on a short or failed write.
*/
br_io_result br_base64_encode_to_writer(br_base64_encoding enc, br_bytes_view src, br_writer w);

/*
Decode `src` into a freshly allocated buffer owned by the caller (free with
`br_bytes_free`). A byte outside the active alphabet (including whitespace, which
is NOT skipped) yields `BR_STATUS_INVALID_ENCODING` with `error_offset` at that
byte; a structurally invalid length or misplaced/incorrect padding is also
`BR_STATUS_INVALID_ENCODING`. On any failure the scratch buffer is freed before
returning, so `value` is always empty when `status` is not `BR_STATUS_OK`.
*/
br_decode_result
br_base64_decode(br_base64_encoding enc, br_bytes_view src, br_allocator allocator);

/*
Decode `src` into the caller buffer `dst` of capacity `dst_cap`, returning the
count written. Malformed input is reported as in `br_base64_decode`.
`BR_STATUS_SHORT_BUFFER` (count 0) if `dst_cap` is smaller than the decoded
length.
*/
br_decode_into_result
br_base64_decode_into(br_base64_encoding enc, br_bytes_view src, uint8_t *dst, size_t dst_cap);

/*
Decode `src` to the writer `w`, returning the count written. Malformed input is
reported as in `br_base64_decode`; a short or failed write propagates the
writer's status.
*/
br_decode_into_result
br_base64_decode_to_writer(br_base64_encoding enc, br_bytes_view src, br_writer w);

BR_EXTERN_C_END

#endif
