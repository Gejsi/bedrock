#ifndef BEDROCK_IO_INTERNAL_H
#define BEDROCK_IO_INTERNAL_H

#include <bedrock/io/io.h>

/*
Continue an ordinary buffered copy after `written` output bytes were already
accepted. Its returned count includes those bytes, and the combined operation
shares the public copy helpers' INT64_MAX limit.
*/
br_i64_result
br__io_copy_buffer(br_stream dst, br_stream src, void *buffer, size_t buffer_len, int64_t written);

#endif
