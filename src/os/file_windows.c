#include "file_internal.h"

/* Expose FileIdInfo unless the caller selected an older Windows SDK target. */
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0602
#endif

#include "error_internal.h"

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string.h>

#include <bedrock/unicode/wtf8.h>

BR_STATIC_ASSERT(sizeof(WCHAR) == sizeof(uint16_t), "WCHAR must be one UTF-16 code unit");

#define BR__FILE_WINDOWS_LEGACY_PATH_UNITS 248u
#define BR__FILE_WINDOWS_MAX_PATH_UNITS 32767u

static HANDLE br__file_windows_handle(const br_file *file) {
  return (HANDLE)file->handle;
}

static HANDLE br__file_windows_positioned_handle(const br_file *file) {
  return (HANDLE)file->positioned_handle;
}

static DWORD br__file_windows_chunk(size_t len) {
  return len > (size_t)UINT32_MAX ? (DWORD)UINT32_MAX : (DWORD)len;
}

static void br__file_overlapped_offset(OVERLAPPED *overlapped, uint64_t offset) {
  *overlapped = (OVERLAPPED){0};
  overlapped->Offset = (DWORD)(offset & UINT32_MAX);
  overlapped->OffsetHigh = (DWORD)(offset >> 32u);
}

static bool br__file_windows_is_separator(WCHAR unit) {
  return unit == L'\\' || unit == L'/';
}

static bool br__file_windows_has_native_prefix(const WCHAR *path, size_t len) {
  if (len >= 4u && br__file_windows_is_separator(path[0]) &&
      br__file_windows_is_separator(path[1]) && (path[2] == L'?' || path[2] == L'.') &&
      br__file_windows_is_separator(path[3])) {
    return true;
  }
  return len >= 4u && br__file_windows_is_separator(path[0]) && path[1] == L'?' &&
         path[2] == L'?' && br__file_windows_is_separator(path[3]);
}

static bool br__file_windows_is_absolute(const WCHAR *path, size_t len) {
  if (len >= 3u && path[1] == L':' && br__file_windows_is_separator(path[2])) {
    return true;
  }
  return len >= 2u && br__file_windows_is_separator(path[0]) &&
         br__file_windows_is_separator(path[1]);
}

static br_error br__file_windows_long_path(HANDLE heap, WCHAR **path, size_t original_len) {
  const WCHAR verbatim_prefix[] = {L'\\', L'\\', L'?', L'\\'};
  const WCHAR unc_prefix[] = {L'\\', L'\\', L'?', L'\\', L'U', L'N', L'C', L'\\'};
  WCHAR stack_path[BR__FILE_WINDOWS_LEGACY_PATH_UNITS];
  WCHAR *absolute_path;
  WCHAR *result_path;
  const WCHAR *absolute;
  const WCHAR *prefix;
  size_t prefix_len;
  size_t skip;
  size_t absolute_len;
  size_t result_len;
  DWORD capacity;
  DWORD count;

  if (original_len == 0u || br__file_windows_has_native_prefix(*path, original_len)) {
    return BR_ERROR_OK;
  }
  if (original_len + 1u < BR__FILE_WINDOWS_LEGACY_PATH_UNITS &&
      br__file_windows_is_absolute(*path, original_len)) {
    return BR_ERROR_OK;
  }

  count = GetFullPathNameW(*path, (DWORD)BR_ARRAY_COUNT(stack_path), stack_path, NULL);
  if (count == 0u) {
    return br__os_error_from_win32(GetLastError());
  }
  if (count < BR__FILE_WINDOWS_LEGACY_PATH_UNITS - 1u) {
    return BR_ERROR_OK;
  }

  absolute_path = NULL;
  absolute = stack_path;
  absolute_len = (size_t)count;
  if (count >= (DWORD)BR_ARRAY_COUNT(stack_path)) {
    capacity = count;
    for (;;) {
      if (capacity > BR__FILE_WINDOWS_MAX_PATH_UNITS) {
        return br_error_make(BR_STATUS_PATH_TOO_LONG);
      }

      absolute_path = (WCHAR *)HeapAlloc(heap, 0u, (size_t)capacity * sizeof(*absolute_path));
      if (absolute_path == NULL) {
        return br_error_make(BR_STATUS_OUT_OF_MEMORY);
      }

      count = GetFullPathNameW(*path, capacity, absolute_path, NULL);
      if (count == 0u) {
        DWORD native_error = GetLastError();

        (void)HeapFree(heap, 0u, absolute_path);
        return br__os_error_from_win32(native_error);
      }
      if (count < capacity) {
        absolute_len = (size_t)count;
        absolute = absolute_path;
        break;
      }

      (void)HeapFree(heap, 0u, absolute_path);
      absolute_path = NULL;
      capacity = count;
    }
  }

  prefix = NULL;
  prefix_len = 0u;
  skip = 0u;
  if (absolute_len >= 3u && absolute[1] == L':' && br__file_windows_is_separator(absolute[2])) {
    prefix = verbatim_prefix;
    prefix_len = BR_ARRAY_COUNT(verbatim_prefix);
  } else if (absolute_len >= 2u && br__file_windows_is_separator(absolute[0]) &&
             br__file_windows_is_separator(absolute[1])) {
    prefix = unc_prefix;
    prefix_len = BR_ARRAY_COUNT(unc_prefix);
    skip = 2u;
  } else {
    if (absolute_path != NULL) {
      (void)HeapFree(heap, 0u, absolute_path);
    }
    return br_error_make(BR_STATUS_PATH_TOO_LONG);
  }

  result_len = prefix_len + absolute_len - skip;
  if (result_len + 1u > BR__FILE_WINDOWS_MAX_PATH_UNITS) {
    if (absolute_path != NULL) {
      (void)HeapFree(heap, 0u, absolute_path);
    }
    return br_error_make(BR_STATUS_PATH_TOO_LONG);
  }

  result_path = (WCHAR *)HeapAlloc(heap, 0u, (result_len + 1u) * sizeof(*result_path));
  if (result_path == NULL) {
    if (absolute_path != NULL) {
      (void)HeapFree(heap, 0u, absolute_path);
    }
    return br_error_make(BR_STATUS_OUT_OF_MEMORY);
  }

  memcpy(result_path, prefix, prefix_len * sizeof(*result_path));
  memcpy(result_path + prefix_len, absolute + skip, (absolute_len - skip) * sizeof(*result_path));
  result_path[result_len] = L'\0';

  if (absolute_path != NULL) {
    (void)HeapFree(heap, 0u, absolute_path);
  }
  (void)HeapFree(heap, 0u, *path);
  *path = result_path;
  return BR_ERROR_OK;
}

static br_error br__file_windows_path(HANDLE heap, br_string_view path, WCHAR **out_native_path) {
  br_bytes_view bytes;
  br_io_result converted;
  br_error error;
  uint16_t *native_path;
  size_t unit_count;
  size_t allocation_size;
  size_t i;

  *out_native_path = NULL;
  if (path.data == NULL && path.len > 0u) {
    return br_error_make(BR_STATUS_INVALID_ARGUMENT);
  }
  for (i = 0u; i < path.len; i += 1u) {
    if (path.data[i] == '\0') {
      return br_error_make(BR_STATUS_INVALID_ARGUMENT);
    }
  }

  bytes = br_bytes_view_make(path.data, path.len);
  if (!br_wtf8_valid(bytes)) {
    return br_error_make(BR_STATUS_INVALID_ENCODING);
  }

  unit_count = br_wtf16_from_wtf8_len(bytes);
  if (unit_count >= BR__FILE_WINDOWS_MAX_PATH_UNITS) {
    return br_error_make(BR_STATUS_PATH_TOO_LONG);
  }
  if (unit_count > SIZE_MAX / sizeof(*native_path) - 1u) {
    return br_error_make(BR_STATUS_PATH_TOO_LONG);
  }
  allocation_size = (unit_count + 1u) * sizeof(*native_path);
  native_path = (uint16_t *)HeapAlloc(heap, 0u, allocation_size);
  if (native_path == NULL) {
    return br_error_make(BR_STATUS_OUT_OF_MEMORY);
  }

  converted = br_wtf16_from_wtf8(bytes, native_path, unit_count);
  if (converted.status != BR_STATUS_OK || converted.count != unit_count) {
    (void)HeapFree(heap, 0u, native_path);
    return br_error_make(converted.status != BR_STATUS_OK ? converted.status
                                                          : BR_STATUS_INVALID_STATE);
  }

  native_path[unit_count] = 0u;
  error = br__file_windows_long_path(heap, (WCHAR **)(void *)&native_path, unit_count);
  if (error.status != BR_STATUS_OK) {
    (void)HeapFree(heap, 0u, native_path);
    return error;
  }

  *out_native_path = (WCHAR *)(void *)native_path;
  return BR_ERROR_OK;
}

static DWORD br__file_windows_access(br_file_open_flags flags) {
  DWORD access;
  bool append;
  bool needs_overwrite_access;

  access = 0u;
  append = (flags & BR_FILE_OPEN_APPEND) != 0u;
  needs_overwrite_access =
    append && (flags & BR_FILE_OPEN_TRUNCATE) != 0u && (flags & BR_FILE_OPEN_CREATE_NEW) == 0u;

  if ((flags & BR_FILE_OPEN_READ) != 0u) {
    access |= GENERIC_READ;
  }
  if ((flags & BR_FILE_OPEN_WRITE) != 0u) {
    if (append && !needs_overwrite_access) {
      access |= FILE_GENERIC_WRITE & ~(DWORD)FILE_WRITE_DATA;
    } else {
      access |= GENERIC_WRITE;
    }
  }
  return access;
}

static DWORD br__file_windows_creation(br_file_open_flags flags) {
  if ((flags & BR_FILE_OPEN_CREATE_NEW) != 0u) {
    return CREATE_NEW;
  }
  if ((flags & BR_FILE_OPEN_CREATE) != 0u && (flags & BR_FILE_OPEN_TRUNCATE) != 0u) {
    return CREATE_ALWAYS;
  }
  if ((flags & BR_FILE_OPEN_CREATE) != 0u) {
    return OPEN_ALWAYS;
  }
  if ((flags & BR_FILE_OPEN_TRUNCATE) != 0u) {
    return TRUNCATE_EXISTING;
  }
  return OPEN_EXISTING;
}

static bool br__file_windows_needs_positioned_handle(br_file_open_flags flags) {
  return (flags & BR_FILE_OPEN_READ) != 0u ||
         ((flags & BR_FILE_OPEN_WRITE) != 0u && (flags & BR_FILE_OPEN_APPEND) == 0u);
}

br_error br__file_platform_open(br_file *file, br_string_view path, br_file_open_options options) {
  BY_HANDLE_FILE_INFORMATION info;
  HANDLE heap;
  HANDLE handle;
  HANDLE positioned_handle;
  WCHAR *native_path;
  br_error error;
  DWORD access;
  DWORD share;

  heap = GetProcessHeap();
  if (heap == NULL) {
    return br_error_make(BR_STATUS_OUT_OF_MEMORY);
  }

  error = br__file_windows_path(heap, path, &native_path);
  if (error.status != BR_STATUS_OK) {
    return error;
  }

  access = br__file_windows_access(options.flags);
  share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  handle = CreateFileW(native_path,
                       access,
                       share,
                       NULL,
                       br__file_windows_creation(options.flags),
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
                       NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    DWORD native_error;
    DWORD attributes;

    native_error = GetLastError();
    attributes = INVALID_FILE_ATTRIBUTES;
    if (native_error == ERROR_ACCESS_DENIED) {
      attributes = GetFileAttributesW(native_path);
    }
    (void)HeapFree(heap, 0u, native_path);

    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
      return br__os_error_from_win32_status(native_error, BR_STATUS_IS_A_DIRECTORY);
    }
    return br__os_error_from_win32(native_error);
  }

  if (GetFileInformationByHandle(handle, &info) &&
      (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
    (void)CloseHandle(handle);
    (void)HeapFree(heap, 0u, native_path);
    return br_error_make(BR_STATUS_IS_A_DIRECTORY);
  }

  positioned_handle = NULL;
  if (br__file_windows_needs_positioned_handle(options.flags)) {
    positioned_handle = ReOpenFile(handle, access, share, FILE_FLAG_OVERLAPPED);
    if (positioned_handle == INVALID_HANDLE_VALUE) {
      DWORD native_error = GetLastError();

      (void)CloseHandle(handle);
      (void)HeapFree(heap, 0u, native_path);
      return br__os_error_from_win32(native_error);
    }
  }
  (void)HeapFree(heap, 0u, native_path);

  file->handle = (uintptr_t)handle;
  file->positioned_handle = (uintptr_t)positioned_handle;
  file->flags = options.flags;
  return BR_ERROR_OK;
}

br_error br__file_platform_close(br_file *file) {
  HANDLE handle;
  HANDLE positioned_handle;
  br_error error;

  handle = br__file_windows_handle(file);
  positioned_handle = br__file_windows_positioned_handle(file);
  file->handle = 0u;
  file->positioned_handle = 0u;
  file->flags = 0u;

  error = BR_ERROR_OK;
  if (positioned_handle != NULL && !CloseHandle(positioned_handle)) {
    error = br__os_error_from_win32(GetLastError());
  }
  if (!CloseHandle(handle)) {
    DWORD native_error = GetLastError();

    if (error.status == BR_STATUS_OK) {
      error = br__os_error_from_win32(native_error);
    }
  }
  return error;
}

br_i64_result br__file_platform_read(br_file *file, void *dst, size_t len) {
  DWORD count;

  count = 0u;
  if (!ReadFile(br__file_windows_handle(file), dst, br__file_windows_chunk(len), &count, NULL)) {
    DWORD native_error = GetLastError();

    return br_i64_result_make_error((int64_t)count, br__os_error_from_win32(native_error));
  }
  if (count == 0u) {
    return br_i64_result_make(0, BR_STATUS_EOF);
  }
  return br_i64_result_make((int64_t)count, BR_STATUS_OK);
}

br_i64_result br__file_platform_read_at(br_file *file, void *dst, size_t len, int64_t offset) {
  HANDLE event;
  OVERLAPPED overlapped;
  DWORD count;
  BOOL started;

  if (offset < 0) {
    return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }

  event = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (event == NULL) {
    return br_i64_result_make_error(0, br__os_error_from_win32(GetLastError()));
  }

  br__file_overlapped_offset(&overlapped, (uint64_t)offset);
  overlapped.hEvent = event;
  count = 0u;
  started = ReadFile(
    br__file_windows_positioned_handle(file), dst, br__file_windows_chunk(len), NULL, &overlapped);
  if (!started) {
    DWORD native_error = GetLastError();

    if (native_error != ERROR_IO_PENDING) {
      (void)CloseHandle(event);
      return br_i64_result_make_error(0, br__os_error_from_win32(native_error));
    }
  }
  if (!GetOverlappedResult(br__file_windows_positioned_handle(file), &overlapped, &count, TRUE)) {
    DWORD native_error = GetLastError();

    (void)CloseHandle(event);
    return br_i64_result_make_error((int64_t)count, br__os_error_from_win32(native_error));
  }
  (void)CloseHandle(event);
  if (count == 0u) {
    return br_i64_result_make(0, BR_STATUS_EOF);
  }
  return br_i64_result_make((int64_t)count, BR_STATUS_OK);
}

br_i64_result br__file_platform_write(br_file *file, const void *src, size_t len) {
  OVERLAPPED append_offset;
  OVERLAPPED *overlapped;
  DWORD count;

  overlapped = NULL;
  if ((file->flags & BR_FILE_OPEN_APPEND) != 0u) {
    /* Win32 defines an all-ones OVERLAPPED offset as an atomic append. */
    br__file_overlapped_offset(&append_offset, UINT64_MAX);
    overlapped = &append_offset;
  }

  count = 0u;
  if (!WriteFile(
        br__file_windows_handle(file), src, br__file_windows_chunk(len), &count, overlapped)) {
    DWORD native_error = GetLastError();

    return br_i64_result_make_error((int64_t)count, br__os_error_from_win32(native_error));
  }
  return br_i64_result_make((int64_t)count, BR_STATUS_OK);
}

br_i64_result
br__file_platform_write_at(br_file *file, const void *src, size_t len, int64_t offset) {
  HANDLE event;
  OVERLAPPED overlapped;
  DWORD count;
  BOOL started;

  if (offset < 0) {
    return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }

  event = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (event == NULL) {
    return br_i64_result_make_error(0, br__os_error_from_win32(GetLastError()));
  }

  br__file_overlapped_offset(&overlapped, (uint64_t)offset);
  overlapped.hEvent = event;
  count = 0u;
  started = WriteFile(
    br__file_windows_positioned_handle(file), src, br__file_windows_chunk(len), NULL, &overlapped);
  if (!started) {
    DWORD native_error = GetLastError();

    if (native_error != ERROR_IO_PENDING) {
      (void)CloseHandle(event);
      return br_i64_result_make_error(0, br__os_error_from_win32(native_error));
    }
  }
  if (!GetOverlappedResult(br__file_windows_positioned_handle(file), &overlapped, &count, TRUE)) {
    DWORD native_error = GetLastError();

    (void)CloseHandle(event);
    return br_i64_result_make_error((int64_t)count, br__os_error_from_win32(native_error));
  }
  (void)CloseHandle(event);
  return br_i64_result_make((int64_t)count, BR_STATUS_OK);
}

br_i64_result br__file_platform_seek(br_file *file, int64_t offset, br_seek_from whence) {
  LARGE_INTEGER distance;
  LARGE_INTEGER position;
  DWORD native_whence;

  switch (whence) {
    case BR_SEEK_FROM_START:
      native_whence = FILE_BEGIN;
      break;
    case BR_SEEK_FROM_CURRENT:
      native_whence = FILE_CURRENT;
      break;
    case BR_SEEK_FROM_END:
      native_whence = FILE_END;
      break;
    default:
      return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }

  distance.QuadPart = offset;
  if (!SetFilePointerEx(br__file_windows_handle(file), distance, &position, native_whence)) {
    DWORD native_error = GetLastError();

    if (native_error == ERROR_INVALID_FUNCTION) {
      return br_i64_result_make_error(
        0, br__os_error_from_win32_status(native_error, BR_STATUS_NOT_SEEKABLE));
    }
    return br_i64_result_make_error(0, br__os_error_from_win32(native_error));
  }
  if (position.QuadPart < 0) {
    return br_i64_result_make(0, BR_STATUS_OUT_OF_RANGE);
  }
  return br_i64_result_make((int64_t)position.QuadPart, BR_STATUS_OK);
}

br_i64_result br__file_platform_size(br_file *file) {
  LARGE_INTEGER size;

  if (!GetFileSizeEx(br__file_windows_handle(file), &size)) {
    DWORD native_error = GetLastError();

    return br_i64_result_make_error(0, br__os_error_from_win32(native_error));
  }
  if (size.QuadPart < 0) {
    return br_i64_result_make(0, BR_STATUS_OUT_OF_RANGE);
  }
  return br_i64_result_make((int64_t)size.QuadPart, BR_STATUS_OK);
}

static br_error br__file_windows_transfer_kind(HANDLE handle, DWORD *kind) {
  SetLastError(NO_ERROR);
  *kind = GetFileType(handle);
  if (*kind == FILE_TYPE_UNKNOWN) {
    DWORD native_error = GetLastError();

    if (native_error != NO_ERROR) {
      return br__os_error_from_win32(native_error);
    }
    return br_error_make(BR_STATUS_NOT_SUPPORTED);
  }
  if (*kind != FILE_TYPE_DISK && *kind != FILE_TYPE_PIPE && *kind != FILE_TYPE_CHAR) {
    return br_error_make(BR_STATUS_NOT_SUPPORTED);
  }
  return BR_ERROR_OK;
}

static br_error br__file_windows_transfer_identity(HANDLE dst, HANDLE src) {
#if _WIN32_WINNT >= 0x0602
  FILE_ID_INFO dst_id = {0};
  FILE_ID_INFO src_id = {0};
  DWORD native_error;

  /*
  ReFS requires the 128-bit ID to avoid collisions between distinct files.
  Compare IDs from the same information class; never mix modern and legacy IDs.
  */
  if (GetFileInformationByHandleEx(src, FileIdInfo, &src_id, (DWORD)sizeof(src_id))) {
    if (GetFileInformationByHandleEx(dst, FileIdInfo, &dst_id, (DWORD)sizeof(dst_id))) {
      if (src_id.VolumeSerialNumber == dst_id.VolumeSerialNumber &&
          memcmp(src_id.FileId.Identifier,
                 dst_id.FileId.Identifier,
                 sizeof(src_id.FileId.Identifier)) == 0) {
        return br_error_make(BR_STATUS_INVALID_ARGUMENT);
      }
      return BR_ERROR_OK;
    }
  }
  native_error = GetLastError();
  switch (native_error) {
    case ERROR_INVALID_PARAMETER:
    case ERROR_INVALID_FUNCTION:
    case ERROR_NOT_SUPPORTED:
    case ERROR_CALL_NOT_IMPLEMENTED:
      /* Older systems/filesystems can still provide the legacy identity. */
      break;
    default:
      return br__os_error_from_win32(native_error);
  }
#endif

  {
    BY_HANDLE_FILE_INFORMATION dst_info = {0};
    BY_HANDLE_FILE_INFORMATION src_info = {0};

    if (!GetFileInformationByHandle(src, &src_info)) {
      return br__os_error_from_win32(GetLastError());
    }
    if (!GetFileInformationByHandle(dst, &dst_info)) {
      return br__os_error_from_win32(GetLastError());
    }
    /* Network providers may return partial information; do not assume identity. */
    if ((src_info.nFileIndexHigh == 0u && src_info.nFileIndexLow == 0u) ||
        (dst_info.nFileIndexHigh == 0u && dst_info.nFileIndexLow == 0u)) {
      return br_error_make(BR_STATUS_NOT_SUPPORTED);
    }
    /*
    Legacy IDs can collide on ReFS. Rejecting equal IDs is conservative: a
    collision can reject distinct files but cannot authorize a same-file copy.
    */
    if (src_info.dwVolumeSerialNumber == dst_info.dwVolumeSerialNumber &&
        src_info.nFileIndexHigh == dst_info.nFileIndexHigh &&
        src_info.nFileIndexLow == dst_info.nFileIndexLow) {
      return br_error_make(BR_STATUS_INVALID_ARGUMENT);
    }
  }
  return BR_ERROR_OK;
}

br__file_transfer_result br__file_platform_transfer(br_file *dst, br_file *src) {
  br__file_transfer_result result = {br_i64_result_make(0, BR_STATUS_OK), false};
  HANDLE dst_handle = br__file_windows_handle(dst);
  HANDLE src_handle = br__file_windows_handle(src);
  DWORD dst_kind;
  DWORD src_kind;
  br_error error;

  /* The common dispatcher has already validated both native file objects. */
  if (dst_handle == src_handle) {
    result.result = br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
    return result;
  }
  error = br__file_windows_transfer_kind(src_handle, &src_kind);
  if (error.status != BR_STATUS_OK) {
    result.result = br_i64_result_make_error(0, error);
    return result;
  }
  error = br__file_windows_transfer_kind(dst_handle, &dst_kind);
  if (error.status != BR_STATUS_OK) {
    result.result = br_i64_result_make_error(0, error);
    return result;
  }
  if (src_kind == FILE_TYPE_DISK && dst_kind == FILE_TYPE_DISK) {
    error = br__file_windows_transfer_identity(dst_handle, src_handle);
    if (error.status != BR_STATUS_OK) {
      result.result = br_i64_result_make_error(0, error);
      return result;
    }
  } else if (src_kind == dst_kind) {
    /*
    The documented file-ID queries exclude pipes and do not establish identity
    for character devices. Without identity, same-kind pairs cannot proceed.
    */
    result.result = br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
    return result;
  }
  /*
  Storage offload/block cloning uses explicit ranges, not sequential cursors
  or atomic append. Continue through the existing ReadFile/WriteFile routines.
  */
  result.fallback = true;
  return result;
}

#endif /* defined(_WIN32) */
