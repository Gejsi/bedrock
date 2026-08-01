#ifndef BEDROCK_OS_FILE_INTERNAL_H
#define BEDROCK_OS_FILE_INTERNAL_H

#include <bedrock/os/file.h>

br_error br__file_platform_open(br_file *file, br_string_view path, br_file_open_options options);
br_error br__file_platform_close(br_file *file);
br_i64_result br__file_platform_read(br_file *file, void *dst, size_t len);
br_i64_result br__file_platform_read_at(br_file *file, void *dst, size_t len, int64_t offset);
br_i64_result br__file_platform_write(br_file *file, const void *src, size_t len);
br_i64_result
br__file_platform_write_at(br_file *file, const void *src, size_t len, int64_t offset);
br_i64_result br__file_platform_seek(br_file *file, int64_t offset, br_seek_from whence);
br_i64_result br__file_platform_size(br_file *file);

#endif
