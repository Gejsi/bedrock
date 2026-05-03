#include <bedrock/types.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#include <bedrock/sync/futex.h>

#include <windows.h>

typedef LONG(WINAPI *br__rtl_wait_on_address_fn)(volatile void *address,
                                                 void *compare_address,
                                                 SIZE_T address_size,
                                                 const LARGE_INTEGER *timeout);
typedef ULONG(WINAPI *br__rtl_nt_status_to_dos_error_fn)(LONG status);
typedef VOID(WINAPI *br__wake_by_address_fn)(void *address);

static bool br__windows_symbol(const char *module_name, const char *name, FARPROC *out_symbol) {
  HMODULE module;

  if (out_symbol == NULL) {
    return false;
  }

  module = GetModuleHandleA(module_name);
  if (module == NULL) {
    return false;
  }

  *out_symbol = GetProcAddress(module, name);
  return *out_symbol != NULL;
}

static bool br__windows_ntdll_symbol(const char *name, FARPROC *out_symbol) {
  return br__windows_symbol("ntdll.dll", name, out_symbol);
}

static bool br__windows_sync_symbol(const char *name, FARPROC *out_symbol) {
  if (br__windows_symbol("api-ms-win-core-synch-l1-2-0.dll", name, out_symbol)) {
    return true;
  }
  if (br__windows_symbol("KernelBase.dll", name, out_symbol)) {
    return true;
  }
  return br__windows_symbol("kernel32.dll", name, out_symbol);
}

static bool br__windows_rtl_wait_on_address(br_futex *futex, u32 *compare, SIZE_T address_size) {
  FARPROC wait_symbol;
  FARPROC status_symbol;
  br__rtl_wait_on_address_fn wait_on_address = NULL;
  br__rtl_nt_status_to_dos_error_fn nt_status_to_dos_error = NULL;
  LONG status;

  if (!br__windows_ntdll_symbol("RtlWaitOnAddress", &wait_symbol)) {
    return false;
  }
  if (!br__windows_ntdll_symbol("RtlNtStatusToDosError", &status_symbol)) {
    return false;
  }

  BR_STATIC_ASSERT(sizeof(wait_on_address) == sizeof(wait_symbol), "unexpected FARPROC size");
  BR_STATIC_ASSERT(sizeof(nt_status_to_dos_error) == sizeof(status_symbol),
                   "unexpected FARPROC size");
  memcpy(&wait_on_address, &wait_symbol, sizeof(wait_on_address));
  memcpy(&nt_status_to_dos_error, &status_symbol, sizeof(nt_status_to_dos_error));

  status = wait_on_address((volatile void *)futex, compare, address_size, NULL);
  if (status != 0) {
    SetLastError(nt_status_to_dos_error(status));
  }

  return status == 0;
}

static void br__windows_wake_by_address(br_futex *futex, bool broadcast) {
  FARPROC wake_symbol;
  br__wake_by_address_fn wake_by_address = NULL;

  if (!br__windows_sync_symbol(broadcast ? "WakeByAddressAll" : "WakeByAddressSingle",
                               &wake_symbol)) {
    return;
  }

  BR_STATIC_ASSERT(sizeof(wake_by_address) == sizeof(wake_symbol), "unexpected FARPROC size");
  memcpy(&wake_by_address, &wake_symbol, sizeof(wake_by_address));
  wake_by_address((void *)futex);
}

bool br_futex_wait(br_futex *futex, u32 expected) {
  u32 compare;

  if (futex == NULL) {
    return false;
  }
  if (br_atomic_load_explicit(futex, BR_ATOMIC_ACQUIRE) != expected) {
    return true;
  }

  compare = expected;
  /*
  Odin avoids public WaitOnAddress because its wrapper must translate NTSTATUS
  into GetLastError-compatible codes. Bedrock keeps that behavior and resolves
  all wait/wake symbols at runtime so users do not need Windows sync import
  libraries when linking a static Bedrock build.
  */
  return br__windows_rtl_wait_on_address(futex, &compare, sizeof(compare));
}

void br_futex_signal(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  br__windows_wake_by_address(futex, false);
}

void br_futex_broadcast(br_futex *futex) {
  if (futex == NULL) {
    return;
  }
  br__windows_wake_by_address(futex, true);
}

#else
typedef u8 br__sync_futex_windows_translation_unit;
#endif
