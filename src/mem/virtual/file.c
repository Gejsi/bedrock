#include "internal.h"

#if defined(BR__VM_TARGET_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static br_vm_map_file_error
br__vm_open_file_path(const char *path, br_vm_map_file_flags flags, br__vm_native_file *file_out) {
  DWORD desired_access = 0u;
  HANDLE file;

  if (file_out == NULL) {
    return BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT;
  }

  if ((flags & BR_VM_MAP_FILE_READ) != 0u) {
    desired_access |= GENERIC_READ;
  }
  if ((flags & BR_VM_MAP_FILE_WRITE) != 0u) {
    desired_access |= GENERIC_READ | GENERIC_WRITE;
  }

  file = CreateFileA(
    path, desired_access, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    return BR_VM_MAP_FILE_ERROR_OPEN_FAILURE;
  }

  *file_out = (br__vm_native_file)(iptr)file;
  return BR_VM_MAP_FILE_ERROR_NONE;
}

static br_vm_map_file_error br__vm_query_file_size(br__vm_native_file file, i64 *size_out) {
  LARGE_INTEGER file_size;
  HANDLE handle = (HANDLE)(iptr)file;

  if (size_out == NULL) {
    return BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT;
  }
  if (GetFileSizeEx(handle, &file_size) == 0) {
    return BR_VM_MAP_FILE_ERROR_STAT_FAILURE;
  }
  if (file_size.QuadPart < 0) {
    return BR_VM_MAP_FILE_ERROR_NEGATIVE_SIZE;
  }

  *size_out = (i64)file_size.QuadPart;
  return BR_VM_MAP_FILE_ERROR_NONE;
}

static void br__vm_close_file(br__vm_native_file file) {
  HANDLE handle = (HANDLE)(iptr)file;

  if (handle != NULL && handle != INVALID_HANDLE_VALUE) {
    (void)CloseHandle(handle);
  }
}

#elif defined(BR__VM_TARGET_LINUX) || defined(BR__VM_TARGET_POSIX)

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static br_vm_map_file_error
br__vm_open_file_path(const char *path, br_vm_map_file_flags flags, br__vm_native_file *file_out) {
  int open_flags = O_RDONLY;
  int fd;

  if (file_out == NULL) {
    return BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT;
  }
  if ((flags & BR_VM_MAP_FILE_WRITE) != 0u) {
    open_flags = O_RDWR;
  }

  fd = open(path, open_flags);
  if (fd < 0) {
    return BR_VM_MAP_FILE_ERROR_OPEN_FAILURE;
  }

  *file_out = (br__vm_native_file)fd;
  return BR_VM_MAP_FILE_ERROR_NONE;
}

static br_vm_map_file_error br__vm_query_file_size(br__vm_native_file file, i64 *size_out) {
  struct stat st;
  int fd = (int)file;

  if (size_out == NULL) {
    return BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT;
  }
  if (fstat(fd, &st) != 0) {
    return BR_VM_MAP_FILE_ERROR_STAT_FAILURE;
  }
  if (st.st_size < 0) {
    return BR_VM_MAP_FILE_ERROR_NEGATIVE_SIZE;
  }

  *size_out = (i64)st.st_size;
  return BR_VM_MAP_FILE_ERROR_NONE;
}

static void br__vm_close_file(br__vm_native_file file) {
  int fd = (int)file;

  if (fd >= 0) {
    (void)close(fd);
  }
}

#else

static br_vm_map_file_error
br__vm_open_file_path(const char *path, br_vm_map_file_flags flags, br__vm_native_file *file_out) {
  BR_UNUSED(path);
  BR_UNUSED(flags);
  BR_UNUSED(file_out);
  return BR_VM_MAP_FILE_ERROR_MAP_FAILURE;
}

static br_vm_map_file_error br__vm_query_file_size(br__vm_native_file file, i64 *size_out) {
  BR_UNUSED(file);
  BR_UNUSED(size_out);
  return BR_VM_MAP_FILE_ERROR_MAP_FAILURE;
}

static void br__vm_close_file(br__vm_native_file file) {
  BR_UNUSED(file);
}

#endif

br_vm_mapped_file_result br_vm_map_file(const char *path, br_vm_map_file_flags flags) {
  br__vm_native_file file = (br__vm_native_file)0;
  br_vm_map_file_error error;
  br_vm_mapped_file_result result;
  i64 size;

  if (path == NULL || (flags & (BR_VM_MAP_FILE_READ | BR_VM_MAP_FILE_WRITE)) == 0u) {
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT);
  }

  /*
  Bedrock still exposes only the path-based public API until an `os/file`
  layer exists, but this file now owns the high-level open/stat/map flow like
  Odin's `virtual/file.odin` instead of pushing it all into per-platform files.
  */
  error = br__vm_open_file_path(path, flags, &file);
  if (error != BR_VM_MAP_FILE_ERROR_NONE) {
    return br__vm_mapped_file_result(NULL, 0u, error);
  }

  error = br__vm_query_file_size(file, &size);
  if (error != BR_VM_MAP_FILE_ERROR_NONE) {
    br__vm_close_file(file);
    return br__vm_mapped_file_result(NULL, 0u, error);
  }
  if ((u64)size > (u64)SIZE_MAX) {
    br__vm_close_file(file);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_TOO_LARGE_SIZE);
  }
  if (size == 0) {
    br__vm_close_file(file);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_NONE);
  }

  result = br__vm_platform_map_open_file(file, (usize)size, flags);
  br__vm_close_file(file);
  return result;
}

void br_vm_unmap_file(br_vm_mapped_file mapping) {
  if (mapping.data == NULL || mapping.size == 0u) {
    return;
  }

  br__vm_platform_unmap_file(mapping);
}
