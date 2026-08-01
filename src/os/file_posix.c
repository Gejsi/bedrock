#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include "file_internal.h"

#if !defined(_WIN32)

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <bedrock/mem/alloc.h>

static br_status br__file_status_from_errno(int error) {
  switch (error) {
    case EINVAL:
      return BR_STATUS_INVALID_ARGUMENT;
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

static br_error br__file_errno_error(int error) {
  return br_error_make_native(
    br__file_status_from_errno(error), BR_ERROR_DOMAIN_POSIX_ERRNO, (uint32_t)error);
}

static int br__file_fd(const br_file *file) {
  return (int)(file->handle - 1u);
}

static size_t br__file_posix_chunk(size_t len) {
  return len > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : len;
}

static br_error br__file_posix_path(br_string_view path, char **out) {
  br_alloc_result alloc;

  *out = NULL;
  if (path.len == SIZE_MAX) {
    return br_error_make(BR_STATUS_PATH_TOO_LONG);
  }

  alloc = br_allocator_alloc_uninit(br_allocator_heap(), path.len + 1u, 1u);
  if (alloc.status != BR_STATUS_OK) {
    return br_error_make(alloc.status);
  }

  if (path.len > 0u) {
    memcpy(alloc.ptr, path.data, path.len);
  }
  ((char *)alloc.ptr)[path.len] = '\0';
  *out = (char *)alloc.ptr;
  return BR_ERROR_OK;
}

static void br__file_posix_path_free(char *path, size_t len) {
  BR_UNUSED(br_allocator_free(br_allocator_heap(), path, len + 1u));
}

br_error br__file_platform_open(br_file *file, br_string_view path, br_file_open_options options) {
  char *native_path;
  br_error error;
  int access;
  int native_flags;
  int fd;
  struct stat info;

  error = br__file_posix_path(path, &native_path);
  if (error.status != BR_STATUS_OK) {
    return error;
  }

  if ((options.flags & BR_FILE_OPEN_READ) != 0u && (options.flags & BR_FILE_OPEN_WRITE) != 0u) {
    access = O_RDWR;
  } else if ((options.flags & BR_FILE_OPEN_WRITE) != 0u) {
    access = O_WRONLY;
  } else {
    access = O_RDONLY;
  }

  native_flags = access;
  if ((options.flags & BR_FILE_OPEN_CREATE) != 0u) {
    native_flags |= O_CREAT;
  }
  if ((options.flags & BR_FILE_OPEN_TRUNCATE) != 0u) {
    native_flags |= O_TRUNC;
  }
  if ((options.flags & BR_FILE_OPEN_APPEND) != 0u) {
    native_flags |= O_APPEND;
  }
  if ((options.flags & BR_FILE_OPEN_CREATE_NEW) != 0u) {
    native_flags |= O_EXCL;
  }
#ifdef O_CLOEXEC
  native_flags |= O_CLOEXEC;
#endif

  do {
    fd = open(native_path, native_flags, (mode_t)options.create_permissions);
  } while (fd < 0 && errno == EINTR);

  if (fd < 0) {
    int native_error = errno;
    br__file_posix_path_free(native_path, path.len);
    return br__file_errno_error(native_error);
  }
  br__file_posix_path_free(native_path, path.len);

#ifndef O_CLOEXEC
  {
    int descriptor_flags;

    do {
      descriptor_flags = fcntl(fd, F_GETFD);
    } while (descriptor_flags < 0 && errno == EINTR);
    if (descriptor_flags < 0) {
      int native_error = errno;
      (void)close(fd);
      return br__file_errno_error(native_error);
    }

    while (fcntl(fd, F_SETFD, descriptor_flags | FD_CLOEXEC) < 0) {
      if (errno != EINTR) {
        int native_error = errno;
        (void)close(fd);
        return br__file_errno_error(native_error);
      }
    }
  }
#endif

  while (fstat(fd, &info) < 0) {
    if (errno != EINTR) {
      int native_error = errno;
      (void)close(fd);
      return br__file_errno_error(native_error);
    }
  }
  if (S_ISDIR(info.st_mode)) {
    (void)close(fd);
    return br_error_make(BR_STATUS_IS_A_DIRECTORY);
  }

  file->handle = (uintptr_t)(unsigned int)fd + 1u;
  file->positioned_handle = 0u;
  file->flags = options.flags;
  return BR_ERROR_OK;
}

br_error br__file_platform_close(br_file *file) {
  int fd;
  int result;

  fd = br__file_fd(file);
  file->handle = 0u;
  file->positioned_handle = 0u;
  file->flags = 0u;
  result = close(fd);
  if (result < 0) {
    return br__file_errno_error(errno);
  }
  return BR_ERROR_OK;
}

br_i64_result br__file_platform_read(br_file *file, void *dst, size_t len) {
  ssize_t count;

  do {
    count = read(br__file_fd(file), dst, br__file_posix_chunk(len));
  } while (count < 0 && errno == EINTR);

  if (count < 0) {
    return br_i64_result_make_error(0, br__file_errno_error(errno));
  }
  if (count == 0) {
    return br_i64_result_make(0, BR_STATUS_EOF);
  }
  return br_i64_result_make((int64_t)count, BR_STATUS_OK);
}

br_i64_result br__file_platform_read_at(br_file *file, void *dst, size_t len, int64_t offset) {
  off_t native_offset;
  ssize_t count;

  if (offset < 0) {
    return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }
  native_offset = (off_t)offset;
  if ((int64_t)native_offset != offset) {
    return br_i64_result_make(0, BR_STATUS_OUT_OF_RANGE);
  }

  do {
    count = pread(br__file_fd(file), dst, br__file_posix_chunk(len), native_offset);
  } while (count < 0 && errno == EINTR);

  if (count < 0) {
    return br_i64_result_make_error(0, br__file_errno_error(errno));
  }
  if (count == 0) {
    return br_i64_result_make(0, BR_STATUS_EOF);
  }
  return br_i64_result_make((int64_t)count, BR_STATUS_OK);
}

br_i64_result br__file_platform_write(br_file *file, const void *src, size_t len) {
  ssize_t count;

  do {
    count = write(br__file_fd(file), src, br__file_posix_chunk(len));
  } while (count < 0 && errno == EINTR);

  if (count < 0) {
    return br_i64_result_make_error(0, br__file_errno_error(errno));
  }
  return br_i64_result_make((int64_t)count, BR_STATUS_OK);
}

br_i64_result
br__file_platform_write_at(br_file *file, const void *src, size_t len, int64_t offset) {
  off_t native_offset;
  ssize_t count;

  if (offset < 0) {
    return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }
  native_offset = (off_t)offset;
  if ((int64_t)native_offset != offset) {
    return br_i64_result_make(0, BR_STATUS_OUT_OF_RANGE);
  }

  do {
    count = pwrite(br__file_fd(file), src, br__file_posix_chunk(len), native_offset);
  } while (count < 0 && errno == EINTR);

  if (count < 0) {
    return br_i64_result_make_error(0, br__file_errno_error(errno));
  }
  return br_i64_result_make((int64_t)count, BR_STATUS_OK);
}

br_i64_result br__file_platform_seek(br_file *file, int64_t offset, br_seek_from whence) {
  int native_whence;
  off_t native_offset;
  off_t result;

  switch (whence) {
    case BR_SEEK_FROM_START:
      native_whence = SEEK_SET;
      break;
    case BR_SEEK_FROM_CURRENT:
      native_whence = SEEK_CUR;
      break;
    case BR_SEEK_FROM_END:
      native_whence = SEEK_END;
      break;
    default:
      return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }

  native_offset = (off_t)offset;
  if ((int64_t)native_offset != offset) {
    return br_i64_result_make(0, BR_STATUS_OUT_OF_RANGE);
  }

  do {
    result = lseek(br__file_fd(file), native_offset, native_whence);
  } while (result < 0 && errno == EINTR);

  if (result < 0) {
    return br_i64_result_make_error(0, br__file_errno_error(errno));
  }
  if ((uintmax_t)result > (uintmax_t)INT64_MAX) {
    return br_i64_result_make(0, BR_STATUS_OUT_OF_RANGE);
  }
  return br_i64_result_make((int64_t)result, BR_STATUS_OK);
}

br_i64_result br__file_platform_size(br_file *file) {
  struct stat info;
  int result;

  do {
    result = fstat(br__file_fd(file), &info);
  } while (result < 0 && errno == EINTR);

  if (result < 0) {
    return br_i64_result_make_error(0, br__file_errno_error(errno));
  }
  if (info.st_size < 0 || (uintmax_t)info.st_size > (uintmax_t)INT64_MAX) {
    return br_i64_result_make(0, BR_STATUS_OUT_OF_RANGE);
  }
  return br_i64_result_make((int64_t)info.st_size, BR_STATUS_OK);
}

#endif
