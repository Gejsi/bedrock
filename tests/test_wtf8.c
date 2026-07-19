#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <bedrock.h>

/* Round-trip a u16 sequence WTF-16 -> WTF-8 -> WTF-16 and assert bit-exact. */
static void assert_wtf16_round_trip(const u16 *units, usize len) {
  u8 wtf8[64];
  u16 back[32];
  br_io_result to8;
  br_io_result to16;
  usize need8 = br_wtf8_from_wtf16_len(br_wtf16_view_make(units, len));

  assert(need8 <= sizeof(wtf8));
  to8 = br_wtf8_from_wtf16(br_wtf16_view_make(units, len), wtf8, sizeof(wtf8));
  assert(to8.status == BR_STATUS_OK && to8.count == need8);

  /* The transcoded WTF-8 must itself be well-formed. */
  assert(br_wtf8_valid(br_bytes_view_make(wtf8, to8.count)));

  to16 = br_wtf16_from_wtf8(br_bytes_view_make(wtf8, to8.count), back, sizeof(back) / sizeof(u16));
  assert(to16.status == BR_STATUS_OK);
  assert(to16.count == len);
  assert(memcmp(back, units, len * sizeof(u16)) == 0);
}

static void test_lossless_round_trip(void) {
  /* THE property: every u16 sequence class survives bit-exact. */
  static const u16 pure_bmp[] = {'h', 'i', 0x00e9u, 0x4e2du}; /* h i é 中 */
  static const u16 astral[] = {0xd801u, 0xdc37u};             /* U+10437 as a pair */
  static const u16 lone_high[] = {'a', 0xd800u, 'b'};         /* unpaired high */
  static const u16 lone_low[] = {'a', 0xdc00u, 'b'};          /* unpaired low */
  static const u16 adjacent_unpaired[] = {0xdc00u, 0xd800u};  /* low THEN high: not a pair */
  static const u16 high_then_high[] = {0xd800u, 0xd801u};     /* two highs: neither pairs */
  static const u16 empty[] = {0};

  assert_wtf16_round_trip(pure_bmp, BR_ARRAY_COUNT(pure_bmp));
  assert_wtf16_round_trip(astral, BR_ARRAY_COUNT(astral));
  assert_wtf16_round_trip(lone_high, BR_ARRAY_COUNT(lone_high));
  assert_wtf16_round_trip(lone_low, BR_ARRAY_COUNT(lone_low));
  assert_wtf16_round_trip(adjacent_unpaired, BR_ARRAY_COUNT(adjacent_unpaired));
  assert_wtf16_round_trip(high_then_high, BR_ARRAY_COUNT(high_then_high));
  assert_wtf16_round_trip(empty, 0u);

  /* An astral pair transcodes to exactly 4 WTF-8 bytes (the U+10437 encoding). */
  {
    u8 buf[8];
    br_io_result r = br_wtf8_from_wtf16(br_wtf16_view_make(astral, 2u), buf, sizeof(buf));
    static const u8 expect[4] = {0xf0u, 0x90u, 0x90u, 0xb7u};
    assert(r.status == BR_STATUS_OK && r.count == 4u);
    assert(memcmp(buf, expect, 4u) == 0);
  }

  /* A lone high surrogate transcodes to its 3-byte WTF-8 form (ED A0 80). */
  {
    static const u16 lone[] = {0xd800u};
    u8 buf[8];
    br_io_result r = br_wtf8_from_wtf16(br_wtf16_view_make(lone, 1u), buf, sizeof(buf));
    static const u8 expect[3] = {0xedu, 0xa0u, 0x80u};
    assert(r.status == BR_STATUS_OK && r.count == 3u);
    assert(memcmp(buf, expect, 3u) == 0);
  }
}

static void test_well_formed_validity(void) {
  /* Lone surrogate (3-byte) is VALID WTF-8. */
  static const u8 lone_high[] = {0xedu, 0xa0u, 0x80u}; /* U+D800 */
  static const u8 lone_low[] = {0xedu, 0xbfu, 0xbfu};  /* U+DFFF */
  /* A high+low pair as TWO 3-byte sequences is ILL-FORMED (must be 4-byte). */
  static const u8 pair_as_halves[] = {0xedu, 0xa0u, 0x80u, 0xedu, 0xbfu, 0xbfu};
  /* The same pair as the 4-byte astral form is VALID. */
  static const u8 pair_as_astral[] = {0xf0u, 0x90u, 0x80u, 0x80u}; /* U+10000 */
  /* Two lone highs in a row: valid (neither completes a pair). */
  static const u8 two_highs[] = {0xedu, 0xa0u, 0x80u, 0xedu, 0xa0u, 0x81u};
  /* Strict-UTF-8 rejections still fire. */
  static const u8 truncated[] = {0xe2u, 0x82u};
  static const u8 overlong[] = {0xc0u, 0xafu};
  static const u8 above_max[] = {0xf4u, 0x90u, 0x80u, 0x80u}; /* U+110000 */
  static const u8 bad_cont[] = {0xe2u, 0x28u, 0xa1u};
  static const u8 lone_cont[] = {0x80u};

  assert(br_wtf8_valid(br_bytes_view_make(lone_high, sizeof(lone_high))));
  assert(br_wtf8_valid(br_bytes_view_make(lone_low, sizeof(lone_low))));
  assert(br_wtf8_valid(br_bytes_view_make(pair_as_astral, sizeof(pair_as_astral))));
  assert(br_wtf8_valid(br_bytes_view_make(two_highs, sizeof(two_highs))));
  assert(br_wtf8_valid(BR_BYTES_LIT("plain ascii")));
  assert(br_wtf8_valid(br_bytes_view_make(NULL, 0u))); /* empty is valid */

  assert(!br_wtf8_valid(br_bytes_view_make(pair_as_halves, sizeof(pair_as_halves))));
  assert(!br_wtf8_valid(br_bytes_view_make(truncated, sizeof(truncated))));
  assert(!br_wtf8_valid(br_bytes_view_make(overlong, sizeof(overlong))));
  assert(!br_wtf8_valid(br_bytes_view_make(above_max, sizeof(above_max))));
  assert(!br_wtf8_valid(br_bytes_view_make(bad_cont, sizeof(bad_cont))));
  assert(!br_wtf8_valid(br_bytes_view_make(lone_cont, sizeof(lone_cont))));
}

/*
Differential agreement (the safeguard for the duplicated arithmetic): every
strictly-valid UTF-8 input must be valid WTF-8, and decode to the same code
points through both surfaces. Reuses the utf8 suite's positive/boundary vectors.
*/
static void test_differential_agreement(void) {
  static const u8 mixed[] = {'A', 0xe2u, 0x82u, 0xacu, 0xf0u, 0x9fu, 0x99u, 0x82u}; /* A € 🙂 */
  static const u8 boundary_07ff[] = {0xdfu, 0xbfu};
  static const u8 boundary_0800[] = {0xe0u, 0xa0u, 0x80u};
  static const u8 boundary_ffff[] = {0xefu, 0xbfu, 0xbfu};
  static const u8 boundary_10000[] = {0xf0u, 0x90u, 0x80u, 0x80u};
  static const u8 boundary_10ffff[] = {0xf4u, 0x8fu, 0xbfu, 0xbfu};
  const br_bytes_view cases[] = {
    br_bytes_view_make(mixed, sizeof(mixed)),
    br_bytes_view_make(boundary_07ff, sizeof(boundary_07ff)),
    br_bytes_view_make(boundary_0800, sizeof(boundary_0800)),
    br_bytes_view_make(boundary_ffff, sizeof(boundary_ffff)),
    br_bytes_view_make(boundary_10000, sizeof(boundary_10000)),
    br_bytes_view_make(boundary_10ffff, sizeof(boundary_10ffff)),
    BR_BYTES_LIT("hello world"),
  };
  usize c;

  for (c = 0u; c < BR_ARRAY_COUNT(cases); ++c) {
    br_bytes_view v = cases[c];
    u16 units[16];
    u8 back[32];
    br_io_result to16;
    br_io_result to8;

    /* Strictly valid UTF-8 is accepted by both surfaces. */
    assert(br_utf8_valid(v));
    assert(br_wtf8_valid(v));

    /* And it survives WTF-8 -> WTF-16 -> WTF-8 unchanged (no surrogates
       involved, so the transcode is the plain UTF-8<->UTF-16 mapping). If the
       WTF-8-local byte arithmetic diverged from the strict codec's, this
       round-trip would corrupt or mis-size. */
    to16 = br_wtf16_from_wtf8(v, units, BR_ARRAY_COUNT(units));
    assert(to16.status == BR_STATUS_OK);
    to8 = br_wtf8_from_wtf16(br_wtf16_view_make(units, to16.count), back, sizeof(back));
    assert(to8.status == BR_STATUS_OK && to8.count == v.len);
    assert(memcmp(back, v.data, v.len) == 0);
  }
}

static void test_concat_seam(void) {
  /* `a` ends in a lone high surrogate, `b` starts with a lone low surrogate:
     the seam must fuse into one 4-byte astral scalar (U+10000 here). */
  static const u8 a_high[] = {'x', 0xedu, 0xa0u, 0x80u}; /* "x" + lone U+D800 */
  static const u8 b_low[] = {0xedu, 0xb0u, 0x80u, 'y'};  /* lone U+DC00 + "y" */
  static const u8 expect[] = {'x', 0xf0u, 0x90u, 0x80u, 0x80u, 'y'};
  br_bytes_result r = br_wtf8_concat(br_bytes_view_make(a_high, sizeof(a_high)),
                                     br_bytes_view_make(b_low, sizeof(b_low)),
                                     br_allocator_heap());

  assert(r.status == BR_STATUS_OK);
  assert(
    br_bytes_equal(br_bytes_view_from_bytes(r.value), br_bytes_view_make(expect, sizeof(expect))));
  assert(br_wtf8_valid(br_bytes_view_from_bytes(r.value))); /* fused result well-formed */
  assert(br_bytes_free(r.value, br_allocator_heap()) == BR_STATUS_OK);

  /* Non-adjacent-surrogate concat is a plain byte join. */
  {
    br_bytes_result plain =
      br_wtf8_concat(BR_BYTES_LIT("foo"), BR_BYTES_LIT("bar"), br_allocator_heap());
    assert(plain.status == BR_STATUS_OK);
    assert(br_bytes_equal(br_bytes_view_from_bytes(plain.value), BR_BYTES_LIT("foobar")));
    assert(br_bytes_free(plain.value, br_allocator_heap()) == BR_STATUS_OK);
  }

  /* Two lone highs across the seam do NOT fuse (b must start with a LOW). */
  {
    static const u8 b_high[] = {0xedu, 0xa0u, 0x80u};
    br_bytes_result nofuse = br_wtf8_concat(br_bytes_view_make(a_high, sizeof(a_high)),
                                            br_bytes_view_make(b_high, sizeof(b_high)),
                                            br_allocator_heap());
    assert(nofuse.status == BR_STATUS_OK);
    assert(nofuse.value.len == sizeof(a_high) + sizeof(b_high)); /* plain join, no fuse */
    assert(br_bytes_free(nofuse.value, br_allocator_heap()) == BR_STATUS_OK);
  }

  /* Empty operands. */
  {
    br_bytes_result e = br_wtf8_concat(
      br_bytes_view_make(NULL, 0u), br_bytes_view_make(NULL, 0u), br_allocator_heap());
    assert(e.status == BR_STATUS_OK && e.value.len == 0u);
    assert(br_bytes_free(e.value, br_allocator_heap()) == BR_STATUS_OK);
  }
}

static void test_sizers_and_short_buffer(void) {
  static const u16 units[] = {'a', 0xd801u, 0xdc37u, 0x00e9u}; /* a + astral + é */
  u8 wtf8[16];
  br_io_result r;
  usize need = br_wtf8_from_wtf16_len(br_wtf16_view_make(units, BR_ARRAY_COUNT(units)));

  /* a=1 + astral=4 + é=2 = 7 bytes. */
  assert(need == 7u);

  /* Exact-fit succeeds. */
  r = br_wtf8_from_wtf16(br_wtf16_view_make(units, BR_ARRAY_COUNT(units)), wtf8, need);
  assert(r.status == BR_STATUS_OK && r.count == need);

  /* One byte short -> SHORT_BUFFER, nothing written (count 0). */
  r = br_wtf8_from_wtf16(br_wtf16_view_make(units, BR_ARRAY_COUNT(units)), wtf8, need - 1u);
  assert(r.status == BR_STATUS_SHORT_BUFFER && r.count == 0u);

  /* Reverse direction sizer + short buffer. */
  {
    u16 out[8];
    usize need16 = br_wtf16_from_wtf8_len(br_bytes_view_make(wtf8, need));
    br_io_result r16;
    assert(need16 == BR_ARRAY_COUNT(units)); /* 4 units */
    r16 = br_wtf16_from_wtf8(br_bytes_view_make(wtf8, need), out, need16 - 1u);
    assert(r16.status == BR_STATUS_SHORT_BUFFER && r16.count == 0u);
    r16 = br_wtf16_from_wtf8(br_bytes_view_make(wtf8, need), out, need16);
    assert(r16.status == BR_STATUS_OK && r16.count == need16);
  }
}

int main(void) {
  test_lossless_round_trip();
  test_well_formed_validity();
  test_differential_agreement();
  test_concat_seam();
  test_sizers_and_short_buffer();
  return 0;
}
