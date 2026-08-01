#include "standard_streams_internal.h"

#if defined(_WIN32)

#include "error_internal.h"

static DWORD br__standard_stream_id(br__standard_stream_kind kind) {
  switch (kind) {
    case BR__STANDARD_STREAM_STDIN:
      return STD_INPUT_HANDLE;
    case BR__STANDARD_STREAM_STDOUT:
      return STD_OUTPUT_HANDLE;
    case BR__STANDARD_STREAM_STDERR:
      return STD_ERROR_HANDLE;
  }

  return STD_INPUT_HANDLE;
}

static br_error br__standard_stream_handle(br__standard_stream_kind kind, HANDLE *out) {
  HANDLE handle;

  SetLastError(ERROR_SUCCESS);
  handle = GetStdHandle(br__standard_stream_id(kind));
  if (handle == NULL) {
    return br__os_error_from_win32_status(ERROR_INVALID_HANDLE, BR_STATUS_INVALID_STATE);
  }
  if (handle == INVALID_HANDLE_VALUE) {
    DWORD error;

    error = GetLastError();
    if (error == ERROR_SUCCESS) {
      error = ERROR_INVALID_HANDLE;
    }
    return br__os_error_from_win32(error);
  }

  *out = handle;
  return BR_ERROR_OK;
}

static DWORD br__standard_stream_windows_chunk(size_t len) {
  return len > (size_t)UINT32_MAX ? (DWORD)UINT32_MAX : (DWORD)len;
}

br_i64_result
br__standard_stream_platform_read(br__standard_stream_kind kind, void *dst, size_t len) {
  br_error error;
  HANDLE handle;
  DWORD count;

  error = br__standard_stream_handle(kind, &handle);
  if (error.status != BR_STATUS_OK) {
    return br_i64_result_make_error(0, error);
  }

  count = 0u;
  if (!ReadFile(handle, dst, br__standard_stream_windows_chunk(len), &count, NULL)) {
    DWORD native_error;

    native_error = GetLastError();
    if (native_error == ERROR_BROKEN_PIPE || native_error == ERROR_HANDLE_EOF) {
      return br_i64_result_make_error((int64_t)count,
                                      br__os_error_from_win32_status(native_error, BR_STATUS_EOF));
    }
    return br_i64_result_make_error((int64_t)count, br__os_error_from_win32(native_error));
  }
  if (count == 0u) {
    return br_i64_result_make(0, BR_STATUS_EOF);
  }
  return br_i64_result_make((int64_t)count, BR_STATUS_OK);
}

br_i64_result
br__standard_stream_platform_write(br__standard_stream_kind kind, const void *src, size_t len) {
  br_error error;
  HANDLE handle;
  DWORD count;

  error = br__standard_stream_handle(kind, &handle);
  if (error.status != BR_STATUS_OK) {
    return br_i64_result_make_error(0, error);
  }

  count = 0u;
  if (!WriteFile(handle, src, br__standard_stream_windows_chunk(len), &count, NULL)) {
    return br_i64_result_make_error((int64_t)count, br__os_error_from_win32(GetLastError()));
  }
  return br_i64_result_make((int64_t)count, BR_STATUS_OK);
}

#endif
