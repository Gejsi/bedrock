#include <bedrock/rand/rand.h>

/* Targets with no supported OS entropy source. Unlike the futex backend (which
   is required and #errors), entropy degrades gracefully: the caller gets
   BR_STATUS_NOT_SUPPORTED and must seed explicitly, rather than the module
   inventing a weak time()-style seed. */
#if !defined(__linux__) && !defined(_WIN32) && !(defined(__APPLE__) && defined(__MACH__)) &&       \
  !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)

br_status br_rand_entropy_fill(void *dst, usize len) {
  BR_UNUSED(dst);
  BR_UNUSED(len);
  return BR_STATUS_NOT_SUPPORTED;
}

#endif
