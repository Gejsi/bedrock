#ifndef BEDROCK_ENCODING_VARINT_H
#define BEDROCK_ENCODING_VARINT_H

#include <bedrock/bytes/bytes.h>
#include <bedrock/io/io.h>

BR_EXTERN_C_BEGIN

/*
LEB128 variable-length integers over the 64-bit ABI. Unsigned values use plain
LEB128; signed values use DWARF sign-extended LEB128 (not zig-zag). A 64-bit
value needs at most 10 bytes.
*/
#define BR_LEB128_MAX_BYTES 10

/*
Decoded unsigned/signed results. `size` is the number of input bytes consumed:
the varint length on success, and how far decoding got on error (there is no
per-byte error offset -- a malformed varint is malformed as a unit).
*/
typedef struct br_uleb128_result {
  uint64_t value;
  size_t size;
  br_status status;
} br_uleb128_result;

typedef struct br_ileb128_result {
  int64_t value;
  size_t size;
  br_status status;
} br_ileb128_result;

/*
Number of bytes the value encodes to (1..10).
*/
size_t br_uleb128_encoded_len(uint64_t value);
size_t br_ileb128_encoded_len(int64_t value);

/*
Encode `value` into `dst`, returning the count written. `BR_STATUS_SHORT_BUFFER`
(count 0, nothing written) if `dst` is NULL or `dst_cap` is smaller than the
encoded length.
*/
br_io_result br_uleb128_encode(uint64_t value, uint8_t *dst, size_t dst_cap);
br_io_result br_ileb128_encode(int64_t value, uint8_t *dst, size_t dst_cap);

/*
Decode the LEB128 varint at the front of `src`.

On success `status` is `BR_STATUS_OK` and `size` is the varint length. A buffer
that ends while a continuation bit is still set (including an empty buffer)
yields `BR_STATUS_UNEXPECTED_EOF` with `size` set to the bytes consumed. A value
that does not fit the 64-bit target -- an 11th byte, or a 10th byte whose
high bits exceed what the type can hold (for signed, whose padding bits do not
all match the sign bit) -- yields `BR_STATUS_INVALID_ENCODING`.
*/
br_uleb128_result br_uleb128_decode(br_bytes_view src);
br_ileb128_result br_ileb128_decode(br_bytes_view src);

BR_EXTERN_C_END

#endif
