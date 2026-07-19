#ifndef BEDROCK_SRC_STRCONV_INTERNAL_H
#define BEDROCK_SRC_STRCONV_INTERNAL_H

#include <bedrock/strconv/strconv.h>

/*
Multiprecision decimal used only for float parsing and formatting. `digits` are
ASCII '0'..'9', big-endian, with an implied decimal point `decimal_point` places
from the left. `trunc` records that digits past the buffer were dropped.
*/
#define BR__DECIMAL_MAX_DIGITS 384

typedef struct br__decimal {
  u8 digits[BR__DECIMAL_MAX_DIGITS];
  i32 count;
  i32 decimal_point;
  bool neg;
  bool trunc;
} br__decimal;

/*
Selects the IEEE-754 shape the shared engine rounds to. `br__f32_info` drives
native float parsing/formatting so f32 never rounds through double.
*/
typedef struct br__float_info {
  u32 mantbits;
  u32 expbits;
  i32 bias;
} br__float_info;

extern const br__float_info br__f32_info;
extern const br__float_info br__f64_info;

/* decimal.c */
void br__decimal_assign(br__decimal *d, u64 value);
void br__decimal_trim(br__decimal *d);
void br__decimal_shift(br__decimal *d, i32 shift);
bool br__decimal_can_round_up(const br__decimal *d, i32 nd);
void br__decimal_round(br__decimal *d, i32 nd);
void br__decimal_round_up(br__decimal *d, i32 nd);
void br__decimal_round_down(br__decimal *d, i32 nd);
u64 br__decimal_rounded_integer(const br__decimal *d);

/*
Round `d` to the shortest digit string that still parses back to the float with
mantissa `mant` and binary exponent `exp` under `info`.
*/
void br__decimal_round_shortest(br__decimal *d, u64 mant, i32 exp, const br__float_info *info);

/*
Convert a rounded decimal to float bits under `info`. `overflow` is set when the
magnitude exceeds the format's range (the caller saturates to infinity).
*/
u64 br__decimal_to_float_bits(br__decimal *d, const br__float_info *info, bool *overflow);

#endif
