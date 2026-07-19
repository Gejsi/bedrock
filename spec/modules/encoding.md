# Encoding

Odin-inspired byte<->text and integer<->bytes conversions. The pilot wave ports
four Odin packages: `core/encoding/hex`, `core/encoding/base64`,
`core/encoding/endian`, `core/encoding/varint`.

## Scope

| Package | Bedrock header | Impl | Status |
| --- | --- | --- | --- |
| hex | encoding/hex.h | src/encoding/hex.c | planned |
| base64 | encoding/base64.h | src/encoding/base64.c | planned |
| endian | encoding/endian.h | (header-only) | planned |
| varint (LEB128) | encoding/varint.h | src/encoding/varint.c | planned |

Base32, CBOR, JSON, ASN.1, PEM, UUID, XML, CSV/INI, entity, hxa are out of
pilot scope.

## Conventions

- Signatures are spelled in standard C types, matching the public-header rule
  in `spec/foundation.md` (Scalar Types In The Public ABI).
- Encoded/decoded input is passed as `br_bytes_view`; owned output is `br_bytes`
  freed with `br_bytes_free`. String callers wrap literals with `BR_BYTES_LIT`
  or C strings with `br_bytes_view_from_cstr`.
- `br_allocator` is passed by value, matching the `bytes`/`strings`/`mem` code.
- Every conversion offers a length helper plus, where sensible, three forms:
  allocating (returns owned bytes), into-buffer (`uint8_t *dst, size_t dst_cap`,
  returns bytes written), and to-stream (`br_writer`/`br_reader`).
- Encoders cannot fail on content; only allocation (`OUT_OF_MEMORY`) or a short
  caller buffer (`SHORT_BUFFER`) can fail an encode. Decoders can additionally
  fail on malformed input.

## Error Model

`br_status` gains `BR_STATUS_INVALID_ENCODING` (appended to the end of the
enum so existing values do not shift). It means "the encoded input data is
malformed" — distinct from `BR_STATUS_INVALID_ARGUMENT`, which stays reserved
for caller misuse (a programmer bug, not corrupt data). The whole upcoming
parser family (csv, ini, rfc3339) shares this status. Decode paths use:

| Condition | Status |
| --- | --- |
| bad alphabet byte / structural violation / varint overflow | `BR_STATUS_INVALID_ENCODING` |
| input ended mid-unit (truncated varint, empty varint buffer) | `BR_STATUS_UNEXPECTED_EOF` |
| caller into-buffer too small | `BR_STATUS_SHORT_BUFFER` |
| NULL required pointer / caller misuse | `BR_STATUS_INVALID_ARGUMENT` |
| allocator failure (allocating variants) | `BR_STATUS_OUT_OF_MEMORY` |

Decode results carry `size_t error_offset`: the index of the first offending
input byte when `status == BR_STATUS_INVALID_ENCODING`; the count of bytes
consumed before input ran out when `UNEXPECTED_EOF`; `0` and meaningless
otherwise. This mirrors Go's `CorruptInputError`.

### Shared result types (encoding/encoding.h)

```c
typedef struct br_decode_result {   /* allocating decode */
  br_bytes value;         /* owned; free with br_bytes_free */
  size_t error_offset;
  br_status status;
} br_decode_result;

typedef struct br_decode_into_result {  /* into-buffer or to-stream decode */
  size_t count;           /* bytes written to dst / writer */
  size_t error_offset;
  br_status status;
} br_decode_into_result;
```

## hex

Lowercase/uppercase base-16. Fixes Odin's decode-leak wart (see
`tracking/odin-suspected-bugs.md`): the allocating decode frees its buffer
before returning any failure. Keeps Odin's single-byte `decode_sequence`.

```c
typedef enum br_hex_case { BR_HEX_LOWER = 0, BR_HEX_UPPER } br_hex_case;

static inline size_t br_hex_encoded_len(size_t src_len);      /* src_len * 2 */
static inline size_t br_hex_decoded_len(size_t encoded_len);  /* encoded_len / 2 */

br_bytes_result br_hex_encode(br_bytes_view src, br_hex_case letter_case,
                              br_allocator allocator);
br_io_result    br_hex_encode_into(br_bytes_view src, br_hex_case letter_case,
                                   uint8_t *dst, size_t dst_cap);
br_io_result    br_hex_encode_to_writer(br_bytes_view src, br_hex_case letter_case,
                                        br_writer w);

br_decode_result      br_hex_decode(br_bytes_view src, br_allocator allocator);
br_decode_into_result br_hex_decode_into(br_bytes_view src, uint8_t *dst, size_t dst_cap);

/* Decode one byte from "23" or "0x23"/"0X23"; value+status via br_io_byte_result. */
br_io_byte_result br_hex_decode_sequence(br_bytes_view seq);
```

Semantics: odd-length input -> `INVALID_ENCODING`, `error_offset = src.len - 1`
(length parity is a structural property). Any non-`[0-9a-fA-F]` byte ->
`INVALID_ENCODING` at its index. Decode is case-insensitive; encode picks case
via `letter_case`.

## base64

Table-driven like Odin, but closes Odin's lenient-only gap with explicit
presets and padded/strict controls (matching Go's Std/URL/RawStd/RawURL +
Strict). Also drops Odin's dead `dst` decode parameter (see suspected-bugs).

```c
typedef enum br_base64_alphabet { BR_BASE64_STD = 0, BR_BASE64_URL } br_base64_alphabet;

typedef struct br_base64_encoding {
  br_base64_alphabet alphabet;  /* '+','/' vs '-','_' */
  bool padded;                  /* encode emits '='; decode requires exact padding */
  bool strict;                  /* decode rejects non-zero trailing bits */
} br_base64_encoding;

static inline br_base64_encoding br_base64_std(void);      /* {STD, padded, !strict} */
static inline br_base64_encoding br_base64_url(void);      /* {URL, padded, !strict} */
static inline br_base64_encoding br_base64_raw_std(void);  /* {STD, !padded, !strict} */
static inline br_base64_encoding br_base64_raw_url(void);  /* {URL, !padded, !strict} */

size_t br_base64_encoded_len(br_base64_encoding enc, size_t src_len);
size_t br_base64_decoded_len(br_base64_encoding enc, br_bytes_view src); /* exact; 0 if length impossible */

br_bytes_result br_base64_encode(br_base64_encoding enc, br_bytes_view src,
                                 br_allocator allocator);
br_io_result    br_base64_encode_into(br_base64_encoding enc, br_bytes_view src,
                                      uint8_t *dst, size_t dst_cap);
br_io_result    br_base64_encode_to_writer(br_base64_encoding enc, br_bytes_view src,
                                           br_writer w);

br_decode_result      br_base64_decode(br_base64_encoding enc, br_bytes_view src,
                                       br_allocator allocator);
br_decode_into_result br_base64_decode_into(br_base64_encoding enc, br_bytes_view src,
                                            uint8_t *dst, size_t dst_cap);
br_decode_into_result br_base64_decode_to_writer(br_base64_encoding enc, br_bytes_view src,
                                                 br_writer w);
```

Semantics:

- `padded=true`: encode pads to a multiple of 4; decode requires
  `length % 4 == 0` and correct trailing `=`. `padded=false` (raw): encode
  emits no padding; decode requires `length % 4 != 1` and forbids `=`.
- Any character outside the active alphabet -> `INVALID_ENCODING` at its index.
- `strict=true` additionally rejects a final quantum whose unused bits are
  non-zero (canonical-encoding / malleability check, exactly Go's Strict).
  `strict=false` masks those bits and accepts, matching Odin's lenient decoder.
- Conformance: RFC 4648 vectors ("", "f", "fo", "foo", "foob", "fooba",
  "foobar").

## endian

Header-only, zero-alloc, static inline. Fixed-width integer and float get/put
with explicit byte order. Integer paths assemble/disassemble with shifts, so
they are host-endianness-agnostic and free of aliasing concerns. Float paths
bit-cast through `memcpy` (strict-aliasing clean). No `f16` in v1 (C11 has no
`_Float16`); deferred behind a future feature gate.

Names carry the `br_endian_` package prefix for consistency with the rest of
the encoding family (and to stay clear of the io module's `br_read`/`br_write`
verb space).

```c
typedef enum br_byte_order { BR_BYTE_ORDER_LITTLE = 0, BR_BYTE_ORDER_BIG } br_byte_order;
static inline br_byte_order br_byte_order_native(void);

/* Bounds-checked over a view; return false if too few bytes. */
bool br_endian_get_u16(br_bytes_view b, br_byte_order order, uint16_t *out);
bool br_endian_get_u32(br_bytes_view b, br_byte_order order, uint32_t *out);
bool br_endian_get_u64(br_bytes_view b, br_byte_order order, uint64_t *out);
bool br_endian_get_i16(br_bytes_view b, br_byte_order order, int16_t *out);
bool br_endian_get_i32(br_bytes_view b, br_byte_order order, int32_t *out);
bool br_endian_get_i64(br_bytes_view b, br_byte_order order, int64_t *out);
bool br_endian_get_f32(br_bytes_view b, br_byte_order order, float *out);
bool br_endian_get_f64(br_bytes_view b, br_byte_order order, double *out);

/* Bounds-checked; return false if dst too small. */
bool br_endian_put_u16(br_bytes dst, br_byte_order order, uint16_t v);
/* ... u32, u64, i16, i32, i64, f32, f64 */

/* Unchecked fast paths; caller guarantees >= N bytes. */
uint16_t br_endian_get_u16_unchecked(const uint8_t *p, br_byte_order order);
void     br_endian_put_u16_unchecked(uint8_t *p, br_byte_order order, uint16_t v);
/* ... all widths */
```

All eight widths get checked get/put and unchecked get/put (abbreviated above).

## varint (LEB128)

Unsigned/signed LEB128. Default ABI is `uint64_t`/`int64_t` (Go's
`encoding/binary` width; portable C11; max 10 bytes for 64 bits). Signed uses
DWARF sign-extended LEB128, exactly like Odin — NOT Go's zig-zag.

```c
#define BR_LEB128_MAX_BYTES 10  /* ceil(64/7) */

typedef struct br_uleb128_result { uint64_t value; size_t size; br_status status; } br_uleb128_result;
typedef struct br_ileb128_result { int64_t value; size_t size; br_status status; } br_ileb128_result;

size_t br_uleb128_encoded_len(uint64_t value);
size_t br_ileb128_encoded_len(int64_t value);

br_io_result br_uleb128_encode(uint64_t value, uint8_t *dst, size_t dst_cap);
br_io_result br_ileb128_encode(int64_t value, uint8_t *dst, size_t dst_cap);

br_uleb128_result br_uleb128_decode(br_bytes_view src); /* size = bytes consumed */
br_ileb128_result br_ileb128_decode(br_bytes_view src);
```

Semantics: `size` is bytes consumed regardless of status (on OK, the varint
length; on error, how far it got). Empty/truncated buffer (continuation bit
set, input ends) -> `UNEXPECTED_EOF`. Value exceeding the target width (an
11th byte, or a 10th byte with bits beyond bit 63) -> `INVALID_ENCODING`.
Stream variants (`br_uleb128_read(br_reader)` / `br_uleb128_write(br_writer, uint64_t)`)
are a recommended follow-on, deferred out of the pilot.

## Intentional deviations from Odin

- varint width is `uint64_t`/`int64_t`, not Odin's `u128`/`i128`; max 10 bytes
  not 19. A future 128-bit path may be gated on `__SIZEOF_INT128__`.
- hex allocating decode frees on failure (Odin leaks — suspected-bugs.md).
- base64 gains explicit std/url + padded/strict presets and drops Odin's dead
  decode `dst` param (suspected-bugs.md); Odin exposes only a lenient decoder.
- base64/hex encoded output is `br_bytes`, not Odin's `string`/`[]byte` split.
- endian is host-endianness-agnostic (shift-based) rather than Odin's
  unaligned-load + conditional byteswap; no `f16` in v1.
- All decoders report a byte error offset (Odin returns only a bool/enum).

## Testing and fuzzing

Round-trip identity for every codec. RFC 4648 vectors for base64; DWARF LEB128
examples for varint; known hex pairs. Recommended fuzz targets:

- hex.decode: odd lengths, non-hex bytes, `0x` prefixes; assert ASan-clean on
  every failure path (guards the leak fix); round-trip.
- base64.decode: all 4 presets x strict/lenient; bad padding, wrong-alphabet
  chars, non-zero trailing bits (strict rejects / lenient accepts), truncated
  tails; round-trip.
- varint.decode: truncated, overflow (11+ bytes / oversized 10th byte), empty;
  round-trip over random values incl. 0, -1, INT64_MIN/MAX and 7-bit-shift
  boundaries.
- endian: round-trip get/put over random values, all widths/orders; short
  buffers must return false without reading out of bounds.
