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

## Conventions

- Signatures are spelled in standard C types (`int64_t`, `size_t`, `double`),
  matching the public-header rule; short aliases stay internal.
- Parse input is a `br_string_view`; format output is written into a
  caller-provided `uint8_t *dst, size_t dst_cap` buffer.
- Symbols are named by what they do — `br_parse_*` / `br_format_*`, NOT a
  `br_strconv_` prefix. "strconv" is a source-ecosystem noun a C consumer does
  not know; the verb is the honest name (same reasoning as the header-comment
  provenance sweep). The directory `include/bedrock/strconv/` keeps the module
  name.
- **Locale independence by construction** (a first-class property, not an
  accident): the radix point is ALWAYS `.`, digits are ALWAYS ASCII, and no
  libc locale facility (`strtod`/`snprintf`/`LC_NUMERIC`) is ever consulted.
  C's `strtod`/`printf` honor `LC_NUMERIC`, so a program that runs
  `setlocale(LC_NUMERIC, "de_DE")` gets comma radix points from libc; Bedrock
  never does. Guarded by a regression test (see Testing).

## Error Model

`br_status` gains `BR_STATUS_OUT_OF_RANGE` (appended to the enum end, no value
shift — same discipline as `BR_STATUS_INVALID_ENCODING`). It is `ERANGE`'s
cousin: valid-syntax-but-unrepresentable is not caller misuse, so overloading
`INVALID_ARGUMENT` for it would be a lie.

| Condition | Status |
| --- | --- |
| malformed input (not a number in the given base) | `BR_STATUS_INVALID_ENCODING` |
| trailing bytes after a strict parse | `BR_STATUS_INVALID_ENCODING` |
| valid digits, value exceeds the target type | `BR_STATUS_OUT_OF_RANGE` |
| base outside {0, 2..36} | `BR_STATUS_INVALID_ARGUMENT` (caller misuse) |
| output buffer too small (incl. NULL / 0-cap) | `BR_STATUS_SHORT_BUFFER` |

On `BR_STATUS_OUT_OF_RANGE` the result's `value` still carries the SATURATED
result (±max for ints, ±HUGE_VAL/±0 for floats), following Go's ParseInt/atof:
callers that want the clamp have it, the status tells them it saturated.

## Parse API — strict by default

The obvious name does the SAFE thing. `br_parse_i64(s, base)` succeeds only if
the WHOLE view is a valid number; the permissive prefix-parse is the explicitly
named `_prefix` form. This inverts C's `atoi`/`strtol` silent-partial-parse
default — C's most notorious numeric footgun — which is why Go made `ParseInt`
strict. Both forms report `consumed` (strict's is `s.len` on success).

```c
typedef struct br_parse_i64_result  { int64_t  value; size_t consumed; br_status status; } br_parse_i64_result;
typedef struct br_parse_u64_result  { uint64_t value; size_t consumed; br_status status; } br_parse_u64_result;
typedef struct br_parse_f64_result  { double   value; size_t consumed; br_status status; } br_parse_f64_result;
typedef struct br_parse_f32_result  { float    value; size_t consumed; br_status status; } br_parse_f32_result;
typedef struct br_parse_bool_result { bool     value; size_t consumed; br_status status; } br_parse_bool_result;

/* STRICT: the whole view must be the number, else INVALID_ENCODING.
   base 0 = infer 0x/0o/0b prefix; else 2..36; outside {0,2..36} = INVALID_ARGUMENT. */
br_parse_i64_result  br_parse_i64(br_string_view s, int base);
br_parse_u64_result  br_parse_u64(br_string_view s, int base);
br_parse_f64_result  br_parse_f64(br_string_view s);
br_parse_f32_result  br_parse_f32(br_string_view s);
br_parse_bool_result br_parse_bool(br_string_view s); /* 1/t/true, 0/f/false */

/* PERMISSIVE: parse a leading number, allow trailing bytes; consumed = bytes used. */
br_parse_i64_result br_parse_i64_prefix(br_string_view s, int base);
br_parse_u64_result br_parse_u64_prefix(br_string_view s, int base);
br_parse_f64_result br_parse_f64_prefix(br_string_view s);
br_parse_f32_result br_parse_f32_prefix(br_string_view s);
```

- Empty or NULL-data view → `INVALID_ENCODING`, `consumed = 0`.
- i32/u32 are narrowing convenience wrappers over the i64/u64 forms with an
  `OUT_OF_RANGE` check; the canonical ABI stays 64-bit (mirrors the varint width
  decision — no width proliferation in the core).
- **f32 parses NATIVELY through the shared decimal engine at f32 precision — it
  does NOT parse as f64 and narrow.** decimal->f64->f32 double-rounds: there
  exist decimal strings whose twice-rounded result differs from the correctly-
  rounded f32. The engine is `Float_Info`-parameterized, so f32 runs the same
  path with `_f32_info`, correctly rounded in one step. This is MANDATED, not
  validate-then-fix, and is guarded by the Paxson f32 vectors (see Testing) — the
  sharpest correctness risk in the module.

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

/* Worst-case byte counts. These constants are honest ONLY for BR_FLOAT_SHORTEST,
   which is bounded by a small constant. The other float modes are NOT: a huge-
   magnitude DECIMAL needs sign + up to 309 integral digits + '.' + prec — for
   those, size with the arithmetic bound function below. */
#define BR_FORMAT_I64_MAX          20   /* -9223372036854775808 */
#define BR_FORMAT_U64_MAX          20
#define BR_FORMAT_F64_SHORTEST_MAX 24
#define BR_FORMAT_F32_SHORTEST_MAX 16

/* Worst-case bytes for ANY (fmt, prec) — the honest bound for the non-shortest modes. */
size_t br_format_f64_bound(br_float_format fmt, int prec);
size_t br_format_f32_bound(br_float_format fmt, int prec);
```

- `prec` is IGNORED for `BR_FLOAT_SHORTEST` (it picks its own digit count) and is
  the fractional/significant count for the other modes (matching Go's
  FormatFloat prec; Bedrock uses the enum where Go/Odin use a fmt byte +
  prec=-1 sentinel).
- Undersized `dst` (including `dst == NULL` or `dst_cap == 0`) →
  `BR_STATUS_SHORT_BUFFER`, `count = 0`. NEVER a truncated number — a half-
  written "3." is worse than an error.
- The `_SHORTEST_MAX` macros bound ONLY `BR_FLOAT_SHORTEST`; for any other mode
  callers size with `br_format_f64_bound(fmt, prec)`.

## Shared internal engine

One fixed-buffer multiprecision-decimal path (a `[384]`-byte digit buffer,
`Float_Info {mantbits, expbits, bias}` selecting f32/f64), ported from Odin's
`decimal` subpackage (itself a port of Go's legacy decimal). No bignum, no heap,
no multi-KB tables. Both parse (`decimal_to_float_bits`) and format
(`generic_ftoa`) select the `Float_Info` by type; the public `_f32`/`_f64` pairs
are typed entry points over this one engine (C has no generics, and typed funcs
beat a runtime `bit_size` arg a caller can pass wrong).

## Strings builder integration (dogfood consumer)

The blocked checklist rows. The builder OWNS an allocator, so it does NOT stack-
buffer — it reserves the worst-case bound, formats DIRECTLY into its tail, and
advances by the written count (zero copy, no fixed cap, and it makes
`br_format_*_bound` load-bearing dogfood):

```c
br_string_builder_io_result br_string_builder_write_int(br_string_builder *b, int64_t value, int base);
br_string_builder_io_result br_string_builder_write_uint(br_string_builder *b, uint64_t value, int base);
br_string_builder_io_result br_string_builder_write_f64(br_string_builder *b, double value,
                                                        br_float_format fmt, int prec);
br_string_builder_io_result br_string_builder_write_f32(br_string_builder *b, float value,
                                                        br_float_format fmt, int prec);
```

Both float widths get their own entry point: promoting an f32 to double and
formatting f64-SHORTEST prints the double's digits (`0.1f` ->
"0.10000000149011612", not "0.1"), so `write_f32` must run the f32 format path.
Per call: `n = br_format_*_bound(...)`; `br_string_builder_reserve(b, n)`; format
into the reserved tail; advance length by `result.count`.

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
- No underscore digit separators in v1 — a DOUBLE deviation, recorded precisely:
  Odin accepts `_` in every digit loop (`1_000`, `0xdead_beef`); Go's rule is
  narrower and more principled — underscores are legal ONLY in base-0
  literal-style parsing, between digits, including ParseFloat, and never with an
  explicit base. Bedrock drops them entirely for v1. If a config-style consumer
  ever needs them, adopt Go's base-0-only rule as the designated future shape,
  NOT Odin's accept-everywhere.
- f32 parses natively at f32 precision, NOT via f64-then-narrow (double-rounding
  correctness). Odin's `parse_f32` narrows from `parse_f64`
  (`strconv.odin:748-751`) — a VERIFIED 1-ULP defect with a machine-checked
  witness; see `tracking/odin-suspected-bugs.md`.
- Typed `br_float_format` enum + `_f32`/`_f64` pairs replace Odin's fmt byte +
  int bit_size.
- Float format emits a sign only for negative values; Odin's `generic_ftoa`
  always emits a leading `+` for positives. Bedrock's `-`-only output matches
  Go/C and the parse round-trip contract (`format` output re-parses to the same
  value without a stray leading `+`).
- Locale independence stated as a property (deviation from C `strtod`, not Odin).
- No `Append*` family; quote/unquote deferred; f16 excluded.
- Algorithm: fixed-buffer decimal; Bedrock DECLINES Rust's Eisel-Lemire/Grisu3
  speed bar deliberately — exact shortest-round-trip at minimal code/data cost is
  the v1 goal; revisit only if profiled hot.

## Testing

Correctness suite = Go's Paxson testbase, VENDORED into `tests/data/` with a
BSD-3 attribution notice (`tests/data/LICENSE-go`) — self-contained per the
vendored-consumption model, no submodule-checkout dependency for CI. Tests
locate the directory via a `BR_TEST_DATA_DIR` macro passed by the Makefile
(keeps the C+make toolchain pure, no path guessing). Files (from Go's
`src/internal/strconv/testdata/` after its 2025 restructure):

- `testfp.txt` — 4 fields/line (type, format-verb, input, expected output);
  `#`/blank lines skipped. Drives `format(input) == output`.
- `atof1k.txt` — one decimal float per line; round-trip parse->format->parse.
- `ftoa1k.txt` — one C99 hexfloat (`%a` form, exact bits) per line; parse the
  exact value, format shortest, check the expected decimal.

Plus: ported hand-picked atof/ftoa cases; int round-trip across all bases with
overflow -> clean `OUT_OF_RANGE` (never wrap/UB); **the f32 double-rounding
vectors specifically**, including the verified witness
`1.00000017881393432617187499` -> `0x3f800001` (guards the native-f32
requirement); the locale-independence regression (set `LC_NUMERIC` to a
comma-radix locale if installed, assert `br_parse_f64("3.14") == 3.14` and
`br_format_f64` emits "3.14" not "3,14"; skip cleanly if the locale is absent);
ASan buffer-bound checks; differential fuzz vs a correctly-rounded oracle (musl
`strtod`).
