#include "error_internal.h"

#if !defined(_WIN32)

#include <errno.h>

static br_status br__os_status_from_errno(int error) {
  switch (error) {
    case EINVAL:
      return BR_STATUS_INVALID_ARGUMENT;
    case EBADF:
      return BR_STATUS_INVALID_STATE;
    case ENOENT:
      return BR_STATUS_NOT_FOUND;
    case EACCES:
    case EPERM:
      return BR_STATUS_PERMISSION_DENIED;
    case EEXIST:
      return BR_STATUS_ALREADY_EXISTS;
    case ENOTDIR:
      return BR_STATUS_NOT_A_DIRECTORY;
#ifdef EISDIR
    case EISDIR:
      return BR_STATUS_IS_A_DIRECTORY;
#endif
#ifdef ENOTEMPTY
    case ENOTEMPTY:
      return BR_STATUS_DIRECTORY_NOT_EMPTY;
#endif
    case EROFS:
      return BR_STATUS_READ_ONLY_FILESYSTEM;
    case ENOSPC:
      return BR_STATUS_NO_SPACE;
#ifdef EDQUOT
    case EDQUOT:
      return BR_STATUS_QUOTA_EXCEEDED;
#endif
    case EFBIG:
      return BR_STATUS_FILE_TOO_LARGE;
    case EMFILE:
    case ENFILE:
      return BR_STATUS_TOO_MANY_OPEN_FILES;
    case ENOMEM:
      return BR_STATUS_RESOURCE_EXHAUSTED;
    case ENAMETOOLONG:
      return BR_STATUS_PATH_TOO_LONG;
    case EBUSY:
#if defined(ETXTBSY) && ETXTBSY != EBUSY
    case ETXTBSY:
#endif
      return BR_STATUS_BUSY;
    case EXDEV:
      return BR_STATUS_CROSS_DEVICE;
    case EPIPE:
      return BR_STATUS_BROKEN_PIPE;
    case EAGAIN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
      return BR_STATUS_WOULD_BLOCK;
#ifdef ETIMEDOUT
    case ETIMEDOUT:
      return BR_STATUS_TIMED_OUT;
#endif
    case ESPIPE:
      return BR_STATUS_NOT_SEEKABLE;
#ifdef EOVERFLOW
    case EOVERFLOW:
      return BR_STATUS_OUT_OF_RANGE;
#endif
#ifdef ENOSYS
    case ENOSYS:
      return BR_STATUS_NOT_SUPPORTED;
#endif
#ifdef ENOTSUP
    case ENOTSUP:
      return BR_STATUS_NOT_SUPPORTED;
#endif
#if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || EOPNOTSUPP != ENOTSUP)
    case EOPNOTSUPP:
      return BR_STATUS_NOT_SUPPORTED;
#endif
    default:
      return BR_STATUS_IO_ERROR;
  }
}

br_error br__os_error_from_errno(int error) {
  return br_error_make_native(
    br__os_status_from_errno(error), BR_ERROR_DOMAIN_POSIX_ERRNO, (uint32_t)error);
}

#endif
