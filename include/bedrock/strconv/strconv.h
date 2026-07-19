#ifndef BEDROCK_STRCONV_STRCONV_H
#define BEDROCK_STRCONV_STRCONV_H

#include <bedrock/io/io.h>
#include <bedrock/strings/strings.h>

BR_EXTERN_C_BEGIN

/*
Locale-independent number <-> string conversion for integers, booleans, and
floats. The radix point is always '.', digits are always ASCII, and no libc
locale facility is ever consulted, so results do not depend on `LC_NUMERIC`.

Parsing is strict by default: `br_parse_i64` and friends succeed only if the
whole view is a valid number. The `_prefix` variants parse a leading number and
report how many bytes they consumed, leaving any trailing bytes to the caller.
*/

typedef struct br_parse_i64_result {
  int64_t value;
  size_t consumed;
  br_status status;
} br_parse_i64_result;

typedef struct br_parse_u64_result {
  uint64_t value;
  size_t consumed;
  br_status status;
} br_parse_u64_result;

typedef struct br_parse_i32_result {
  int32_t value;
  size_t consumed;
  br_status status;
} br_parse_i32_result;

typedef struct br_parse_u32_result {
  uint32_t value;
  size_t consumed;
  br_status status;
} br_parse_u32_result;

typedef struct br_parse_f64_result {
  double value;
  size_t consumed;
  br_status status;
} br_parse_f64_result;

typedef struct br_parse_f32_result {
  float value;
  size_t consumed;
  br_status status;
} br_parse_f32_result;

typedef struct br_parse_bool_result {
  bool value;
  size_t consumed;
  br_status status;
} br_parse_bool_result;

/*
Parse the WHOLE view as a number, else `BR_STATUS_INVALID_ENCODING`.

`base` selects the numeral base: 0 infers `0x`/`0o`/`0b` from the prefix (else
decimal), or an explicit base in 2..36. A base outside {0, 2..36} is caller
misuse and returns `BR_STATUS_INVALID_ARGUMENT`. Digits beyond the base, an
empty or NULL view, or trailing bytes all yield `BR_STATUS_INVALID_ENCODING`.
Valid digits whose value exceeds the target type yield `BR_STATUS_OUT_OF_RANGE`
with `value` saturated to the nearest representable bound. On success `consumed`
equals the view length.
*/
br_parse_i64_result br_parse_i64(br_string_view s, int base);
br_parse_u64_result br_parse_u64(br_string_view s, int base);

/*
Narrowing convenience wrappers over the 64-bit forms; a value that does not fit
the 32-bit type yields `BR_STATUS_OUT_OF_RANGE` with `value` saturated.
*/
br_parse_i32_result br_parse_i32(br_string_view s, int base);
br_parse_u32_result br_parse_u32(br_string_view s, int base);

/*
Parse the whole view as a floating-point number. `+Inf`/`-Inf`/`Infinity` and
`NaN` are recognized case-insensitively. A value too large to represent yields
`BR_STATUS_OUT_OF_RANGE` with `value` saturated to the signed infinity; a value
too small underflows to a signed zero with `BR_STATUS_OK`. `br_parse_f32`
rounds directly to `float` precision (it does not round to `double` first).
*/
br_parse_f64_result br_parse_f64(br_string_view s);
br_parse_f32_result br_parse_f32(br_string_view s);

/*
Parse `1`/`t`/`true`/`0`/`f`/`false` (case-sensitive, matching the format
output). Anything else is `BR_STATUS_INVALID_ENCODING`.
*/
br_parse_bool_result br_parse_bool(br_string_view s);

/*
Parse a leading number and allow trailing bytes. `consumed` reports how many
bytes formed the number. An empty leading number is `BR_STATUS_INVALID_ENCODING`
with `consumed` 0.
*/
br_parse_i64_result br_parse_i64_prefix(br_string_view s, int base);
br_parse_u64_result br_parse_u64_prefix(br_string_view s, int base);
br_parse_i32_result br_parse_i32_prefix(br_string_view s, int base);
br_parse_u32_result br_parse_u32_prefix(br_string_view s, int base);
br_parse_f64_result br_parse_f64_prefix(br_string_view s);
br_parse_f32_result br_parse_f32_prefix(br_string_view s);

typedef enum br_float_format {
  BR_FLOAT_SHORTEST = 0, /* minimal digits that round-trip; `prec` ignored */
  BR_FLOAT_DECIMAL,      /* ddd.ddd, `prec` fractional digits */
  BR_FLOAT_EXPONENT,     /* d.ddde±dd, `prec` fractional digits */
  BR_FLOAT_GENERAL       /* exponent form for large/small exponents, else decimal */
} br_float_format;

/*
Format `value` into `dst`, writing at most `dst_cap` bytes and reporting the
count written. No terminating NUL is written. When `dst` is NULL, `dst_cap` is
zero, or the buffer cannot hold the whole result, the return is
`BR_STATUS_SHORT_BUFFER` with count 0 and no bytes written -- never a truncated
number. `base` outside 2..36 is `BR_STATUS_INVALID_ARGUMENT`.
*/
br_io_result br_format_i64(int64_t value, int base, uint8_t *dst, size_t dst_cap);
br_io_result br_format_u64(uint64_t value, int base, uint8_t *dst, size_t dst_cap);

/*
Format `value` as `true` or `false`.
*/
br_io_result br_format_bool(bool value, uint8_t *dst, size_t dst_cap);

/*
Format a floating-point number. `prec` is ignored for `BR_FLOAT_SHORTEST` and is
otherwise the count of fractional digits (`BR_FLOAT_DECIMAL`/`BR_FLOAT_EXPONENT`)
or significant digits (`BR_FLOAT_GENERAL`). A negative `prec` in any non-shortest
mode is `BR_STATUS_INVALID_ARGUMENT`. NaN and infinities format as `NaN`,
`+Inf`, and `-Inf`. Buffer rules match the integer formatters. `br_format_f32`
formats at `float` precision (it does not promote to `double`).
*/
br_io_result
br_format_f64(double value, br_float_format fmt, int prec, uint8_t *dst, size_t dst_cap);
br_io_result
br_format_f32(float value, br_float_format fmt, int prec, uint8_t *dst, size_t dst_cap);

/*
Worst-case byte counts. The `_SHORTEST_MAX` macros bound ONLY the
`BR_FLOAT_SHORTEST` mode; other float modes are unbounded by a constant (a
huge-magnitude `BR_FLOAT_DECIMAL` needs hundreds of integral digits), so size
those with `br_format_f64_bound`/`br_format_f32_bound`.
*/
#define BR_FORMAT_I64_MAX 20u          /* -9223372036854775808 */
#define BR_FORMAT_U64_MAX 20u          /* 18446744073709551615 */
#define BR_FORMAT_F64_SHORTEST_MAX 24u /* sign, digits, '.', 'e', exp sign, exp */
#define BR_FORMAT_F32_SHORTEST_MAX 16u

/*
Worst-case bytes to format any value in `(fmt, prec)`. The result is clamped to
a safe maximum, so a pathological `prec` cannot overflow the computation; the
formatter still reports `BR_STATUS_SHORT_BUFFER` if `dst` cannot hold it.
*/
size_t br_format_f64_bound(br_float_format fmt, int prec);
size_t br_format_f32_bound(br_float_format fmt, int prec);

BR_EXTERN_C_END

#endif
