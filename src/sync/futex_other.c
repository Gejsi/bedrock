#include <bedrock/types.h>

#if !defined(__linux__) && !defined(_WIN32) && !(defined(__APPLE__) && defined(__MACH__)) &&       \
  !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)

#error "Bedrock sync requires a futex backend for this target."

#else
typedef u8 br__sync_futex_other_translation_unit;
#endif
