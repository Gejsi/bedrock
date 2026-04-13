#ifndef BEDROCK_TYPES_H
#define BEDROCK_TYPES_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef BEDROCK_NO_SHORT_TYPES
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef intptr_t iptr;
typedef uintptr_t uptr;

typedef ptrdiff_t isize;
typedef size_t usize;

typedef float f32;
typedef double f64;
#endif

#endif
