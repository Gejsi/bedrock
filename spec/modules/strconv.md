# Strconv

Locale-independent number<->string parsing and formatting: integers (all
bases), booleans, and floats (f32/f64) through one shared decimal engine.

## Scope

| Facility | Bedrock surface | Status |
| --- | --- | --- |
| int parse/format, base 2-36 + base-0 infer | `br_parse_i64`/`_u64`, `br_format_i64`/`_u64` | planned |
| bool parse/format | `br_parse_bool`, `br_format_bool` | planned |
| float parse/format, f64 + f32 | `br_parse_f64`/`_f32`, `br_format_f64`/`_f32` | planned |

Deferred: quote/unquote (to the `ini` consumer; `unquote_string` first when it
lands). Excluded: a Go-style `Append*` family (the write-into-buffer form is the
append form; the strings builder covers accumulation), f16 (no standard C type).

Header `include/bedrock/strconv/strconv.h`; impl `src/strconv/` over an internal
fixed-buffer decimal engine. Module umbrella `include/bedrock/strconv.h`.

## Why this exists (design properties, not incidental)

C's `strtol`/`strtod` are the facilities this replaces, and it earns its place
by fixing their three standing defects — state these as the module's purpose:

1. **Rich, first-class errors.** `strtol` reports failure through the
   errno + endptr two-channel dance and cannot cleanly reject trailing junk.
   Bedrock returns a result struct with a `br_status` that distinguishes
   malformed syntax (`BR_STATUS_INVALID_ENCODING`) from value-out-of-range
   (`BR_STATUS_OUT_OF_RANGE`), plus a consumed-byte count — one channel, fully
   typed.
2. **Locale independence by construction.** `strtod`/`printf` honor
   `LC_NUMERIC`, so a program that calls `setlocale(LC_NUMERIC, "de_DE")` gets
   comma radix points from libc. Bedrock's radix point is ALWAYS `.`, digits are
   ALWAYS ASCII, and no libc locale facility is consulted. This is a deliberate
   correctness property with a regression test (see Testing).
3. **Exact shortest round-trip formatting**, which `%g` does not guarantee.

## Error Model

`br_status` gains `BR_STATUS_OUT_OF_RANGE` (appended to the enum end, no value
shift — same discipline as `BR_STATUS_INVALID_ENCODING`). It is ERANGE's cousin:
valid-syntax-but-unrepresentable is not caller misuse, so overloading
`INVALID_ARGUMENT` for it would be a lie.

| Condition | Status |
| --- | --- |
| malformed input (not a number in the given base) | `BR_STATUS_INVALID_ENCODING` |
| valid digits, value exceeds the target type | `BR_STATUS_OUT_OF_RANGE` |
| trailing junk after a strict parse | `BR_STATUS_INVALID_ENCODING` |
| base outside {0, 2..36} | `BR_STATUS_INVALID_ARGUMENT` (caller misuse) |
| output buffer too small | `BR_STATUS_SHORT_BUFFER` |

On `BR_STATUS_OUT_OF_RANGE`, the result's `value` still carries the SATURATED
result (±max for ints, ±HUGE_VAL/±0 for floats), following Go's ParseInt/atof:
callers that want the clamp have it, the status tells them it saturated.

## Parse API — strict by default

The obvious name does the SAFE thing. `br_parse_i64(s, base)` succeeds only if
the WHOLE view is a valid number; a permissive prefix-parse is the explicitly
named `_prefix` form. This inverts C's `atoi`/`strtol` default (silent partial
parse), which is C's most notorious numeric footgun; Go made `ParseInt` strict
for exactly this reason. Both forms report `consumed` (strict's is `s.len` on
success).

```c
typedef struct br_parse_i64_result  { int64_t  value; size_t consumed; br_status status; } br_parse_i64_result;
typedef struct br_parse_u64_result  { uint64_t value; size_t consumed; br_status status; } br_parse_u64_result;
typedef struct br_parse_f64_result  { double   value; size_t consumed; br_status status; } br_parse_f64_result;
typedef struct br_parse_f32_result  { float    value; size_t consumed; br_status status; } br_parse_f32_result;
typedef struct br_parse_bool_result { bool     value; size_t consumed; br_status status; } br_parse_bool_result;

/* STRICT: whole view must be the number, else INVALID_ENCODING. base 0 = infer
   0x/0o/0b prefix; else 2..36; outside {0,2..36} = INVALID_ARGUMENT. */
br_parse_i64_result br_parse_i64(br_string_view s, int base);
br_parse_u64_result br_parse_u64(br_string_view s, int base);
br_parse_f64_result br_parse_f64(br_string_view s);
br_parse_f32_result br_parse_f32(br_string_view s);
br_parse_bool_result br_parse_bool(br_string_view s); /* 1/t/true, 0/f/false */

/* PERMISSIVE: parse a leading number, allow trailing bytes; consumed = digits used. */
br_parse_i64_result br_parse_i64_prefix(br_string_view s, int base);
br_parse_u64_result br_parse_u64_prefix(br_string_view s, int base);
br_parse_f64_result br_parse_f64_prefix(br_string_view s);
br_parse_f32_result br_parse_f32_prefix(br_string_view s);
```

- Empty/NULL view → `INVALID_ENCODING`, consumed 0.
- i32/u32 are narrowing convenience wrappers over the i64/u64 forms with an
  `OUT_OF_RANGE` check; the canonical ABI stays 64-bit (mirrors the varint width
  decision — no width proliferation in the core).
- **f32 parses NATIVELY through the shared decimal engine at f32 precision — it
  does NOT parse as f64 and narrow.** decimal→f64→f32 double-rounds: some decimal
  strings round twice to a different value than the correctly-rounded f32. The
  engine is `Float_Info`-parameterized, so f32 runs the same path with
  `_f32_info`, correctly rounded in one step. This MUST be validated against the
  Paxson f32 vectors (see Testing); it is the sharpest correctness risk in the
  module.

## Format API — write into caller buffer, typed modes

```c
typedef enum br_float_format {
  BR_FLOAT_SHORTEST = 0, /* minimal digits that round-trip; `prec` ignored; default */
  BR_FLOAT_DECIMAL,      /* 'f': ddd.ddd, `prec` fractional digits */
  BR_FLOAT_EXPONENT,     /* 'e': d.ddde±dd, `prec` fraction digits */
  BR_FLOAT_GENERAL       /* 'g': %e for large/small exponent else %f, `prec` significant digits */
} br_float_format;

br_io_result br_format_i64(int64_t value, int base, uint8_t *dst, size_t dst_cap);
br_io_result br_format_u64(uint64_t value, int base, uint8_t *dst, size_t dst_cap);
br_io_result br_format_bool(bool value, uint8_t *dst, size_t dst_cap);
br_io_result br_format_f64(double value, br_float_format fmt, int prec, uint8_t *dst, size_t dst_cap);
br_io_result br_format_f32(float value, br_float_format fmt, int prec, uint8_t *dst, size_t dst_cap);

/* Worst-case byte counts. SHORTEST is bounded by a small constant; the other
   float modes are NOT — a huge-magnitude DECIMAL needs sign + up to 309
   integral digits + '.' + prec, so those need the arithmetic bound below. */
#define BR_FORMAT_I64_MAX          20   /* -9223372036854775808 */
#define BR_FORMAT_U64_MAX          20
#define BR_FORMAT_F64_SHORTEST_MAX 24   /* sign, digits, '.', 'e', exp sign, exp — shortest only */
#define BR_FORMAT_F32_SHORTEST_MAX 16

/* Worst-case bytes for ANY (fmt, prec) — the honest bound for non-shortest modes. */
size_t br_format_f64_bound(br_float_format fmt, int prec);
size_t br_format_f32_bound(br_float_format fmt, int prec);
```

- `prec` is IGNORED for `BR_FLOAT_SHORTEST` (it picks its own digit count) and is
  the fractional/significant count for the other modes (matching Go's
  FormatFloat prec; Bedrock uses the enum where Go/Odin use a fmt byte + prec=-1
  sentinel).
- Undersized `dst` (incl. `dst==NULL` / `dst_cap==0`) → `BR_STATUS_SHORT_BUFFER`,
  count 0. NEVER a truncated number — a half-written "3." is worse than an error.
- The `_SHORTEST_MAX` macros are honest ONLY for `BR_FLOAT_SHORTEST`; for any
  other mode callers must size with `br_format_f64_bound(fmt, prec)`. (The
  earlier "one MAX bounds any format" idea was wrong: DECIMAL of 1e308 alone
  needs ~309 digits.)

## Shared internal engine

One fixed-buffer multiprecision-decimal path (a `[384]`-byte digit buffer,
`Float_Info {mantbits, expbits, bias}` selecting f32/f64), ported from Odin's
`decimal` subpackage (itself a port of Go's legacy decimal). No bignum, no heap,
no multi-KB tables. Both parse (`decimal_to_float_bits`) and format
(`generic_ftoa`) select the `Float_Info` by type; the public `_f32`/`_f64` pairs
are typed entry points over this one engine (C has no generics, and typed funcs
beat a runtime `bit_size` arg a caller can pass wrong).

## Strings builder integration (dogfood consumer)

The blocked checklist rows. The builder OWNS an allocator, so it does NOT
stack-buffer — it reserves the worst-case bound, formats DIRECTLY into its tail,
and advances by the written count (zero copy, no fixed cap, and it makes
`br_format_*_bound` load-bearing dogfood):

```c
br_string_builder_io_result br_string_builder_write_int(br_string_builder *b, int64_t value, int base);
br_string_builder_io_result br_string_builder_write_uint(br_string_builder *b, uint64_t value, int base);
br_string_builder_io_result br_string_builder_write_f64(br_string_builder *b, double value,
                                                        br_float_format fmt, int prec);
br_string_builder_io_result br_string_builder_write_f32(br_string_builder *b, float value,
                                                        br_float_format fmt, int prec);
```

Both float widths get their own builder entry point: promoting an f32 to double
and formatting f64-SHORTEST prints the double's digits (`0.1f` →
"0.10000000149011612", not "0.1"), so `write_f32` must run the f32 format path.
Implementation per call: `n = br_format_*_bound(...)`; `br_string_builder_reserve(b, n)`;
format into the reserved tail; advance length by `result.count`.

## Deviations from Odin

- Strict-by-default parse (`_prefix` for permissive); Odin/Go/C default to
  permissive or all-or-nothing without the split.
- Rich `br_status` + consumed-length replaces Odin's bare `ok: bool` and C's
  errno+endptr. NAMED as the module's purpose.
- Full base 2..36 — Odin's signed path asserts `base <= 16`; Bedrock is an
  intentional superset (the digit-value helper extends to 'z'), and a bad base
  returns `INVALID_ARGUMENT` rather than Odin's `assert` panic (no-panic rule).
- base-0 recognizes `0x`/`0o`/`0b` only; Odin's `0d` (decimal) and `0z`
  (dozenal) are intentionally dropped as non-idiomatic for C.
- No underscore digit separators in v1 (Odin accepts `1_000` in every digit
  loop). Deferred; the designated future shape is Go's rule — underscores legal
  only in base-0 literal-style parses — if a config-style consumer asks.
- f32 parses natively at f32 precision, NOT via f64-then-narrow (double-rounding
  correctness); if Odin's `parse_f32` narrows from `parse_f64`, that is a
  suspected-bugs candidate (routed to scout).
- Typed `br_float_format` enum + `_f32`/`_f64` pairs replace Odin's fmt byte +
  int bit_size.
- Locale independence stated as a property (deviation from C strtod, not Odin).
- No `Append*` family; quote/unquote deferred; f16 excluded.
- Algorithm: fixed-buffer decimal; Bedrock DECLINES Rust's Eisel-Lemire/Grisu3
  speed bar deliberately — exact shortest-round-trip at minimal code/data cost is
  the v1 goal; revisit only if profiled hot.

## Testing

Correctness suite = Go's Paxson testbase, VENDORED into `tests/data/` with a
BSD-3 attribution notice (`tests/data/LICENSE-go`) — self-contained per the
vendored-consumption philosophy, no submodule-checkout dependency for CI. Tests
locate the directory via a `BR_TEST_DATA_DIR` macro passed by the Makefile
(keeps C+make purity, no path guessing). Files (from Go's
`src/internal/strconv/testdata/` after its 2025 restructure):
- `testfp.txt` — 4 fields/line (type, format-verb, input, expected output);
  `#`/blank lines skipped. Drives `format(input) == output`.
- `atof1k.txt` — one decimal float per line; round-trip parse→format→parse.
- `ftoa1k.txt` — one C99 hexfloat (`%a` form, exact bits) per line; parse the
  exact value, format shortest, check the expected decimal.

Plus: ported hand-picked atof/ftoa cases; int round-trip across all bases +
overflow→clean `OUT_OF_RANGE` (never wrap/UB); **the f32 double-rounding
vectors specifically** (guards the native-f32 requirement); the
locale-independence regression (set `LC_NUMERIC` to a comma-radix locale if
installed, assert `br_parse_f64("3.14")==3.14` and `br_format_f64` emits "3.14"
not "3,14"; skip cleanly if the locale is absent); ASan buffer-bound checks;
differential fuzz vs a correctly-rounded oracle (musl `strtod`).
