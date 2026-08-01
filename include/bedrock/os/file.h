#ifndef BEDROCK_OS_FILE_H
#define BEDROCK_OS_FILE_H

#include <bedrock/io/io.h>
#include <bedrock/strings/strings.h>

BR_EXTERN_C_BEGIN

typedef uint32_t br_file_open_flags;

enum {
  BR_FILE_OPEN_READ = 1u << 0,
  BR_FILE_OPEN_WRITE = 1u << 1,
  BR_FILE_OPEN_CREATE = 1u << 2,
  BR_FILE_OPEN_TRUNCATE = 1u << 3,
  BR_FILE_OPEN_APPEND = 1u << 4,
  BR_FILE_OPEN_CREATE_NEW = 1u << 5
};

typedef struct br_file_open_options {
  br_file_open_flags flags;
  uint32_t create_permissions;
} br_file_open_options;

static inline br_file_open_options br_file_open_options_make(br_file_open_flags flags) {
  br_file_open_options options;

  options.flags = flags;
  options.create_permissions = 0666u;
  return options;
}

typedef struct br_file {
  /* Opaque implementation fields. Do not inspect or copy an open handle. */
  uintptr_t handle;
  br_file_open_flags flags;
} br_file;

#define BR_FILE_INIT {0u, 0u}

/*
Open `path` into an inert, initialized file handle.

POSIX paths are opaque bytes except NUL. Windows paths are well-formed WTF-8.
On success, the file owns one native descriptor or handle. On failure it
remains inert. An open file must not be copied.
*/
br_error br_file_open(br_file *file, br_string_view path, br_file_open_options options);

bool br_file_is_open(const br_file *file);

/*
Close and invalidate `file`.

The handle becomes inert even when the native close reports an error. Closing
an inert handle returns `BR_STATUS_INVALID_STATE`.
*/
br_error br_file_close(br_file *file);

/*
Return a stream that borrows `file`. The stream is invalid after the file is
closed or its storage stops existing. Destroying the stream closes the file but
does not free caller-owned storage.
*/
br_stream br_file_as_stream(br_file *file);

BR_EXTERN_C_END

#endif
