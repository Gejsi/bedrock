#ifndef BEDROCK_MEM_MEM_H
#define BEDROCK_MEM_MEM_H

#include <bedrock/base.h>

BR_EXTERN_C_BEGIN

/*
Portable low-level memory helpers: a thin, standard-C-typed layer over the C
library memory routines. All helpers take `void *`/`size_t` and are header-only
because each is a single wrapper over `<string.h>`. Pointer-arithmetic and
runtime-layout helpers are deliberately out of scope for this module.
*/

/*
Set the first `len` bytes at `dst` to `value`; returns `dst`.
*/
static inline void *br_mem_set(void *dst, uint8_t value, size_t len) {
  if (len != 0u) {
    memset(dst, (int)value, len);
  }
  return dst;
}

/*
Set the first `len` bytes at `dst` to zero; returns `dst`.

This is an ordinary zeroing and may be optimized away when the store is not
observed. A guaranteed, non-elidable secure zero is a distinct primitive and is
not provided yet.
*/
static inline void *br_mem_zero(void *dst, size_t len) {
  if (len != 0u) {
    memset(dst, 0, len);
  }
  return dst;
}

/*
Copy `len` bytes from `src` to `dst`, which must NOT overlap; returns `dst`.
Overlapping ranges are undefined behavior; use `br_mem_move` when they may
overlap.
*/
static inline void *br_mem_copy(void *dst, const void *src, size_t len) {
  if (len != 0u) {
    memcpy(dst, src, len);
  }
  return dst;
}

/*
Copy `len` bytes from `src` to `dst`, which may overlap; returns `dst`.
*/
static inline void *br_mem_move(void *dst, const void *src, size_t len) {
  if (len != 0u) {
    memmove(dst, src, len);
  }
  return dst;
}

/*
Compare `n` bytes at `a` and `b`, returning `-1`, `0`, or `+1` for `a` less
than, equal to, or greater than `b`.
*/
static inline int br_mem_compare_ptrs(const void *a, const void *b, size_t n) {
  int cmp;

  if (n == 0u) {
    return 0;
  }
  cmp = memcmp(a, b, n);
  if (cmp < 0) {
    return -1;
  }
  if (cmp > 0) {
    return 1;
  }
  return 0;
}

/*
Compare two byte ranges of possibly different lengths, returning `-1`, `0`, or
`+1`. The shared prefix is compared first; if it is equal, the shorter range
compares as smaller. Two empty ranges compare equal. This matches
`br_bytes_compare`.
*/
static inline int br_mem_compare(const void *a, size_t a_len, const void *b, size_t b_len) {
  size_t common = a_len < b_len ? a_len : b_len;
  int cmp = br_mem_compare_ptrs(a, b, common);

  if (cmp != 0) {
    return cmp;
  }
  if (a_len == b_len) {
    return 0;
  }
  return a_len < b_len ? -1 : 1;
}

/*
Report whether all `len` bytes starting at `ptr` are zero. A zero length (or a
NULL pointer with zero length) is vacuously all-zero and returns `true`.

The scan is a straightforward in-bounds byte walk that never reads outside
`[ptr, ptr + len)`.
*/
static inline bool br_mem_check_zero(const void *ptr, size_t len) {
  const uint8_t *bytes = (const uint8_t *)ptr;

  if (len == 0u || ptr == NULL) {
    return true;
  }

  for (size_t i = 0u; i < len; i += 1u) {
    if (bytes[i] != 0u) {
      return false;
    }
  }
  return true;
}

BR_EXTERN_C_END

#endif
