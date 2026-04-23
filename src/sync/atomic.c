#include <bedrock/sync/atomic.h>

void br_cpu_relax(void) {
#if defined(__i386__) || defined(__x86_64__)
  __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
  __asm__ __volatile__("yield");
#else
  br_atomic_signal_fence(BR_ATOMIC_SEQ_CST);
#endif
}
