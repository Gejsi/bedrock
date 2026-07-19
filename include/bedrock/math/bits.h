#ifndef BEDROCK_MATH_BITS_H
#define BEDROCK_MATH_BITS_H

#include <bedrock/base.h>

BR_EXTERN_C_BEGIN

/*
Fixed-width bit operations ported from Odin `core/math/bits` (modeled on Go's
`math/bits`). See spec/modules/math.md.

Count/scan operations are TOTAL: they are defined at 0 (unlike the raw
`__builtin_*`, whose zero input is undefined). `clz`/`ctz` return the bit width
for 0; `bit_width` returns 0 for 0. There is no standalone `log2` — it is a
footgun at 0 (Odin returns a silent `max(T)`); use `bit_width`, where
`floor(log2 x) == bit_width(x) - 1` for `x > 0`.

Byte-order helpers (from_be/to_le) are NOT here; see the encoding/endian module.
This module only provides the endianness-agnostic `byteswap`.
*/

/* Counts and scans (total; defined at 0). Return int for ergonomic arithmetic. */
int br_bits_count_ones_u8(uint8_t x);
int br_bits_count_ones_u16(uint16_t x);
int br_bits_count_ones_u32(uint32_t x);
int br_bits_count_ones_u64(uint64_t x);

int br_bits_count_zeros_u8(uint8_t x);
int br_bits_count_zeros_u16(uint16_t x);
int br_bits_count_zeros_u32(uint32_t x);
int br_bits_count_zeros_u64(uint64_t x);

int br_bits_clz_u8(uint8_t x);   /* leading zeros; 8 if x == 0 */
int br_bits_clz_u16(uint16_t x); /* 16 if x == 0 */
int br_bits_clz_u32(uint32_t x); /* 32 if x == 0 */
int br_bits_clz_u64(uint64_t x); /* 64 if x == 0 */

int br_bits_ctz_u8(uint8_t x);   /* trailing zeros; 8 if x == 0 */
int br_bits_ctz_u16(uint16_t x); /* 16 if x == 0 */
int br_bits_ctz_u32(uint32_t x); /* 32 if x == 0 */
int br_bits_ctz_u64(uint64_t x); /* 64 if x == 0 */

int br_bits_bit_width_u8(uint8_t x); /* min bits to represent x; 0 if x == 0 */
int br_bits_bit_width_u16(uint16_t x);
int br_bits_bit_width_u32(uint32_t x);
int br_bits_bit_width_u64(uint64_t x);

/* Bit and byte rearrangement. */
uint8_t br_bits_reverse_u8(uint8_t x);
uint16_t br_bits_reverse_u16(uint16_t x);
uint32_t br_bits_reverse_u32(uint32_t x);
uint64_t br_bits_reverse_u64(uint64_t x);

uint16_t br_bits_byteswap_u16(uint16_t x);
uint32_t br_bits_byteswap_u32(uint32_t x);
uint64_t br_bits_byteswap_u64(uint64_t x);

/* Rotations. `k` is taken modulo the width, so any `k` is well-defined. */
uint8_t br_bits_rotate_left_u8(uint8_t x, unsigned k);
uint16_t br_bits_rotate_left_u16(uint16_t x, unsigned k);
uint32_t br_bits_rotate_left_u32(uint32_t x, unsigned k);
uint64_t br_bits_rotate_left_u64(uint64_t x, unsigned k);

uint8_t br_bits_rotate_right_u8(uint8_t x, unsigned k);
uint16_t br_bits_rotate_right_u16(uint16_t x, unsigned k);
uint32_t br_bits_rotate_right_u32(uint32_t x, unsigned k);
uint64_t br_bits_rotate_right_u64(uint64_t x, unsigned k);

/* Power-of-two predicates. `base.h` keeps `br_is_power_of_two_size` for
   `size_t`; these are the fixed-width family. Signed variants report false for
   non-positive inputs. */
bool br_bits_is_power_of_two_u8(uint8_t x);
bool br_bits_is_power_of_two_u16(uint16_t x);
bool br_bits_is_power_of_two_u32(uint32_t x);
bool br_bits_is_power_of_two_u64(uint64_t x);
bool br_bits_is_power_of_two_i8(int8_t x);
bool br_bits_is_power_of_two_i16(int16_t x);
bool br_bits_is_power_of_two_i32(int32_t x);
bool br_bits_is_power_of_two_i64(int64_t x);

/* Add with carry: sum plus a carry-out bit. `carry_in` must be 0 or 1. */
typedef struct br_bits_add_u32_result {
  uint32_t sum;
  bool carry_out;
} br_bits_add_u32_result;

typedef struct br_bits_add_u64_result {
  uint64_t sum;
  bool carry_out;
} br_bits_add_u64_result;

br_bits_add_u32_result br_bits_add_u32(uint32_t x, uint32_t y, bool carry_in);
br_bits_add_u64_result br_bits_add_u64(uint64_t x, uint64_t y, bool carry_in);

/* Subtract with borrow. `borrow_in` must be 0 or 1. */
typedef struct br_bits_sub_u32_result {
  uint32_t diff;
  bool borrow_out;
} br_bits_sub_u32_result;

typedef struct br_bits_sub_u64_result {
  uint64_t diff;
  bool borrow_out;
} br_bits_sub_u64_result;

br_bits_sub_u32_result br_bits_sub_u32(uint32_t x, uint32_t y, bool borrow_in);
br_bits_sub_u64_result br_bits_sub_u64(uint64_t x, uint64_t y, bool borrow_in);

/* Full-width multiply: the (hi, lo) halves of x*y, following Odin and Go's
   `bits.Mul` order (note: Rust's `widening_mul` returns (lo, hi) instead). */
typedef struct br_bits_mul_u32_result {
  uint32_t hi;
  uint32_t lo;
} br_bits_mul_u32_result;

typedef struct br_bits_mul_u64_result {
  uint64_t hi;
  uint64_t lo;
} br_bits_mul_u64_result;

br_bits_mul_u32_result br_bits_mul_u32(uint32_t x, uint32_t y);
br_bits_mul_u64_result br_bits_mul_u64(uint64_t x, uint64_t y);

/* Divide the double-width value (hi:lo) by y. Never panics: on a divide error
   (y == 0) or quotient overflow (y <= hi), status is
   BR_STATUS_INVALID_ARGUMENT and quo/rem are 0. */
typedef struct br_bits_div_u32_result {
  uint32_t quo;
  uint32_t rem;
  br_status status;
} br_bits_div_u32_result;

typedef struct br_bits_div_u64_result {
  uint64_t quo;
  uint64_t rem;
  br_status status;
} br_bits_div_u64_result;

br_bits_div_u32_result br_bits_div_u32(uint32_t hi, uint32_t lo, uint32_t y);
br_bits_div_u64_result br_bits_div_u64(uint64_t hi, uint64_t lo, uint64_t y);

/* Bitfields. The caller guarantees `offset + bits <= width`; out-of-range
   values are unchecked, like C shift operands. `extract_iN` sign-extends the
   field. */
uint8_t br_bits_field_extract_u8(uint8_t value, unsigned offset, unsigned bits);
uint16_t br_bits_field_extract_u16(uint16_t value, unsigned offset, unsigned bits);
uint32_t br_bits_field_extract_u32(uint32_t value, unsigned offset, unsigned bits);
uint64_t br_bits_field_extract_u64(uint64_t value, unsigned offset, unsigned bits);

int8_t br_bits_field_extract_i8(int8_t value, unsigned offset, unsigned bits);
int16_t br_bits_field_extract_i16(int16_t value, unsigned offset, unsigned bits);
int32_t br_bits_field_extract_i32(int32_t value, unsigned offset, unsigned bits);
int64_t br_bits_field_extract_i64(int64_t value, unsigned offset, unsigned bits);

uint8_t br_bits_field_insert_u8(uint8_t base, uint8_t insert, unsigned offset, unsigned bits);
uint16_t br_bits_field_insert_u16(uint16_t base, uint16_t insert, unsigned offset, unsigned bits);
uint32_t br_bits_field_insert_u32(uint32_t base, uint32_t insert, unsigned offset, unsigned bits);
uint64_t br_bits_field_insert_u64(uint64_t base, uint64_t insert, unsigned offset, unsigned bits);

int8_t br_bits_field_insert_i8(int8_t base, int8_t insert, unsigned offset, unsigned bits);
int16_t br_bits_field_insert_i16(int16_t base, int16_t insert, unsigned offset, unsigned bits);
int32_t br_bits_field_insert_i32(int32_t base, int32_t insert, unsigned offset, unsigned bits);
int64_t br_bits_field_insert_i64(int64_t base, int64_t insert, unsigned offset, unsigned bits);

BR_EXTERN_C_END

#endif
