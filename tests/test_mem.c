#include <assert.h>

#include <bedrock.h>

static void test_mem_set_zero(void) {
  u8 buf[16];
  void *ret;

  ret = br_mem_set(buf, 0xabu, sizeof(buf));
  assert(ret == buf);
  for (usize i = 0u; i < sizeof(buf); i += 1u) {
    assert(buf[i] == 0xabu);
  }

  ret = br_mem_zero(buf, sizeof(buf));
  assert(ret == buf);
  assert(br_mem_check_zero(buf, sizeof(buf)));

  /* Zero length is a no-op and must not touch the buffer. */
  buf[0] = 0x11u;
  assert(br_mem_set(buf, 0xffu, 0u) == buf);
  assert(buf[0] == 0x11u);
  assert(br_mem_zero(buf, 0u) == buf);
  assert(buf[0] == 0x11u);
}

static void test_mem_copy_overlap(void) {
  u8 buf[8];
  u8 disjoint_src[4] = {1u, 2u, 3u, 4u};
  u8 disjoint_dst[4];
  void *ret;

  /* Forward overlap: dst ahead of src within the same buffer. memmove must not
     corrupt the tail the way a naive forward memcpy would. */
  for (usize i = 0u; i < sizeof(buf); i += 1u) {
    buf[i] = (u8)(i + 1u); /* 1 2 3 4 5 6 7 8 */
  }
  ret = br_mem_copy(buf + 2, buf, 4u); /* copy [1 2 3 4] over offset 2 */
  assert(ret == buf + 2);
  {
    static const u8 expect[8] = {1u, 2u, 1u, 2u, 3u, 4u, 7u, 8u};
    assert(br_mem_compare_ptrs(buf, expect, sizeof(buf)) == 0);
  }

  /* Backward overlap: src ahead of dst. */
  for (usize i = 0u; i < sizeof(buf); i += 1u) {
    buf[i] = (u8)(i + 1u);
  }
  ret = br_mem_copy(buf, buf + 2, 4u); /* copy [3 4 5 6] to the front */
  assert(ret == buf);
  {
    static const u8 expect[8] = {3u, 4u, 5u, 6u, 5u, 6u, 7u, 8u};
    assert(br_mem_compare_ptrs(buf, expect, sizeof(buf)) == 0);
  }

  /* Non-overlapping copy between disjoint buffers. */
  ret = br_mem_copy_non_overlapping(disjoint_dst, disjoint_src, sizeof(disjoint_src));
  assert(ret == disjoint_dst);
  assert(br_mem_compare_ptrs(disjoint_dst, disjoint_src, sizeof(disjoint_src)) == 0);

  /* Zero-length copies are no-ops that still return dst. */
  assert(br_mem_copy(buf, disjoint_src, 0u) == buf);
  assert(br_mem_copy_non_overlapping(buf, disjoint_src, 0u) == buf);
}

static void test_mem_compare(void) {
  /* compare_ptrs: fixed-width orderings. */
  assert(br_mem_compare_ptrs("abc", "abc", 3u) == 0);
  assert(br_mem_compare_ptrs("abc", "abd", 3u) == -1);
  assert(br_mem_compare_ptrs("abd", "abc", 3u) == 1);
  assert(br_mem_compare_ptrs("abc", "abc", 0u) == 0); /* n == 0 */

  /* compare: bytewise first, then length as the tiebreak. */
  assert(br_mem_compare("abc", 3u, "abc", 3u) == 0);
  assert(br_mem_compare("abc", 3u, "abd", 3u) == -1); /* differ in prefix */
  assert(br_mem_compare("ab", 2u, "abc", 3u) == -1);  /* prefix equal, shorter is smaller */
  assert(br_mem_compare("abc", 3u, "ab", 2u) == 1);   /* prefix equal, longer is bigger */
  assert(br_mem_compare(NULL, 0u, NULL, 0u) == 0);    /* both empty */
  assert(br_mem_compare(NULL, 0u, "x", 1u) == -1);    /* empty vs non-empty */
  assert(br_mem_compare("x", 1u, NULL, 0u) == 1);
  /* A high byte must compare as greater (unsigned), guarding signedness bugs. */
  {
    static const u8 hi[1] = {0x80u};
    static const u8 lo[1] = {0x7fu};
    assert(br_mem_compare_ptrs(hi, lo, 1u) == 1);
  }
}

static void test_mem_check_zero(void) {
  u8 buf[32];

  br_mem_zero(buf, sizeof(buf));
  assert(br_mem_check_zero(buf, sizeof(buf)));

  /* A single non-zero byte at any offset must be detected. */
  for (usize off = 0u; off < sizeof(buf); off += 1u) {
    br_mem_zero(buf, sizeof(buf));
    buf[off] = 1u;
    assert(!br_mem_check_zero(buf, sizeof(buf)));
  }

  /* Empty and NULL cases are vacuously all-zero. */
  assert(br_mem_check_zero(buf, 0u));
  assert(br_mem_check_zero(NULL, 0u));

  /* len == 3 starting at an ODD address: this is the case that trips Odin's
     word-alignment prologue into an out-of-bounds read. Bedrock's byte walk
     stays in bounds. Exercised at an odd offset with the 3 bytes non-zero, and
     with a non-zero byte just past the region to catch any over-read. */
  {
    u8 guard[8] = {0u, 1u, 1u, 1u, 0xffu, 0u, 0u, 0u};
    /* guard[1..3] are the region (all non-zero -> false); guard[4] is a
       sentinel that must not be read when len == 3. */
    assert(!br_mem_check_zero(guard + 1, 3u));
    guard[1] = 0u;
    guard[2] = 0u;
    guard[3] = 0u;
    guard[4] = 0xffu; /* still set; a correct len==3 scan ignores it */
    assert(br_mem_check_zero(guard + 1, 3u));
  }
}

int main(void) {
  test_mem_set_zero();
  test_mem_copy_overlap();
  test_mem_compare();
  test_mem_check_zero();
  return 0;
}
