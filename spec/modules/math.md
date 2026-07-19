# Math

Bit-level integer operations ported from Odin `core/math/bits` (modeled on Go's
`math/bits`). The pilot wave ports the fixed-width bit helpers.

## Scope

| Package | Bedrock header | Impl | Status |
| --- | --- | --- | --- |
| math/bits | math/bits.h | src/math/bits.c | planned |

Module umbrella: include/bedrock/math.h. Byte-ORDER conversion (from_be/to_le)
is NOT here — it lives in encoding/endian (spec/modules/encoding.md). This
module provides only the endianness-agnostic `byteswap` primitive.

## Conventions

- Fixed-width only: u8/u16/u32/u64 (and signed where Odin has them). Odin's
  word-sized `uint`/`int` variants and its `U*_MIN/MAX` constants are dropped —
  C fixes widths via `<stdint.h>` (`UINT32_MAX`, etc.) already.
- Per-width named functions are the canonical ABI (`br_bits_clz_u32`). Optional
  `_Generic` sugar (`br_bits_clz(x)`) may layer on top per foundation rule 3.
- Count/scan operations are TOTAL: defined for 0 (see below). This is a
  deliberate improvement over the raw `__builtin_*`, whose zero input is UB.

## Zero-input contract (headline)

`br_bits_clz_uN(0)` returns N; `br_bits_ctz_uN(0)` returns N; `br_bits_popcount`
and `br_bits_count_zeros` are naturally defined. Implementations MUST guard the
builtin tier because `__builtin_clz(0)`/`ctz(0)` are undefined behavior and
silent. This matches C23 `<stdbit.h>` semantics. (Odin's `log2(0)` returns a silent
`max(T)` sentinel; Bedrock drops standalone log2 entirely — see the API notes.)

## Three-tier intrinsic strategy (approved)

For clz / ctz / popcount / byteswap:

1. C23 `<stdbit.h>` when `__STDC_VERSION__ >= 202311L && __has_include(<stdbit.h>)`
   (`stdc_leading_zeros`, `stdc_trailing_zeros`, `stdc_count_ones`).
2. Compiler builtins: gcc/clang `__builtin_clz/clzll/ctz/ctzll/popcount/popcountll`
   and `__builtin_bswap16/32/64`; MSVC `_BitScanReverse/Forward[64]`,
   `__popcnt[64]`, `_byteswap_ushort/ulong/uint64`. For u8/u16, promote to
   32-bit and correct the width offset for clz; guard x==0 first in all tiers.
3. Portable fallback: de Bruijn multiply for ctz/clz, the 256-entry length
   table for `len`, parallel/Kernighan popcount, shift-based bswap.

Tiers 2/3 must be differential-testable: a build knob forces the fallback so
tests compare it against the builtin tier exhaustively for u8/u16 and over
random u32/u64 sweeps.

## Error Model

Only division can fail. `br_bits_div_uN` returns a result struct and NEVER
panics or asserts (foundation forbids it), smoothing Odin's inconsistency
(u32 `assert` vs u64 `panic`):

```c
typedef struct br_bits_div_u32_result { uint32_t quo, rem; br_status status; } br_bits_div_u32_result;
typedef struct br_bits_div_u64_result { uint64_t quo, rem; br_status status; } br_bits_div_u64_result;
```

`status = BR_STATUS_INVALID_ARGUMENT` when `y == 0` (divide error) or `y <= hi`
(quotient overflow); quo/rem are 0 on error.

## API (per width; u32 shown, u8/u16/u64 parallel unless noted)

```c
/* Counts / scans — TOTAL (defined at 0). */
int br_bits_clz_u32(uint32_t x);          /* leading zeros; 32 if x==0 */
int br_bits_ctz_u32(uint32_t x);          /* trailing zeros; 32 if x==0 */
int br_bits_popcount_u32(uint32_t x);     /* set bits */
int br_bits_count_zeros_u32(uint32_t x);  /* width - popcount */
int br_bits_bit_width_u32(uint32_t x);    /* min bits to represent; 0 if x==0 (C23 stdc_bit_width) */
/* floor(log2 x) == br_bits_bit_width_uN(x) - 1 for x > 0. No standalone log2:
   Odin's log2(0)=max(T) sentinel is a footgun; bit_width is total and honest. */

/* Bit rearrangement. */
uint32_t br_bits_rotate_left_u32(uint32_t x, unsigned k);   /* k taken mod 32 */
uint32_t br_bits_rotate_right_u32(uint32_t x, unsigned k);
uint32_t br_bits_reverse_u32(uint32_t x);   /* reverse bit order */
uint16_t br_bits_byteswap_u16(uint16_t x);  /* also u32/u64; endianness-agnostic */

/* Power-of-two (fixed-width family; base.h keeps br_is_power_of_two_size). */
bool br_bits_is_power_of_two_u32(uint32_t x);   /* signed variants too: _i32 etc. */

/* Multi-word arithmetic. */
typedef struct br_bits_add_u32_result { uint32_t sum;  bool carry_out;  } br_bits_add_u32_result;
typedef struct br_bits_sub_u32_result { uint32_t diff; bool borrow_out; } br_bits_sub_u32_result;
typedef struct br_bits_mul_u32_result { uint32_t hi, lo; } br_bits_mul_u32_result; /* (hi,lo) as Odin/Go */
br_bits_add_u32_result br_bits_add_u32(uint32_t x, uint32_t y, bool carry_in);
br_bits_sub_u32_result br_bits_sub_u32(uint32_t x, uint32_t y, bool borrow_in);
br_bits_mul_u32_result br_bits_mul_u32(uint32_t x, uint32_t y);   /* u64 path may use __int128 when available */
br_bits_div_u32_result br_bits_div_u32(uint32_t hi, uint32_t lo, uint32_t y);

/* Bitfields (unsigned + signed, u8..u64 / i8..i64). */
uint32_t br_bits_field_extract_u32(uint32_t value, unsigned offset, unsigned bits);
int32_t  br_bits_field_extract_i32(int32_t value, unsigned offset, unsigned bits); /* sign-extends */
uint32_t br_bits_field_insert_u32(uint32_t base, uint32_t insert, unsigned offset, unsigned bits);
```

Bitfield contract: `offset + bits <= width` is the caller's responsibility;
out-of-range values are unchecked, like C shift operands.

Widths provided: clz/ctz/popcount/count_zeros/bit_width/rotate/reverse for
u8,u16,u32,u64; byteswap for u16,u32,u64; is_power_of_two for u8..u64 + i8..i64;
add/sub/mul/div for u32,u64; field_extract/insert for u8..u64 and i8..i64.

## Intentional deviations from Odin

- No u128/i128 anywhere in the pilot (portable C11 has no 128-bit); a future
  path may be gated on `__SIZEOF_INT128__`, matching the varint decision.
  `mul_u64` MAY use `__int128` internally when available with a portable
  dual-word fallback.
- `br_bits_mul_*` returns `(hi, lo)`, following Odin and Go's `bits.Mul`. Note
  for porters coming from Rust: `widening_mul` there returns `(low, high)` —
  the opposite order.
- No word-sized `uint`/`int` variants and no `U*_MIN/MAX` constants (use
  `<stdint.h>`).
- div returns `{quo, rem, status}` instead of panicking/asserting; the u32/u64
  error paths are unified.
- clz/ctz/popcount are total (defined at 0); Odin exposes the raw intrinsic.
- No standalone log2: Odin's `log2(0) = max(T)` silent sentinel is dropped as a
  footgun. Use total `bit_width` (0 -> 0); floor-log2 is `bit_width(x) - 1` for
  x > 0.
- Odin/Go's `len` is named `bit_width` (clearer in C; matches C23
  `stdc_bit_width` exactly, including the 0 -> 0 case).
- add/sub carry/borrow in/out are `bool` (Rust idiom), not integer 0/1
  (Odin/Go).
- Byte-order helpers (from_be/to_le) are NOT re-exposed here — see
  encoding/endian.

## Testing and fuzzing

Differential-test the portable fallback against the builtin tier: exhaustive
for u8/u16, random sweep for u32/u64, on every count/scan/reverse/byteswap.
Property tests: popcount + count_zeros == width;
rotate_left(rotate_right(x,k),k) == x; reverse(reverse(x)) == x;
bit_width(x) - 1 == index of highest set bit for x > 0; div reconstructs
(quo*y + rem == (hi:lo)) when status OK; div by 0 and overflow return
INVALID_ARGUMENT. Fuzz div/mul/add/sub for
wraparound correctness.
