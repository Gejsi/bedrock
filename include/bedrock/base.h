#ifndef BEDROCK_BASE_H
#define BEDROCK_BASE_H

#include <string.h>

#include <bedrock/types.h>

#ifdef __cplusplus
#define BR_EXTERN_C_BEGIN extern "C" {
#define BR_EXTERN_C_END }
#else
#define BR_EXTERN_C_BEGIN
#define BR_EXTERN_C_END
#endif

#define BR_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define BR_CONCAT_INNER(a, b) a##b
#define BR_CONCAT(a, b) BR_CONCAT_INNER(a, b)
#define BR_DEFAULT_ALIGNMENT ((size_t)_Alignof(max_align_t))
#define BR_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#define BR_UNUSED(x) ((void)(x))

typedef enum br_status {
  BR_STATUS_OK = 0,
  BR_STATUS_INVALID_ARGUMENT,
  BR_STATUS_OUT_OF_MEMORY,
  BR_STATUS_NOT_SUPPORTED,
  BR_STATUS_EOF,
  BR_STATUS_UNEXPECTED_EOF,
  BR_STATUS_INVALID_STATE,
  BR_STATUS_SHORT_WRITE,
  BR_STATUS_SHORT_BUFFER,
  BR_STATUS_BUFFER_FULL,
  BR_STATUS_NO_PROGRESS,
  BR_STATUS_INVALID_ENCODING,
  BR_STATUS_OUT_OF_RANGE,
  BR_STATUS_NOT_FOUND,
  BR_STATUS_PERMISSION_DENIED,
  BR_STATUS_ALREADY_EXISTS,
  BR_STATUS_NOT_A_DIRECTORY,
  BR_STATUS_IS_A_DIRECTORY,
  BR_STATUS_DIRECTORY_NOT_EMPTY,
  BR_STATUS_READ_ONLY_FILESYSTEM,
  BR_STATUS_NO_SPACE,
  BR_STATUS_QUOTA_EXCEEDED,
  BR_STATUS_FILE_TOO_LARGE,
  BR_STATUS_TOO_MANY_OPEN_FILES,
  BR_STATUS_RESOURCE_EXHAUSTED,
  BR_STATUS_PATH_TOO_LONG,
  BR_STATUS_BUSY,
  BR_STATUS_CROSS_DEVICE,
  BR_STATUS_BROKEN_PIPE,
  BR_STATUS_WOULD_BLOCK,
  BR_STATUS_TIMED_OUT,
  BR_STATUS_NOT_SEEKABLE,
  BR_STATUS_IO_ERROR
} br_status;

/*
Stable, allocation-free description of a portable status.
*/
const char *br_status_string(br_status status);

typedef enum br_error_domain {
  BR_ERROR_DOMAIN_NONE = 0,
  BR_ERROR_DOMAIN_POSIX_ERRNO,
  BR_ERROR_DOMAIN_WIN32
} br_error_domain;

typedef struct br_native_error {
  br_error_domain domain;
  uint32_t code;
} br_native_error;

typedef struct br_error {
  br_status status;
  br_native_error native;
} br_error;

#define BR_NATIVE_ERROR_NONE ((br_native_error){BR_ERROR_DOMAIN_NONE, 0u})
#define BR_ERROR_OK ((br_error){BR_STATUS_OK, BR_NATIVE_ERROR_NONE})

static inline br_native_error br_native_error_make(br_error_domain domain, uint32_t code) {
  br_native_error error;

  error.domain = domain;
  error.code = code;
  return error;
}

static inline br_error br_error_make(br_status status) {
  br_error error;

  error.status = status;
  error.native = BR_NATIVE_ERROR_NONE;
  return error;
}

static inline br_error
br_error_make_native(br_status status, br_error_domain domain, uint32_t code) {
  br_error error;

  if (status == BR_STATUS_OK) {
    return BR_ERROR_OK;
  }

  error.status = status;
  error.native = br_native_error_make(domain, code);
  return error;
}

static inline bool br_error_is_ok(br_error error) {
  return error.status == BR_STATUS_OK;
}

static inline bool br_is_power_of_two_size(size_t value) {
  return value != 0u && (value & (value - 1u)) == 0u;
}

static inline size_t br_min_size(size_t a, size_t b) {
  return a < b ? a : b;
}

static inline size_t br_max_size(size_t a, size_t b) {
  return a > b ? a : b;
}

#endif
