#include "internal.h"

#if defined(BR__VM_TARGET_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

usize br__vm_platform_page_size_query(void) {
  SYSTEM_INFO info;

  GetSystemInfo(&info);
  return (usize)info.dwPageSize;
}

br_vm_region_result br__vm_platform_reserve(usize size) {
  void *ptr = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);

  if (ptr == NULL) {
    return br__vm_region_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
  }

  return br__vm_region_result((u8 *)ptr, size, BR_STATUS_OK);
}

br_status br__vm_platform_commit(void *ptr, usize size) {
  return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL ? BR_STATUS_OK
                                                                     : BR_STATUS_OUT_OF_MEMORY;
}

void br__vm_platform_decommit(void *ptr, usize size) {
  (void)VirtualFree(ptr, size, MEM_DECOMMIT);
}

void br__vm_platform_release(void *ptr, usize size) {
  BR_UNUSED(size);
  (void)VirtualFree(ptr, 0u, MEM_RELEASE);
}

bool br__vm_platform_protect(void *ptr, usize size, br_vm_protect_flags flags) {
  DWORD protect = PAGE_NOACCESS;
  DWORD old_protect = 0u;

  switch (flags) {
    case BR_VM_PROTECT_NONE:
      protect = PAGE_NOACCESS;
      break;
    case BR_VM_PROTECT_READ:
      protect = PAGE_READONLY;
      break;
    case BR_VM_PROTECT_WRITE:
      protect = PAGE_WRITECOPY;
      break;
    case BR_VM_PROTECT_READ | BR_VM_PROTECT_WRITE:
      protect = PAGE_READWRITE;
      break;
    case BR_VM_PROTECT_EXECUTE:
      protect = PAGE_EXECUTE;
      break;
    case BR_VM_PROTECT_EXECUTE | BR_VM_PROTECT_READ:
      protect = PAGE_EXECUTE_READ;
      break;
    case BR_VM_PROTECT_EXECUTE | BR_VM_PROTECT_WRITE:
      protect = PAGE_EXECUTE_WRITECOPY;
      break;
    case BR_VM_PROTECT_EXECUTE | BR_VM_PROTECT_READ | BR_VM_PROTECT_WRITE:
      protect = PAGE_EXECUTE_READWRITE;
      break;
    default:
      return false;
  }

  return VirtualProtect(ptr, size, protect, &old_protect) != 0;
}

#endif
