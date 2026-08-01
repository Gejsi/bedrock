#include "error_internal.h"

#if defined(_WIN32)

static br_status br__os_status_from_win32(DWORD error) {
  switch (error) {
    case ERROR_INVALID_PARAMETER:
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_NEGATIVE_SEEK:
      return BR_STATUS_INVALID_ARGUMENT;
    case ERROR_INVALID_HANDLE:
      return BR_STATUS_INVALID_STATE;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_BAD_NETPATH:
    case ERROR_BAD_NET_NAME:
      return BR_STATUS_NOT_FOUND;
    case ERROR_ACCESS_DENIED:
    case ERROR_CANNOT_MAKE:
    case ERROR_PRIVILEGE_NOT_HELD:
      return BR_STATUS_PERMISSION_DENIED;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
      return BR_STATUS_ALREADY_EXISTS;
    case ERROR_DIRECTORY:
      return BR_STATUS_NOT_A_DIRECTORY;
#ifdef ERROR_DIRECTORY_NOT_SUPPORTED
    case ERROR_DIRECTORY_NOT_SUPPORTED:
      return BR_STATUS_IS_A_DIRECTORY;
#endif
    case ERROR_DIR_NOT_EMPTY:
      return BR_STATUS_DIRECTORY_NOT_EMPTY;
    case ERROR_WRITE_PROTECT:
      return BR_STATUS_READ_ONLY_FILESYSTEM;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
      return BR_STATUS_NO_SPACE;
#ifdef ERROR_DISK_QUOTA_EXCEEDED
    case ERROR_DISK_QUOTA_EXCEEDED:
      return BR_STATUS_QUOTA_EXCEEDED;
#endif
#ifdef ERROR_FILE_TOO_LARGE
    case ERROR_FILE_TOO_LARGE:
      return BR_STATUS_FILE_TOO_LARGE;
#endif
    case ERROR_TOO_MANY_OPEN_FILES:
      return BR_STATUS_TOO_MANY_OPEN_FILES;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
      return BR_STATUS_OUT_OF_MEMORY;
    case ERROR_NO_SYSTEM_RESOURCES:
    case ERROR_NOT_ENOUGH_QUOTA:
      return BR_STATUS_RESOURCE_EXHAUSTED;
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_BUFFER_OVERFLOW:
      return BR_STATUS_PATH_TOO_LONG;
    case ERROR_BUSY:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
    case ERROR_PIPE_BUSY:
      return BR_STATUS_BUSY;
    case ERROR_NOT_SAME_DEVICE:
      return BR_STATUS_CROSS_DEVICE;
    case ERROR_BROKEN_PIPE:
    case ERROR_NO_DATA:
      return BR_STATUS_BROKEN_PIPE;
    case ERROR_IO_PENDING:
      return BR_STATUS_WOULD_BLOCK;
    case ERROR_SEM_TIMEOUT:
#ifdef ERROR_TIMEOUT
    case ERROR_TIMEOUT:
#endif
      return BR_STATUS_TIMED_OUT;
    case ERROR_SEEK_ON_DEVICE:
      return BR_STATUS_NOT_SEEKABLE;
    case ERROR_INVALID_FUNCTION:
    case ERROR_NOT_SUPPORTED:
    case ERROR_CALL_NOT_IMPLEMENTED:
      return BR_STATUS_NOT_SUPPORTED;
    case ERROR_ARITHMETIC_OVERFLOW:
      return BR_STATUS_OUT_OF_RANGE;
    case ERROR_HANDLE_EOF:
      return BR_STATUS_EOF;
    default:
      return BR_STATUS_IO_ERROR;
  }
}

br_error br__os_error_from_win32(DWORD error) {
  return br_error_make_native(
    br__os_status_from_win32(error), BR_ERROR_DOMAIN_WIN32, (uint32_t)error);
}

br_error br__os_error_from_win32_status(DWORD error, br_status status) {
  return br_error_make_native(status, BR_ERROR_DOMAIN_WIN32, (uint32_t)error);
}

#endif
