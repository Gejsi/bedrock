#include "internal.h"

#if defined(BR__VM_BACKEND_WINDOWS)

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

br_vm_mapped_file_result br__vm_platform_map_file(const char *path, br_vm_map_file_flags flags) {
  DWORD desired_access = 0u;
  DWORD protect = PAGE_READONLY;
  DWORD map_access = FILE_MAP_READ;
  HANDLE file = INVALID_HANDLE_VALUE;
  HANDLE mapping = NULL;
  LARGE_INTEGER file_size;
  void *view = NULL;

  if ((flags & BR_VM_MAP_FILE_READ) != 0u) {
    desired_access |= GENERIC_READ;
  }
  if ((flags & BR_VM_MAP_FILE_WRITE) != 0u) {
    desired_access |= GENERIC_READ | GENERIC_WRITE;
    protect = PAGE_READWRITE;
    map_access = FILE_MAP_READ | FILE_MAP_WRITE;
  }

  file = CreateFileA(
    path, desired_access, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_OPEN_FAILURE);
  }

  if (GetFileSizeEx(file, &file_size) == 0) {
    CloseHandle(file);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_STAT_FAILURE);
  }
  if (file_size.QuadPart < 0) {
    CloseHandle(file);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_NEGATIVE_SIZE);
  }
  if ((u64)file_size.QuadPart > (u64)SIZE_MAX) {
    CloseHandle(file);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_TOO_LARGE_SIZE);
  }
  if (file_size.QuadPart == 0) {
    CloseHandle(file);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_NONE);
  }

  mapping = CreateFileMappingA(file, NULL, protect, 0u, 0u, NULL);
  if (mapping == NULL) {
    CloseHandle(file);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_MAP_FAILURE);
  }

  view = MapViewOfFile(mapping, map_access, 0u, 0u, 0u);
  CloseHandle(mapping);
  CloseHandle(file);
  if (view == NULL) {
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_MAP_FAILURE);
  }

  return br__vm_mapped_file_result(
    (u8 *)view, (usize)file_size.QuadPart, BR_VM_MAP_FILE_ERROR_NONE);
}

void br__vm_platform_unmap_file(br_vm_mapped_file mapping) {
  (void)FlushViewOfFile(mapping.data, mapping.size);
  (void)UnmapViewOfFile(mapping.data);
}

#endif
