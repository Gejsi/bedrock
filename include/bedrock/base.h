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
  BR_STATUS_INVALID_ENCODING
} br_status;

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
