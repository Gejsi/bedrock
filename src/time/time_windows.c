#include <bedrock/time/time.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

/* GetSystemTimePreciseAsFileTime requires Windows 8 (0x0602). */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#include <windows.h>

#define BR__WINDOWS_EPOCH_OFFSET_100NS ((u64)116444736000000000ull)

static i64 br__time_mul_div_i64(i64 value, i64 numerator, i64 denominator) {
  i64 quotient;
  i64 remainder;
  i64 fractional;

  if (value <= 0 || numerator <= 0 || denominator <= 0) {
    return 0;
  }
  quotient = value / denominator;
  remainder = value % denominator;
  if (quotient > INT64_MAX / numerator) {
    return INT64_MAX;
  }

  /*
  The exact fractional result is smaller than numerator. Floating-point avoids
  overflowing remainder*numerator on unusual high-frequency counters; any
  rounding is confined to the sub-second remainder.
  */
  fractional = (i64)((f64)remainder * (f64)numerator / (f64)denominator);
  if (quotient * numerator > INT64_MAX - fractional) {
    return INT64_MAX;
  }
  return quotient * numerator + fractional;
}

static i64 br__time_filetime_to_nsec(u64 ticks_100ns) {
  if (ticks_100ns >= BR__WINDOWS_EPOCH_OFFSET_100NS) {
    u64 since_epoch = ticks_100ns - BR__WINDOWS_EPOCH_OFFSET_100NS;
    if (since_epoch > (u64)INT64_MAX / 100u) {
      return INT64_MAX;
    }
    return (i64)(since_epoch * 100u);
  } else {
    u64 before_epoch = BR__WINDOWS_EPOCH_OFFSET_100NS - ticks_100ns;
    if (before_epoch > ((u64)INT64_MAX + 1u) / 100u) {
      return INT64_MIN;
    }
    return -(i64)(before_epoch * 100u);
  }
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

  result.nsec = br__time_filetime_to_nsec(ticks.QuadPart);
  return result;
}

void br_sleep(br_duration duration) {
  const br_duration max_chunk_millis = (br_duration)INFINITE - 1;
  const br_duration max_chunk = max_chunk_millis * BR_MILLISECOND;

  while (duration > max_chunk) {
    Sleep((DWORD)max_chunk_millis);
    duration -= max_chunk;
  }
  if (duration > 0) {
    br_duration millis = duration / BR_MILLISECOND;

    if (duration % BR_MILLISECOND != 0) {
      millis += 1;
    }
    Sleep((DWORD)millis);
  }
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
