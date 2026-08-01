#include <bedrock/base.h>

const char *br_status_string(br_status status) {
  switch (status) {
    case BR_STATUS_OK:
      return "ok";
    case BR_STATUS_INVALID_ARGUMENT:
      return "invalid argument";
    case BR_STATUS_OUT_OF_MEMORY:
      return "out of memory";
    case BR_STATUS_NOT_SUPPORTED:
      return "not supported";
    case BR_STATUS_EOF:
      return "end of file";
    case BR_STATUS_UNEXPECTED_EOF:
      return "unexpected end of file";
    case BR_STATUS_INVALID_STATE:
      return "invalid state";
    case BR_STATUS_SHORT_WRITE:
      return "short write";
    case BR_STATUS_SHORT_BUFFER:
      return "short buffer";
    case BR_STATUS_BUFFER_FULL:
      return "buffer full";
    case BR_STATUS_NO_PROGRESS:
      return "no progress";
    case BR_STATUS_INVALID_ENCODING:
      return "invalid encoding";
    case BR_STATUS_OUT_OF_RANGE:
      return "out of range";
    case BR_STATUS_NOT_FOUND:
      return "not found";
    case BR_STATUS_PERMISSION_DENIED:
      return "permission denied";
    case BR_STATUS_ALREADY_EXISTS:
      return "already exists";
    case BR_STATUS_NOT_A_DIRECTORY:
      return "not a directory";
    case BR_STATUS_IS_A_DIRECTORY:
      return "is a directory";
    case BR_STATUS_DIRECTORY_NOT_EMPTY:
      return "directory not empty";
    case BR_STATUS_READ_ONLY_FILESYSTEM:
      return "read-only filesystem";
    case BR_STATUS_NO_SPACE:
      return "no space";
    case BR_STATUS_QUOTA_EXCEEDED:
      return "quota exceeded";
    case BR_STATUS_FILE_TOO_LARGE:
      return "file too large";
    case BR_STATUS_TOO_MANY_OPEN_FILES:
      return "too many open files";
    case BR_STATUS_RESOURCE_EXHAUSTED:
      return "resource exhausted";
    case BR_STATUS_PATH_TOO_LONG:
      return "path too long";
    case BR_STATUS_BUSY:
      return "busy";
    case BR_STATUS_CROSS_DEVICE:
      return "cross-device operation";
    case BR_STATUS_BROKEN_PIPE:
      return "broken pipe";
    case BR_STATUS_WOULD_BLOCK:
      return "operation would block";
    case BR_STATUS_TIMED_OUT:
      return "timed out";
    case BR_STATUS_NOT_SEEKABLE:
      return "not seekable";
    case BR_STATUS_IO_ERROR:
      return "I/O error";
    default:
      return "unknown status";
  }
}
