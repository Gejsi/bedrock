#ifndef BEDROCK_ENCODING_ENDIAN_H
#define BEDROCK_ENCODING_ENDIAN_H

#include <string.h>

#include <bedrock/bytes/bytes.h>

BR_EXTERN_C_BEGIN

/*
Fixed-width integer and float get/put with explicit byte order.

Integer paths assemble and disassemble values with shifts over individual
bytes, so they are agnostic to the host's endianness and never perform a wide
unaligned load -- there are no strict-aliasing or alignment concerns by
construction. Each byte is widened to the result type before it is shifted, so
the high byte never overflows a signed `int`. Float paths bit-cast through
`memcpy`, the portable, strict-aliasing-clean way to reinterpret between a
float and its bit pattern.

No `f16`: C11 has no `_Float16`, so 16-bit float support is deferred behind a
future feature gate (see spec/modules/encoding.md).

Each width offers four entry points, e.g. for `u16`:
- `br_endian_get_u16` / `br_endian_put_u16` -- bounds-checked over a
  `br_bytes_view` / `br_bytes`; return false when the buffer is too small (or a
  required pointer is NULL) without reading or writing any bytes.
- `br_endian_get_u16_unchecked` / `br_endian_put_u16_unchecked` -- fast paths
  over a raw pointer; the caller guarantees at least the width in bytes.
*/

BR_STATIC_ASSERT(sizeof(float) == 4, "br_endian f32 paths require a 32-bit float");
BR_STATIC_ASSERT(sizeof(double) == 8, "br_endian f64 paths require a 64-bit double");

typedef enum br_byte_order { BR_BYTE_ORDER_LITTLE = 0, BR_BYTE_ORDER_BIG } br_byte_order;

static inline br_byte_order br_byte_order_native(void) {
  uint16_t probe = 1u;
  uint8_t first;

  memcpy(&first, &probe, sizeof(first));
  return first != 0 ? BR_BYTE_ORDER_LITTLE : BR_BYTE_ORDER_BIG;
}

/* --- Unchecked get: caller guarantees the pointer spans at least the width. */

static inline uint16_t br_endian_get_u16_unchecked(const uint8_t *p, br_byte_order order) {
  if (order == BR_BYTE_ORDER_LITTLE) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
  }
  return (uint16_t)((uint16_t)p[1] | ((uint16_t)p[0] << 8));
}

static inline uint32_t br_endian_get_u32_unchecked(const uint8_t *p, br_byte_order order) {
  if (order == BR_BYTE_ORDER_LITTLE) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
  }
  return (uint32_t)p[3] | ((uint32_t)p[2] << 8) | ((uint32_t)p[1] << 16) | ((uint32_t)p[0] << 24);
}

static inline uint64_t br_endian_get_u64_unchecked(const uint8_t *p, br_byte_order order) {
  if (order == BR_BYTE_ORDER_LITTLE) {
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
  }
  return (uint64_t)p[7] | ((uint64_t)p[6] << 8) | ((uint64_t)p[5] << 16) | ((uint64_t)p[4] << 24) |
         ((uint64_t)p[3] << 32) | ((uint64_t)p[2] << 40) | ((uint64_t)p[1] << 48) |
         ((uint64_t)p[0] << 56);
}

static inline int16_t br_endian_get_i16_unchecked(const uint8_t *p, br_byte_order order) {
  return (int16_t)br_endian_get_u16_unchecked(p, order);
}

static inline int32_t br_endian_get_i32_unchecked(const uint8_t *p, br_byte_order order) {
  return (int32_t)br_endian_get_u32_unchecked(p, order);
}

static inline int64_t br_endian_get_i64_unchecked(const uint8_t *p, br_byte_order order) {
  return (int64_t)br_endian_get_u64_unchecked(p, order);
}

static inline float br_endian_get_f32_unchecked(const uint8_t *p, br_byte_order order) {
  uint32_t raw = br_endian_get_u32_unchecked(p, order);
  float value;

  memcpy(&value, &raw, sizeof(value));
  return value;
}

static inline double br_endian_get_f64_unchecked(const uint8_t *p, br_byte_order order) {
  uint64_t raw = br_endian_get_u64_unchecked(p, order);
  double value;

  memcpy(&value, &raw, sizeof(value));
  return value;
}

/* --- Unchecked put: caller guarantees the pointer spans at least the width. */

static inline void br_endian_put_u16_unchecked(uint8_t *p, br_byte_order order, uint16_t v) {
  if (order == BR_BYTE_ORDER_LITTLE) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
  } else {
    p[1] = (uint8_t)(v & 0xFFu);
    p[0] = (uint8_t)((v >> 8) & 0xFFu);
  }
}

static inline void br_endian_put_u32_unchecked(uint8_t *p, br_byte_order order, uint32_t v) {
  if (order == BR_BYTE_ORDER_LITTLE) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
  } else {
    p[3] = (uint8_t)(v & 0xFFu);
    p[2] = (uint8_t)((v >> 8) & 0xFFu);
    p[1] = (uint8_t)((v >> 16) & 0xFFu);
    p[0] = (uint8_t)((v >> 24) & 0xFFu);
  }
}

static inline void br_endian_put_u64_unchecked(uint8_t *p, br_byte_order order, uint64_t v) {
  if (order == BR_BYTE_ORDER_LITTLE) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
    p[4] = (uint8_t)((v >> 32) & 0xFFu);
    p[5] = (uint8_t)((v >> 40) & 0xFFu);
    p[6] = (uint8_t)((v >> 48) & 0xFFu);
    p[7] = (uint8_t)((v >> 56) & 0xFFu);
  } else {
    p[7] = (uint8_t)(v & 0xFFu);
    p[6] = (uint8_t)((v >> 8) & 0xFFu);
    p[5] = (uint8_t)((v >> 16) & 0xFFu);
    p[4] = (uint8_t)((v >> 24) & 0xFFu);
    p[3] = (uint8_t)((v >> 32) & 0xFFu);
    p[2] = (uint8_t)((v >> 40) & 0xFFu);
    p[1] = (uint8_t)((v >> 48) & 0xFFu);
    p[0] = (uint8_t)((v >> 56) & 0xFFu);
  }
}

static inline void br_endian_put_i16_unchecked(uint8_t *p, br_byte_order order, int16_t v) {
  br_endian_put_u16_unchecked(p, order, (uint16_t)v);
}

static inline void br_endian_put_i32_unchecked(uint8_t *p, br_byte_order order, int32_t v) {
  br_endian_put_u32_unchecked(p, order, (uint32_t)v);
}

static inline void br_endian_put_i64_unchecked(uint8_t *p, br_byte_order order, int64_t v) {
  br_endian_put_u64_unchecked(p, order, (uint64_t)v);
}

static inline void br_endian_put_f32_unchecked(uint8_t *p, br_byte_order order, float v) {
  uint32_t raw;

  memcpy(&raw, &v, sizeof(raw));
  br_endian_put_u32_unchecked(p, order, raw);
}

static inline void br_endian_put_f64_unchecked(uint8_t *p, br_byte_order order, double v) {
  uint64_t raw;

  memcpy(&raw, &v, sizeof(raw));
  br_endian_put_u64_unchecked(p, order, raw);
}

/* --- Bounds-checked get over a view; false (no read) if fewer than N bytes. */

static inline bool br_endian_get_u16(br_bytes_view b, br_byte_order order, uint16_t *out) {
  if (out == NULL || b.data == NULL || b.len < 2u) {
    return false;
  }
  *out = br_endian_get_u16_unchecked(b.data, order);
  return true;
}

static inline bool br_endian_get_u32(br_bytes_view b, br_byte_order order, uint32_t *out) {
  if (out == NULL || b.data == NULL || b.len < 4u) {
    return false;
  }
  *out = br_endian_get_u32_unchecked(b.data, order);
  return true;
}

static inline bool br_endian_get_u64(br_bytes_view b, br_byte_order order, uint64_t *out) {
  if (out == NULL || b.data == NULL || b.len < 8u) {
    return false;
  }
  *out = br_endian_get_u64_unchecked(b.data, order);
  return true;
}

static inline bool br_endian_get_i16(br_bytes_view b, br_byte_order order, int16_t *out) {
  uint16_t v;

  if (out == NULL || !br_endian_get_u16(b, order, &v)) {
    return false;
  }
  *out = (int16_t)v;
  return true;
}

static inline bool br_endian_get_i32(br_bytes_view b, br_byte_order order, int32_t *out) {
  uint32_t v;

  if (out == NULL || !br_endian_get_u32(b, order, &v)) {
    return false;
  }
  *out = (int32_t)v;
  return true;
}

static inline bool br_endian_get_i64(br_bytes_view b, br_byte_order order, int64_t *out) {
  uint64_t v;

  if (out == NULL || !br_endian_get_u64(b, order, &v)) {
    return false;
  }
  *out = (int64_t)v;
  return true;
}

static inline bool br_endian_get_f32(br_bytes_view b, br_byte_order order, float *out) {
  uint32_t v;

  if (out == NULL || !br_endian_get_u32(b, order, &v)) {
    return false;
  }
  memcpy(out, &v, sizeof(*out));
  return true;
}

static inline bool br_endian_get_f64(br_bytes_view b, br_byte_order order, double *out) {
  uint64_t v;

  if (out == NULL || !br_endian_get_u64(b, order, &v)) {
    return false;
  }
  memcpy(out, &v, sizeof(*out));
  return true;
}

/* --- Bounds-checked put; false (no write) if the destination is too small. */

static inline bool br_endian_put_u16(br_bytes dst, br_byte_order order, uint16_t v) {
  if (dst.data == NULL || dst.len < 2u) {
    return false;
  }
  br_endian_put_u16_unchecked(dst.data, order, v);
  return true;
}

static inline bool br_endian_put_u32(br_bytes dst, br_byte_order order, uint32_t v) {
  if (dst.data == NULL || dst.len < 4u) {
    return false;
  }
  br_endian_put_u32_unchecked(dst.data, order, v);
  return true;
}

static inline bool br_endian_put_u64(br_bytes dst, br_byte_order order, uint64_t v) {
  if (dst.data == NULL || dst.len < 8u) {
    return false;
  }
  br_endian_put_u64_unchecked(dst.data, order, v);
  return true;
}

static inline bool br_endian_put_i16(br_bytes dst, br_byte_order order, int16_t v) {
  return br_endian_put_u16(dst, order, (uint16_t)v);
}

static inline bool br_endian_put_i32(br_bytes dst, br_byte_order order, int32_t v) {
  return br_endian_put_u32(dst, order, (uint32_t)v);
}

static inline bool br_endian_put_i64(br_bytes dst, br_byte_order order, int64_t v) {
  return br_endian_put_u64(dst, order, (uint64_t)v);
}

static inline bool br_endian_put_f32(br_bytes dst, br_byte_order order, float v) {
  uint32_t raw;

  memcpy(&raw, &v, sizeof(raw));
  return br_endian_put_u32(dst, order, raw);
}

static inline bool br_endian_put_f64(br_bytes dst, br_byte_order order, double v) {
  uint64_t raw;

  memcpy(&raw, &v, sizeof(raw));
  return br_endian_put_u64(dst, order, raw);
}

BR_EXTERN_C_END

#endif
