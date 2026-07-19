#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <bedrock/encoding/endian.h>

/*
Exercises get/put on a heap buffer sized to EXACTLY the value's width, so a
negative-path over-read (bounds check skipped) would land in an ASan red zone.
`slot` and `slot_len` describe the writable region; the bounds tests below hand
in deliberately short slots.
*/

static void test_native_order(void) {
  uint8_t bytes[2];
  br_byte_order native = br_byte_order_native();

  assert(native == BR_BYTE_ORDER_LITTLE || native == BR_BYTE_ORDER_BIG);

  /* Writing 0x0102 in native order must match the platform's in-memory layout. */
  br_endian_put_u16_unchecked(bytes, native, 0x0102u);
  if (native == BR_BYTE_ORDER_LITTLE) {
    assert(bytes[0] == 0x02u && bytes[1] == 0x01u);
  } else {
    assert(bytes[0] == 0x01u && bytes[1] == 0x02u);
  }
}

static void test_known_layout(void) {
  uint8_t buf[8];
  br_bytes dst;
  uint16_t got16;
  uint32_t got32;
  uint64_t got64;
  br_bytes_view view;

  /* u16 */
  dst = br_bytes_make(buf, 2u);
  assert(br_endian_put_u16(dst, BR_BYTE_ORDER_LITTLE, 0x0102u));
  assert(buf[0] == 0x02u && buf[1] == 0x01u);
  assert(br_endian_put_u16(dst, BR_BYTE_ORDER_BIG, 0x0102u));
  assert(buf[0] == 0x01u && buf[1] == 0x02u);

  /* u32 */
  dst = br_bytes_make(buf, 4u);
  assert(br_endian_put_u32(dst, BR_BYTE_ORDER_LITTLE, 0x01020304u));
  assert(buf[0] == 0x04u && buf[1] == 0x03u && buf[2] == 0x02u && buf[3] == 0x01u);
  assert(br_endian_put_u32(dst, BR_BYTE_ORDER_BIG, 0x01020304u));
  assert(buf[0] == 0x01u && buf[1] == 0x02u && buf[2] == 0x03u && buf[3] == 0x04u);

  /* u64 */
  dst = br_bytes_make(buf, 8u);
  assert(br_endian_put_u64(dst, BR_BYTE_ORDER_LITTLE, 0x0102030405060708uLL));
  assert(buf[0] == 0x08u && buf[7] == 0x01u);
  assert(br_endian_put_u64(dst, BR_BYTE_ORDER_BIG, 0x0102030405060708uLL));
  assert(buf[0] == 0x01u && buf[7] == 0x08u);

  /* Reading back a hand-laid big-endian buffer confirms get orientation. */
  buf[0] = 0xDEu;
  buf[1] = 0xADu;
  buf[2] = 0xBEu;
  buf[3] = 0xEFu;
  view = br_bytes_view_make(buf, 4u);
  assert(br_endian_get_u32(view, BR_BYTE_ORDER_BIG, &got32));
  assert(got32 == 0xDEADBEEFu);
  assert(br_endian_get_u32(view, BR_BYTE_ORDER_LITTLE, &got32));
  assert(got32 == 0xEFBEADDEu);

  view = br_bytes_view_make(buf, 2u);
  assert(br_endian_get_u16(view, BR_BYTE_ORDER_BIG, &got16));
  assert(got16 == 0xDEADu);

  buf[0] = 0x01u;
  buf[1] = 0x00u;
  buf[2] = 0x00u;
  buf[3] = 0x00u;
  buf[4] = 0x00u;
  buf[5] = 0x00u;
  buf[6] = 0x00u;
  buf[7] = 0x00u;
  view = br_bytes_view_make(buf, 8u);
  assert(br_endian_get_u64(view, BR_BYTE_ORDER_LITTLE, &got64));
  assert(got64 == 1uLL);
}

static void test_roundtrip_unsigned(void) {
  static const uint16_t u16s[] = {0u, 1u, 0x00FFu, 0x7FFFu, 0x8000u, 0xFFFFu, 0x1234u};
  static const uint32_t u32s[] = {
    0u, 1u, 0x0000FFFFu, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, 0x12345678u};
  static const uint64_t u64s[] = {0uLL,
                                  1uLL,
                                  0x00000000FFFFFFFFuLL,
                                  0x7FFFFFFFFFFFFFFFuLL,
                                  0x8000000000000000uLL,
                                  0xFFFFFFFFFFFFFFFFuLL,
                                  0x0123456789ABCDEFuLL};
  uint8_t buf[8];
  size_t i;
  int o;

  for (o = 0; o < 2; ++o) {
    br_byte_order order = o == 0 ? BR_BYTE_ORDER_LITTLE : BR_BYTE_ORDER_BIG;

    for (i = 0u; i < sizeof(u16s) / sizeof(u16s[0]); ++i) {
      uint16_t got;
      assert(br_endian_put_u16(br_bytes_make(buf, 2u), order, u16s[i]));
      assert(br_endian_get_u16(br_bytes_view_make(buf, 2u), order, &got));
      assert(got == u16s[i]);
      assert(br_endian_get_u16_unchecked(buf, order) == u16s[i]);
    }
    for (i = 0u; i < sizeof(u32s) / sizeof(u32s[0]); ++i) {
      uint32_t got;
      assert(br_endian_put_u32(br_bytes_make(buf, 4u), order, u32s[i]));
      assert(br_endian_get_u32(br_bytes_view_make(buf, 4u), order, &got));
      assert(got == u32s[i]);
      assert(br_endian_get_u32_unchecked(buf, order) == u32s[i]);
    }
    for (i = 0u; i < sizeof(u64s) / sizeof(u64s[0]); ++i) {
      uint64_t got;
      assert(br_endian_put_u64(br_bytes_make(buf, 8u), order, u64s[i]));
      assert(br_endian_get_u64(br_bytes_view_make(buf, 8u), order, &got));
      assert(got == u64s[i]);
      assert(br_endian_get_u64_unchecked(buf, order) == u64s[i]);
    }
  }
}

static void test_roundtrip_signed(void) {
  static const int16_t i16s[] = {0, 1, -1, 0x7FFF, (int16_t)0x8000, 123, -123};
  static const int32_t i32s[] = {0, 1, -1, 0x7FFFFFFF, (int32_t)0x80000000, 123456, -123456};
  static const int64_t i64s[] = {
    0, 1, -1, 0x7FFFFFFFFFFFFFFFLL, (int64_t)0x8000000000000000uLL, 1234567890123LL};
  uint8_t buf[8];
  size_t i;
  int o;

  for (o = 0; o < 2; ++o) {
    br_byte_order order = o == 0 ? BR_BYTE_ORDER_LITTLE : BR_BYTE_ORDER_BIG;

    for (i = 0u; i < sizeof(i16s) / sizeof(i16s[0]); ++i) {
      int16_t got;
      assert(br_endian_put_i16(br_bytes_make(buf, 2u), order, i16s[i]));
      assert(br_endian_get_i16(br_bytes_view_make(buf, 2u), order, &got));
      assert(got == i16s[i]);
      assert(br_endian_get_i16_unchecked(buf, order) == i16s[i]);
    }
    for (i = 0u; i < sizeof(i32s) / sizeof(i32s[0]); ++i) {
      int32_t got;
      assert(br_endian_put_i32(br_bytes_make(buf, 4u), order, i32s[i]));
      assert(br_endian_get_i32(br_bytes_view_make(buf, 4u), order, &got));
      assert(got == i32s[i]);
      assert(br_endian_get_i32_unchecked(buf, order) == i32s[i]);
    }
    for (i = 0u; i < sizeof(i64s) / sizeof(i64s[0]); ++i) {
      int64_t got;
      assert(br_endian_put_i64(br_bytes_make(buf, 8u), order, i64s[i]));
      assert(br_endian_get_i64(br_bytes_view_make(buf, 8u), order, &got));
      assert(got == i64s[i]);
      assert(br_endian_get_i64_unchecked(buf, order) == i64s[i]);
    }
  }
}

static void test_roundtrip_float(void) {
  static const float f32s[] = {0.0f, -0.0f, 1.0f, -1.0f, 3.14159f, 1e30f, 1.4e-45f};
  static const double f64s[] = {0.0, -0.0, 1.0, -1.0, 3.141592653589793, 1e300, 5e-324};
  uint8_t buf[8];
  size_t i;
  int o;

  for (o = 0; o < 2; ++o) {
    br_byte_order order = o == 0 ? BR_BYTE_ORDER_LITTLE : BR_BYTE_ORDER_BIG;

    for (i = 0u; i < sizeof(f32s) / sizeof(f32s[0]); ++i) {
      float got;
      assert(br_endian_put_f32(br_bytes_make(buf, 4u), order, f32s[i]));
      assert(br_endian_get_f32(br_bytes_view_make(buf, 4u), order, &got));
      /* Compare bit patterns, not values, so -0.0 stays distinct from 0.0. */
      assert(memcmp(&got, &f32s[i], sizeof(got)) == 0);
    }
    for (i = 0u; i < sizeof(f64s) / sizeof(f64s[0]); ++i) {
      double got;
      assert(br_endian_put_f64(br_bytes_make(buf, 8u), order, f64s[i]));
      assert(br_endian_get_f64(br_bytes_view_make(buf, 8u), order, &got));
      assert(memcmp(&got, &f64s[i], sizeof(got)) == 0);
    }
  }
}

static void test_roundtrip_float_nonfinite(void) {
  double inf = 1e308 * 10.0;
  double nan = inf - inf;
  float finf = (float)inf;
  float fnan = (float)nan;
  uint8_t buf[8];
  float gf;
  double gd;

  assert(br_endian_put_f32(br_bytes_make(buf, 4u), BR_BYTE_ORDER_LITTLE, finf));
  assert(br_endian_get_f32(br_bytes_view_make(buf, 4u), BR_BYTE_ORDER_LITTLE, &gf));
  assert(memcmp(&gf, &finf, sizeof(gf)) == 0);

  assert(br_endian_put_f32(br_bytes_make(buf, 4u), BR_BYTE_ORDER_BIG, fnan));
  assert(br_endian_get_f32(br_bytes_view_make(buf, 4u), BR_BYTE_ORDER_BIG, &gf));
  assert(memcmp(&gf, &fnan, sizeof(gf)) == 0);

  assert(br_endian_put_f64(br_bytes_make(buf, 8u), BR_BYTE_ORDER_LITTLE, inf));
  assert(br_endian_get_f64(br_bytes_view_make(buf, 8u), BR_BYTE_ORDER_LITTLE, &gd));
  assert(memcmp(&gd, &inf, sizeof(gd)) == 0);

  assert(br_endian_put_f64(br_bytes_make(buf, 8u), BR_BYTE_ORDER_BIG, nan));
  assert(br_endian_get_f64(br_bytes_view_make(buf, 8u), BR_BYTE_ORDER_BIG, &gd));
  assert(memcmp(&gd, &nan, sizeof(gd)) == 0);
}

/*
Bounds rejection. Each short buffer is a heap allocation sized one byte below
the width, so if a get/put wrongly skipped its length check the access would
run into an ASan red zone. The value outputs must be left untouched on failure.
*/
static void test_bounds_reject(void) {
  int o;

  for (o = 0; o < 2; ++o) {
    br_byte_order order = o == 0 ? BR_BYTE_ORDER_LITTLE : BR_BYTE_ORDER_BIG;
    uint8_t *b1 = (uint8_t *)malloc(1u);
    uint8_t *b3 = (uint8_t *)malloc(3u);
    uint8_t *b7 = (uint8_t *)malloc(7u);
    uint16_t g16 = 0xAAAAu;
    uint32_t g32 = 0xAAAAAAAAu;
    uint64_t g64 = 0xAAAAAAAAAAAAAAAAuLL;

    assert(b1 != NULL && b3 != NULL && b7 != NULL);

    assert(!br_endian_get_u16(br_bytes_view_make(b1, 1u), order, &g16));
    assert(g16 == 0xAAAAu);
    assert(!br_endian_get_u32(br_bytes_view_make(b3, 3u), order, &g32));
    assert(g32 == 0xAAAAAAAAu);
    assert(!br_endian_get_u64(br_bytes_view_make(b7, 7u), order, &g64));
    assert(g64 == 0xAAAAAAAAAAAAAAAAuLL);

    assert(!br_endian_put_u16(br_bytes_make(b1, 1u), order, 0x1122u));
    assert(!br_endian_put_u32(br_bytes_make(b3, 3u), order, 0x11223344u));
    assert(!br_endian_put_u64(br_bytes_make(b7, 7u), order, 0x1122334455667788uLL));

    /* Signed and float wrappers must reject on short buffers too. */
    {
      int16_t s16 = 0x5555;
      float f = 1.0f;
      double d = 1.0;
      assert(!br_endian_get_i16(br_bytes_view_make(b1, 1u), order, &s16));
      assert(s16 == 0x5555);
      assert(!br_endian_get_f32(br_bytes_view_make(b3, 3u), order, &f));
      assert(f == 1.0f);
      assert(!br_endian_get_f64(br_bytes_view_make(b7, 7u), order, &d));
      assert(d == 1.0);
      assert(!br_endian_put_f32(br_bytes_make(b3, 3u), order, 2.0f));
      assert(!br_endian_put_f64(br_bytes_make(b7, 7u), order, 2.0));
    }

    free(b1);
    free(b3);
    free(b7);
  }
}

static void test_null_and_empty(void) {
  uint8_t buf[8];
  uint32_t g32 = 7u;

  /* NULL out pointer is caller misuse -> false, no crash. */
  assert(!br_endian_get_u32(br_bytes_view_make(buf, 4u), BR_BYTE_ORDER_LITTLE, NULL));
  /* NULL data view -> false. */
  assert(!br_endian_get_u32(br_bytes_view_make(NULL, 4u), BR_BYTE_ORDER_LITTLE, &g32));
  assert(g32 == 7u);
  /* NULL destination -> false. */
  assert(!br_endian_put_u32(br_bytes_make(NULL, 4u), BR_BYTE_ORDER_LITTLE, 1u));
}

/*
Misaligned access. A single heap block is read/written at byte offset 1 for the
widest types, proving the shift-based paths make no alignment assumption (also
UBSan -fsanitize=alignment clean).
*/
static void test_misaligned(void) {
  uint8_t *block = (uint8_t *)malloc(16u);
  uint8_t *p = NULL;
  uint32_t g32;
  uint64_t g64;
  float gf;
  double gd;

  assert(block != NULL);
  p = block + 1u; /* deliberately odd address */

  assert(br_endian_put_u32(br_bytes_make(p, 4u), BR_BYTE_ORDER_BIG, 0xCAFEBABEu));
  assert(br_endian_get_u32(br_bytes_view_make(p, 4u), BR_BYTE_ORDER_BIG, &g32));
  assert(g32 == 0xCAFEBABEu);

  assert(br_endian_put_u64(br_bytes_make(p, 8u), BR_BYTE_ORDER_LITTLE, 0x0102030405060708uLL));
  assert(br_endian_get_u64(br_bytes_view_make(p, 8u), BR_BYTE_ORDER_LITTLE, &g64));
  assert(g64 == 0x0102030405060708uLL);

  assert(br_endian_put_f32(br_bytes_make(p, 4u), BR_BYTE_ORDER_LITTLE, -2.5f));
  assert(br_endian_get_f32(br_bytes_view_make(p, 4u), BR_BYTE_ORDER_LITTLE, &gf));
  assert(gf == -2.5f);

  br_endian_put_f64_unchecked(p, BR_BYTE_ORDER_BIG, 1234.5);
  gd = br_endian_get_f64_unchecked(p, BR_BYTE_ORDER_BIG);
  assert(gd == 1234.5);

  free(block);
}

int main(void) {
  test_native_order();
  test_known_layout();
  test_roundtrip_unsigned();
  test_roundtrip_signed();
  test_roundtrip_float();
  test_roundtrip_float_nonfinite();
  test_bounds_reject();
  test_null_and_empty();
  test_misaligned();
  return 0;
}
