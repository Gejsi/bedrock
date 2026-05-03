#include <bedrock/time/time.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#define BR__WINDOWS_EPOCH_OFFSET_100NS ((u64)116444736000000000ull)

static i64 br__time_mul_div_i64(i64 value, i64 numerator, i64 denominator) {
  i64 quotient = value / denominator;
  i64 remainder = value % denominator;
  return quotient * numerator + remainder * numerator / denominator;
}

bool br_time_is_supported(void) {
  return true;
}

br_time br_time_now(void) {
  FILETIME file_time;
  ULARGE_INTEGER ticks;
  br_time result;

  GetSystemTimePreciseAsFileTime(&file_time);
  ticks.LowPart = file_time.dwLowDateTime;
  ticks.HighPart = file_time.dwHighDateTime;

  result.nsec = (i64)((ticks.QuadPart - BR__WINDOWS_EPOCH_OFFSET_100NS) * 100u);
  return result;
}

void br_sleep(br_duration duration) {
  if (duration <= 0) {
    return;
  }

  Sleep((DWORD)(duration / BR_MILLISECOND));
}

br_tick br_tick_now(void) {
  LARGE_INTEGER frequency;
  LARGE_INTEGER now;
  br_tick result = {0};

  if (QueryPerformanceFrequency(&frequency) == 0) {
    return result;
  }
  if (QueryPerformanceCounter(&now) == 0) {
    return result;
  }

  result.nsec = br__time_mul_div_i64(now.QuadPart, BR_SECOND, frequency.QuadPart);
  return result;
}

void br_yield(void) {
  SwitchToThread();
}

#else
typedef u8 br__time_windows_translation_unit;
#endif
