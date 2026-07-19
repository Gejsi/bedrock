#include <bedrock/encoding/varint.h>

static br_uleb128_result br__uleb128_result(u64 value, usize size, br_status status) {
  br_uleb128_result r;

  r.value = value;
  r.size = size;
  r.status = status;
  return r;
}

static br_ileb128_result br__ileb128_result(i64 value, usize size, br_status status) {
  br_ileb128_result r;

  r.value = value;
  r.size = size;
  r.status = status;
  return r;
}

size_t br_uleb128_encoded_len(uint64_t value) {
  usize n = 1u;

  while (value >= 0x80u) {
    value >>= 7;
    n += 1u;
  }
  return n;
}

size_t br_ileb128_encoded_len(int64_t value) {
  /* Walk the same termination rule the encoder uses, without writing bytes. */
  usize n = 0u;
  bool more = true;

  while (more) {
    u8 byte = (u8)(value & 0x7Fu);
    value >>= 7; /* arithmetic shift preserves the sign */
    n += 1u;
    if ((value == 0 && (byte & 0x40u) == 0u) || (value == -1 && (byte & 0x40u) != 0u)) {
      more = false;
    }
  }
  return n;
}

br_io_result br_uleb128_encode(uint64_t value, uint8_t *dst, size_t dst_cap) {
  u8 tmp[BR_LEB128_MAX_BYTES];
  usize n = 0u;

  do {
    u8 byte = (u8)(value & 0x7Fu);
    value >>= 7;
    if (value != 0u) {
      byte |= 0x80u;
    }
    tmp[n] = byte;
    n += 1u;
  } while (value != 0u);

  if (dst == NULL || dst_cap < n) {
    return br_io_result_make(0u, BR_STATUS_SHORT_BUFFER);
  }
  memcpy(dst, tmp, n);
  return br_io_result_make(n, BR_STATUS_OK);
}

br_io_result br_ileb128_encode(int64_t value, uint8_t *dst, size_t dst_cap) {
  u8 tmp[BR_LEB128_MAX_BYTES];
  usize n = 0u;
  bool more = true;

  while (more) {
    u8 byte = (u8)(value & 0x7Fu);
    value >>= 7; /* arithmetic shift preserves the sign */
    if ((value == 0 && (byte & 0x40u) == 0u) || (value == -1 && (byte & 0x40u) != 0u)) {
      more = false;
    } else {
      byte |= 0x80u;
    }
    tmp[n] = byte;
    n += 1u;
  }

  if (dst == NULL || dst_cap < n) {
    return br_io_result_make(0u, BR_STATUS_SHORT_BUFFER);
  }
  memcpy(dst, tmp, n);
  return br_io_result_make(n, BR_STATUS_OK);
}

br_uleb128_result br_uleb128_decode(br_bytes_view src) {
  u64 value = 0u;
  usize i = 0u;

  while (i < src.len) {
    u8 byte = src.data[i];
    u32 shift = (u32)(i * 7u);

    /*
    On the 10th byte only the low bit fits (9*7 = 63 bits already placed); any
    higher bit overflows u64. An 11th byte is always too much.
    */
    if (i == BR_LEB128_MAX_BYTES - 1u) {
      if (byte > 0x01u) {
        return br__uleb128_result(0u, i + 1u, BR_STATUS_INVALID_ENCODING);
      }
    } else if (i >= BR_LEB128_MAX_BYTES) {
      return br__uleb128_result(0u, i, BR_STATUS_INVALID_ENCODING);
    }

    value |= (u64)(byte & 0x7Fu) << shift;
    i += 1u;
    if ((byte & 0x80u) == 0u) {
      return br__uleb128_result(value, i, BR_STATUS_OK);
    }
  }

  /* Ran out of input with a continuation bit still pending. */
  return br__uleb128_result(0u, i, BR_STATUS_UNEXPECTED_EOF);
}

br_ileb128_result br_ileb128_decode(br_bytes_view src) {
  u64 value = 0u;
  usize i = 0u;

  while (i < src.len) {
    u8 byte = src.data[i];
    u32 shift = (u32)(i * 7u);

    if (i == BR_LEB128_MAX_BYTES - 1u) {
      /*
      63 payload bits are already placed, so bit 63 is the only new value bit
      the 10th byte contributes; its remaining bits are sign padding and must
      all match that sign bit. Valid 10th bytes are therefore 0x00 (positive,
      padding 0) and 0x7F (negative, padding 1); anything else is malformed.
      INT64_MIN legitimately encodes to a 10-byte form ending in 0x7F.
      */
      if (byte != 0x00u && byte != 0x7Fu) {
        return br__ileb128_result(0, i + 1u, BR_STATUS_INVALID_ENCODING);
      }
    } else if (i >= BR_LEB128_MAX_BYTES) {
      return br__ileb128_result(0, i, BR_STATUS_INVALID_ENCODING);
    }

    value |= (u64)(byte & 0x7Fu) << shift;
    i += 1u;
    if ((byte & 0x80u) == 0u) {
      /* Sign-extend from the final group's sign bit if the value is negative. */
      if (shift + 7u < 64u && (byte & 0x40u) != 0u) {
        value |= ~(u64)0u << (shift + 7u);
      }
      return br__ileb128_result((i64)value, i, BR_STATUS_OK);
    }
  }

  return br__ileb128_result(0, i, BR_STATUS_UNEXPECTED_EOF);
}
