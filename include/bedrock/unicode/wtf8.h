#ifndef BEDROCK_UNICODE_WTF8_H
#define BEDROCK_UNICODE_WTF8_H

#include <bedrock/bytes/bytes.h>
#include <bedrock/io/io.h>

BR_EXTERN_C_BEGIN

/*
WTF-8: UTF-8 generalized to admit lone (unpaired) UTF-16 surrogates as their
natural 3-byte sequences. This lets a Windows UTF-16 path -- which may contain
unpaired surrogates -- round-trip losslessly through a byte string.

WTF-8 data travels in ordinary br_bytes_view / br_bytes; there is no wrapper
type. The strict br_utf8_* codec is unaffected and admits no surrogates; WTF-8
is a separate surface that relaxes exactly the surrogate ban.
*/

/*
Native-endian UTF-16 code units (a Windows WCHAR sequence). Reading
foreign-endian UTF-16 is the caller's concern; this module is a pure codec.
*/
typedef struct br_wtf16_view {
  const uint16_t *data;
  size_t len;
} br_wtf16_view;

static inline br_wtf16_view br_wtf16_view_make(const uint16_t *data, size_t len) {
  br_wtf16_view view;

  view.data = data;
  view.len = len;
  return view;
}

/*
Report whether `s` is well-formed WTF-8.

Accepts everything strict UTF-8 accepts, plus a lone surrogate (U+D800..U+DFFF,
unpaired) as a 3-byte sequence. Rejects, as strict UTF-8 does, truncated
sequences, overlong encodings, code points above U+10FFFF, and malformed
continuation bytes -- and additionally rejects an adjacent high-then-low
surrogate encoded as two 3-byte sequences (a real pair must be the single
4-byte astral form).
*/
bool br_wtf8_valid(br_bytes_view s);

/*
Number of WTF-8 bytes needed to encode `s`. Pair the two-pass sizer with the
transcode below.
*/
size_t br_wtf8_from_wtf16_len(br_wtf16_view s);

/*
Transcode `s` into WTF-8 at `dst`. Returns the number of bytes written. If
`dst_cap` is smaller than `br_wtf8_from_wtf16_len(s)`, returns
`BR_STATUS_SHORT_BUFFER` with count 0 and writes nothing (never truncates).
*/
br_io_result br_wtf8_from_wtf16(br_wtf16_view s, uint8_t *dst, size_t dst_cap);

/*
Number of UTF-16 code units needed to encode WTF-8 `s`.
*/
size_t br_wtf16_from_wtf8_len(br_bytes_view s);

/*
Transcode WTF-8 `s` into UTF-16 code units at `dst`. Returns the number of code
units written. If `dst_cap` is smaller than `br_wtf16_from_wtf8_len(s)`, returns
`BR_STATUS_SHORT_BUFFER` with count 0 and writes nothing (never truncates).
*/
br_io_result br_wtf16_from_wtf8(br_bytes_view s, uint16_t *dst, size_t dst_cap);

/*
Concatenate two WTF-8 strings into an owned buffer, keeping the result
well-formed.

If `a` ends in a lone high surrogate and `b` begins with a lone low surrogate,
the seam halves re-encode as one 4-byte astral scalar (rather than two 3-byte
lone surrogates, which would be ill-formed WTF-8). Otherwise this is a plain
byte concatenation. The returned value is owned by the caller and freed with
`br_bytes_free`; a failed allocation frees any partial buffer.
*/
br_bytes_result br_wtf8_concat(br_bytes_view a, br_bytes_view b, br_allocator allocator);

BR_EXTERN_C_END

#endif
