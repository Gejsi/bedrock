#ifndef BEDROCK_OS_STANDARD_STREAMS_INTERNAL_H
#define BEDROCK_OS_STANDARD_STREAMS_INTERNAL_H

#include <bedrock/io/io.h>

typedef enum br__standard_stream_kind {
  BR__STANDARD_STREAM_STDIN = 0,
  BR__STANDARD_STREAM_STDOUT,
  BR__STANDARD_STREAM_STDERR
} br__standard_stream_kind;

br_i64_result
br__standard_stream_platform_read(br__standard_stream_kind kind, void *dst, size_t len);
br_i64_result
br__standard_stream_platform_write(br__standard_stream_kind kind, const void *src, size_t len);

#endif
