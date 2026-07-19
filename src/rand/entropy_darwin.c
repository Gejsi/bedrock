#include <bedrock/rand/rand.h>

#if (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__) || defined(__NetBSD__) ||    \
  defined(__OpenBSD__)

#include <sys/random.h> /* getentropy */

/* getentropy fills at most 256 bytes per call. */
#define BR__RAND_GETENTROPY_MAX 256u

br_status br_rand_entropy_fill(void *dst, usize len) {
  u8 *out = (u8 *)dst;
  usize filled = 0u;

  while (filled < len) {
    usize chunk = len - filled;
    if (chunk > BR__RAND_GETENTROPY_MAX) {
      chunk = BR__RAND_GETENTROPY_MAX;
    }
    if (getentropy(out + filled, chunk) != 0) {
      return BR_STATUS_NOT_SUPPORTED;
    }
    filled += chunk;
  }
  return BR_STATUS_OK;
}

#endif
