#include <bedrock/time/time.h>

#if !defined(__linux__) && !defined(_WIN32) && !(defined(__APPLE__) && defined(__MACH__)) &&       \
  !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)

bool br_time_is_supported(void) {
  return false;
}

br_time br_time_now(void) {
  br_time result = {0};
  return result;
}

void br_sleep(br_duration duration) {
  BR_UNUSED(duration);
}

br_tick br_tick_now(void) {
  br_tick result = {0};
  return result;
}

void br_yield(void) {}

#else
typedef u8 br__time_other_translation_unit;
#endif
