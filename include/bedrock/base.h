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
#define BR_DEFAULT_ALIGNMENT ((usize) _Alignof(max_align_t))
#define BR_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

typedef enum br_status {
  BR_STATUS_OK = 0,
  BR_STATUS_INVALID_ARGUMENT = 1,
  BR_STATUS_OUT_OF_MEMORY = 2,
  BR_STATUS_NOT_SUPPORTED = 3,
  BR_STATUS_EOF = 4,
  BR_STATUS_INVALID_STATE = 5,
  BR_STATUS_SHORT_WRITE = 6
} br_status;

static inline bool br_is_power_of_two_size(usize value) {
  return value != 0u && (value & (value - 1u)) == 0u;
}

static inline usize br_min_size(usize a, usize b) {
  return a < b ? a : b;
}

#endif
