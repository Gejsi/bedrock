#include <bedrock/os/file.h>

#include <string.h>

#include "../io/io_internal.h"
#include "file_internal.h"

#if defined(_MSC_VER)
#define BR__FILE_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define BR__FILE_NOINLINE __attribute__((noinline))
#else
#define BR__FILE_NOINLINE
#endif

#define BR__FILE_OPEN_ALL                                                                          \
  ((br_file_open_flags)(BR_FILE_OPEN_READ | BR_FILE_OPEN_WRITE | BR_FILE_OPEN_CREATE |             \
                        BR_FILE_OPEN_TRUNCATE | BR_FILE_OPEN_APPEND | BR_FILE_OPEN_CREATE_NEW))

static bool br__file_path_has_nul(br_string_view path) {
  size_t i;

  for (i = 0u; i < path.len; i += 1u) {
    if (path.data[i] == '\0') {
      return true;
    }
  }
  return false;
}

bool br_file_is_open(const br_file *file) {
  return file != NULL && file->handle != 0u;
}

br_error br_file_open(br_file *file, br_string_view path, br_file_open_options options) {
  br_file_open_flags flags;
  br_error error;

  if (file == NULL || (path.data == NULL && path.len > 0u)) {
    return br_error_make(BR_STATUS_INVALID_ARGUMENT);
  }
  if (br_file_is_open(file)) {
    return br_error_make(BR_STATUS_INVALID_STATE);
  }
  if (br__file_path_has_nul(path)) {
    return br_error_make(BR_STATUS_INVALID_ARGUMENT);
  }

  flags = options.flags;
  if ((flags & ~BR__FILE_OPEN_ALL) != 0u ||
      (flags & (BR_FILE_OPEN_READ | BR_FILE_OPEN_WRITE)) == 0u) {
    return br_error_make(BR_STATUS_INVALID_ARGUMENT);
  }
  if ((flags & (BR_FILE_OPEN_TRUNCATE | BR_FILE_OPEN_APPEND)) != 0u &&
      (flags & BR_FILE_OPEN_WRITE) == 0u) {
    return br_error_make(BR_STATUS_INVALID_ARGUMENT);
  }
  if (options.create_permissions > 07777u) {
    return br_error_make(BR_STATUS_INVALID_ARGUMENT);
  }

  if ((flags & BR_FILE_OPEN_CREATE_NEW) != 0u) {
    flags |= BR_FILE_OPEN_CREATE;
  }

  file->handle = 0u;
  file->positioned_handle = 0u;
  file->flags = 0u;
  options.flags = flags;
  error = br__file_platform_open(file, path, options);
  if (error.status != BR_STATUS_OK) {
    file->handle = 0u;
    file->positioned_handle = 0u;
    file->flags = 0u;
  }
  return error;
}

br_error br_file_close(br_file *file) {
  if (!br_file_is_open(file)) {
    return br_error_make(BR_STATUS_INVALID_STATE);
  }
  return br__file_platform_close(file);
}

static br_i64_result br__file_stream_proc(
  void *context, br_io_mode mode, void *data, size_t data_len, int64_t offset, br_seek_from whence);

static BR__FILE_NOINLINE br_i64_result br__file_transfer_fallback(br_file *dst,
                                                                  br_file *src,
                                                                  int64_t written) {
  uint8_t buffer[4096];

  return br__io_copy_buffer(
    br_file_as_stream(dst), br_file_as_stream(src), buffer, sizeof(buffer), written);
}

static br_i64_result br__file_transfer(br_file *file, void *data, size_t data_len, bool write_to) {
  br_io_transfer_request request;
  br__file_transfer_result native;
  br_i64_result result;
  br_file *peer;
  br_file *dst;
  br_file *src;

  if (data == NULL || data_len != sizeof(request)) {
    return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }
  memcpy(&request, data, sizeof(request));
  if (!br_stream_is_valid(request.peer)) {
    return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }
  if (request.peer.procedure != br__file_stream_proc) {
    return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }

  peer = (br_file *)request.peer.context;
  if (!br_file_is_open(peer)) {
    return br_i64_result_make(0, BR_STATUS_INVALID_STATE);
  }
  dst = write_to ? peer : file;
  src = write_to ? file : peer;
  if (dst == src || (src->flags & BR_FILE_OPEN_READ) == 0u ||
      (dst->flags & BR_FILE_OPEN_WRITE) == 0u) {
    return br_i64_result_make(0, BR_STATUS_INVALID_ARGUMENT);
  }

  native = br__file_platform_transfer(dst, src);
  result = native.result;
  if (result.value < 0 || (native.fallback && result.status != BR_STATUS_OK)) {
    return br_i64_result_make(0, BR_STATUS_INVALID_STATE);
  }
  if (native.fallback) {
    result = br__file_transfer_fallback(dst, src, result.value);
  }
  /*
  A recognized file pair has already entered its transfer operation. Unsupported
  identity or I/O errors are terminal, even when no output was accepted: an
  outer probe must not bypass identity checks or reconsume advanced input.
  */
  if (result.status == BR_STATUS_NOT_SUPPORTED) {
    result.status = BR_STATUS_IO_ERROR;
  }
  return result;
}

static br_i64_result br__file_stream_proc(void *context,
                                          br_io_mode mode,
                                          void *data,
                                          size_t data_len,
                                          int64_t offset,
                                          br_seek_from whence) {
  br_file *file;
  br_io_mode_set modes;
  bool can_read;
  bool can_write;

  file = (br_file *)context;
  if (!br_file_is_open(file)) {
    if (mode == BR_IO_MODE_DESTROY) {
      return br_i64_result_make(0, BR_STATUS_OK);
    }
    return br_i64_result_make(0, BR_STATUS_INVALID_STATE);
  }

  can_read = (file->flags & BR_FILE_OPEN_READ) != 0u;
  can_write = (file->flags & BR_FILE_OPEN_WRITE) != 0u;

  switch (mode) {
    case BR_IO_MODE_CLOSE:
      return br_i64_result_make_error(0, br_file_close(file));
    case BR_IO_MODE_READ:
      if (!can_read) {
        return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
      }
      if (data_len == 0u) {
        return br_i64_result_make(0, BR_STATUS_OK);
      }
      return br__file_platform_read(file, data, data_len);
    case BR_IO_MODE_READ_AT:
      if (!can_read) {
        return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
      }
      if (data_len == 0u) {
        return br_i64_result_make(0, BR_STATUS_OK);
      }
      return br__file_platform_read_at(file, data, data_len, offset);
    case BR_IO_MODE_WRITE:
      if (!can_write) {
        return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
      }
      if (data_len == 0u) {
        return br_i64_result_make(0, BR_STATUS_OK);
      }
      return br__file_platform_write(file, data, data_len);
    case BR_IO_MODE_WRITE_AT:
      if (!can_write) {
        return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
      }
      if ((file->flags & BR_FILE_OPEN_APPEND) != 0u) {
        return br_i64_result_make(0, BR_STATUS_INVALID_STATE);
      }
      if (data_len == 0u) {
        return br_i64_result_make(0, BR_STATUS_OK);
      }
      return br__file_platform_write_at(file, data, data_len, offset);
    case BR_IO_MODE_SEEK:
      return br__file_platform_seek(file, offset, whence);
    case BR_IO_MODE_SIZE:
      return br__file_platform_size(file);
    case BR_IO_MODE_WRITE_TO:
      if (!can_read) {
        return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
      }
      return br__file_transfer(file, data, data_len, true);
    case BR_IO_MODE_READ_FROM:
      if (!can_write) {
        return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
      }
      return br__file_transfer(file, data, data_len, false);
    case BR_IO_MODE_DESTROY:
      return br_i64_result_make(0, BR_STATUS_OK);
    case BR_IO_MODE_QUERY:
      modes = br_io_mode_bit(BR_IO_MODE_CLOSE) | br_io_mode_bit(BR_IO_MODE_SEEK) |
              br_io_mode_bit(BR_IO_MODE_SIZE) | br_io_mode_bit(BR_IO_MODE_DESTROY);
      if (can_read) {
        modes |= br_io_mode_bit(BR_IO_MODE_READ) | br_io_mode_bit(BR_IO_MODE_READ_AT) |
                 br_io_mode_bit(BR_IO_MODE_WRITE_TO);
      }
      if (can_write) {
        modes |= br_io_mode_bit(BR_IO_MODE_WRITE) | br_io_mode_bit(BR_IO_MODE_READ_FROM);
        if ((file->flags & BR_FILE_OPEN_APPEND) == 0u) {
          modes |= br_io_mode_bit(BR_IO_MODE_WRITE_AT);
        }
      }
      return br_stream_query_utility(modes);
    case BR_IO_MODE_FLUSH:
    case BR_IO_MODE_COUNT:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }

  return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
}

br_stream br_file_as_stream(br_file *file) {
  return br_stream_make(file, br__file_stream_proc);
}
