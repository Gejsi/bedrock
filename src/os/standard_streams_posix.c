#include "standard_streams_internal.h"

#if !defined(_WIN32)

#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "error_internal.h"

static int br__standard_stream_fd(br__standard_stream_kind kind) {
  switch (kind) {
    case BR__STANDARD_STREAM_STDIN:
      return STDIN_FILENO;
    case BR__STANDARD_STREAM_STDOUT:
      return STDOUT_FILENO;
    case BR__STANDARD_STREAM_STDERR:
      return STDERR_FILENO;
  }

  return -1;
}

static size_t br__standard_stream_posix_chunk(size_t len) {
  return len > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : len;
}

br_i64_result
br__standard_stream_platform_read(br__standard_stream_kind kind, void *dst, size_t len) {
  ssize_t count;

  do {
    count = read(br__standard_stream_fd(kind), dst, br__standard_stream_posix_chunk(len));
  } while (count < 0 && errno == EINTR);

  if (count < 0) {
    return br_i64_result_make_error(0, br__os_error_from_errno(errno));
  }
  if (count == 0) {
    return br_i64_result_make(0, BR_STATUS_EOF);
  }
  return br_i64_result_make((int64_t)count, BR_STATUS_OK);
}

br_i64_result
br__standard_stream_platform_write(br__standard_stream_kind kind, const void *src, size_t len) {
  ssize_t count;

  do {
    count = write(br__standard_stream_fd(kind), src, br__standard_stream_posix_chunk(len));
  } while (count < 0 && errno == EINTR);

  if (count < 0) {
    return br_i64_result_make_error(0, br__os_error_from_errno(errno));
  }
  return br_i64_result_make((int64_t)count, BR_STATUS_OK);
}

#endif
