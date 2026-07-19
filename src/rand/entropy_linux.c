#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* getrandom */
#endif

#include <bedrock/rand/rand.h>

#if defined(__linux__)

#include <errno.h>
#include <sys/random.h>

br_status br_rand_entropy_fill(void *dst, usize len) {
  u8 *out = (u8 *)dst;
  usize filled = 0u;

  while (filled < len) {
    ssize_t n = getrandom(out + filled, len - filled, 0);
    if (n < 0) {
      if (errno == EINTR) {
        continue; /* interrupted before any bytes; retry */
      }
      return BR_STATUS_NOT_SUPPORTED;
    }
    /* getrandom can return fewer bytes than requested; advance and continue
       until the whole buffer is filled. */
    filled += (usize)n;
  }
  return BR_STATUS_OK;
}

#endif /* defined(__linux__) */
