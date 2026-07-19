#ifndef BEDROCK_ENCODING_ENCODING_H
#define BEDROCK_ENCODING_ENCODING_H

#include <bedrock/bytes/bytes.h>

BR_EXTERN_C_BEGIN

/*
Result of an allocating decode.

`value` is owned by the caller and must be freed with `br_bytes_free`. On any
failure it is empty (`{NULL, 0}`); decoders that allocate before detecting a
fault free that scratch buffer before returning, so a non-OK status never hands
back a live allocation.

`error_offset` is the index of the first offending input byte when `status` is
`BR_STATUS_INVALID_ENCODING`, the count of input bytes consumed before the input
ran out when `BR_STATUS_UNEXPECTED_EOF`, and `0` (meaningless) otherwise.
*/
typedef struct br_decode_result {
  br_bytes value;
  size_t error_offset;
  br_status status;
} br_decode_result;

/*
Result of an into-buffer or to-stream decode.

`count` is the number of bytes written to the destination buffer or writer.
`error_offset` follows the same convention as `br_decode_result`.
*/
typedef struct br_decode_into_result {
  size_t count;
  size_t error_offset;
  br_status status;
} br_decode_into_result;

BR_EXTERN_C_END

#endif
