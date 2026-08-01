#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <bedrock.h>

static long test_process_id(void) {
#if defined(_WIN32)
  return (long)_getpid();
#else
  return (long)getpid();
#endif
}

static void test_path(char *buffer, size_t cap, const char *suffix) {
  int count;

  count = snprintf(buffer, cap, "bedrock-os-file-%ld-%s.tmp", test_process_id(), suffix);
  assert(count > 0);
  assert((size_t)count < cap);
}

static br_string_view test_path_view(const char *path) {
  return br_string_view_from_cstr(path);
}

static void test_cleanup(const char *path) {
  (void)remove(path);
}

#if defined(_WIN32)
static void test_windows_make_component(char *path, size_t cap, char value, size_t len) {
  size_t path_len;

  path_len = strlen(path);
  assert(path_len + len + 2u <= cap);
  path[path_len] = '/';
  memset(path + path_len + 1u, value, len);
  path[path_len + len + 1u] = '\0';
}

static size_t test_windows_extended_path(const char *path, char *extended, size_t cap) {
  WCHAR input[512];
  WCHAR absolute[512];
  DWORD count;
  size_t len;
  size_t i;

  len = strlen(path);
  assert(len + 1u <= BR_ARRAY_COUNT(input));
  for (i = 0u; i <= len; i += 1u) {
    input[i] = (WCHAR)(unsigned char)path[i];
  }

  count = GetFullPathNameW(input, (DWORD)BR_ARRAY_COUNT(absolute), absolute, NULL);
  assert(count > 0u);
  assert((size_t)count + 5u <= cap);
  assert(count >= 3u);
  assert(absolute[1] == L':');
  assert(absolute[2] == L'\\');

  extended[0] = '\\';
  extended[1] = '\\';
  extended[2] = '?';
  extended[3] = '\\';
  for (i = 0u; i <= (size_t)count; i += 1u) {
    assert((uint16_t)absolute[i] < 0x80u);
    extended[i + 4u] = (char)absolute[i];
  }
  return (size_t)count + 4u;
}
#endif

static void test_file_validation_and_errors(void) {
  br_file file = BR_FILE_INIT;
  br_file_open_options options;
  br_error error;
  char missing[128];
  const char with_nul[] = {'a', '\0', 'b'};

  assert(!br_file_is_open(&file));
  assert(br_file_close(&file).status == BR_STATUS_INVALID_STATE);

  options = br_file_open_options_make(0u);
  assert(br_file_open(&file, BR_STR_LIT("unused"), options).status == BR_STATUS_INVALID_ARGUMENT);

  options = br_file_open_options_make(BR_FILE_OPEN_READ | BR_FILE_OPEN_APPEND);
  assert(br_file_open(&file, BR_STR_LIT("unused"), options).status == BR_STATUS_INVALID_ARGUMENT);

  options = br_file_open_options_make(BR_FILE_OPEN_READ | BR_FILE_OPEN_TRUNCATE);
  assert(br_file_open(&file, BR_STR_LIT("unused"), options).status == BR_STATUS_INVALID_ARGUMENT);

  options = br_file_open_options_make(BR_FILE_OPEN_READ);
  options.create_permissions = 010000u;
  assert(br_file_open(&file, BR_STR_LIT("unused"), options).status == BR_STATUS_INVALID_ARGUMENT);

  options = br_file_open_options_make(BR_FILE_OPEN_READ);
  assert(br_file_open(&file, br_string_view_make(with_nul, sizeof(with_nul)), options).status ==
         BR_STATUS_INVALID_ARGUMENT);
  assert(br_file_open(&file, br_string_view_make(NULL, 1u), options).status ==
         BR_STATUS_INVALID_ARGUMENT);

  test_path(missing, sizeof(missing), "missing");
  test_cleanup(missing);
  error = br_file_open(&file, test_path_view(missing), options);
  assert(error.status == BR_STATUS_NOT_FOUND);
#if defined(_WIN32)
  assert(error.native.domain == BR_ERROR_DOMAIN_WIN32);
#else
  assert(error.native.domain == BR_ERROR_DOMAIN_POSIX_ERRNO);
#endif
  assert(error.native.code != 0u);
  assert(!br_file_is_open(&file));

  error = br_file_open(&file, BR_STR_LIT("."), options);
  assert(error.status == BR_STATUS_IS_A_DIRECTORY);
  assert(!br_file_is_open(&file));
}

static void test_file_create_read_write_and_positioned_io(void) {
  br_file file = BR_FILE_INIT;
  br_file_open_options options;
  br_stream stream;
  br_io_result io_result;
  br_io_seek_result seek_result;
  br_io_size_result size_result;
  br_io_query_result query_result;
  br_error error;
  char path[128];
  char data[8];

  test_path(path, sizeof(path), "rw");
  test_cleanup(path);
  options =
    br_file_open_options_make(BR_FILE_OPEN_READ | BR_FILE_OPEN_WRITE | BR_FILE_OPEN_CREATE_NEW);
  error = br_file_open(&file, test_path_view(path), options);
  assert(error.status == BR_STATUS_OK);
  assert(br_file_is_open(&file));
  assert(br_file_open(&file, test_path_view(path), options).status == BR_STATUS_INVALID_STATE);

  stream = br_file_as_stream(&file);
  query_result = br_query(stream);
  assert(query_result.status == BR_STATUS_OK);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_READ)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_READ_AT)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_WRITE)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_WRITE_AT)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_FLUSH)) == 0u);

  io_result = br_write_full(stream, "abcdef", 6u);
  assert(io_result.count == 6u);
  assert(io_result.status == BR_STATUS_OK);

  size_result = br_size(stream);
  assert(size_result.status == BR_STATUS_OK);
  assert(size_result.size == 6);

  seek_result = br_seek(stream, 2, BR_SEEK_FROM_START);
  assert(seek_result.status == BR_STATUS_OK);
  assert(seek_result.offset == 2);

  memset(data, 0, sizeof(data));
  io_result = br_read_at(stream, data, 2u, 4);
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(data, "ef", 2u) == 0);

  seek_result = br_seek(stream, 0, BR_SEEK_FROM_CURRENT);
  assert(seek_result.status == BR_STATUS_OK);
  assert(seek_result.offset == 2);
  assert(br_read_at(stream, data, 1u, -1).status == BR_STATUS_INVALID_ARGUMENT);
  assert(br_write_at(stream, "!", 1u, -1).status == BR_STATUS_INVALID_ARGUMENT);
  assert(br_read_at(stream, data, 1u, 99).status == BR_STATUS_EOF);
  seek_result = br_seek(stream, 0, BR_SEEK_FROM_CURRENT);
  assert(seek_result.status == BR_STATUS_OK);
  assert(seek_result.offset == 2);

  io_result = br_write_at(stream, "XY", 2u, 1);
  assert(io_result.count == 2u);
  assert(io_result.status == BR_STATUS_OK);
  seek_result = br_seek(stream, 0, BR_SEEK_FROM_CURRENT);
  assert(seek_result.status == BR_STATUS_OK);
  assert(seek_result.offset == 2);

  assert(br_seek(stream, 0, BR_SEEK_FROM_START).status == BR_STATUS_OK);
  memset(data, 0, sizeof(data));
  io_result = br_read_full(stream, data, 6u);
  assert(io_result.count == 6u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(data, "aXYdef", 6u) == 0);

  io_result = br_read(stream, data, 1u);
  assert(io_result.count == 0u);
  assert(io_result.status == BR_STATUS_EOF);
  assert(br_read(stream, NULL, 0u).status == BR_STATUS_OK);
  assert(br_write(stream, NULL, 0u).status == BR_STATUS_OK);

  assert(br_file_close(&file).status == BR_STATUS_OK);
  assert(!br_file_is_open(&file));
  assert(br_file_close(&file).status == BR_STATUS_INVALID_STATE);
  assert(br_read(stream, data, 1u).status == BR_STATUS_INVALID_STATE);

  options = br_file_open_options_make(BR_FILE_OPEN_WRITE | BR_FILE_OPEN_CREATE_NEW);
  error = br_file_open(&file, test_path_view(path), options);
  assert(error.status == BR_STATUS_ALREADY_EXISTS);
  assert(error.native.code != 0u);

  test_cleanup(path);
}

static void test_file_read_only_create(void) {
  br_file file = BR_FILE_INIT;
  br_file_open_options options;
  char path[128];

  test_path(path, sizeof(path), "read-create");
  test_cleanup(path);

  options = br_file_open_options_make(BR_FILE_OPEN_READ | BR_FILE_OPEN_CREATE_NEW);
  assert(br_file_open(&file, test_path_view(path), options).status == BR_STATUS_OK);
  assert(br_file_close(&file).status == BR_STATUS_OK);

  test_cleanup(path);
}

static void test_file_truncate_append_and_capabilities(void) {
  br_file file = BR_FILE_INIT;
  br_file_open_options options;
  br_stream stream;
  br_io_query_result query_result;
  br_io_result io_result;
  char path[128];
  char data[8];

  test_path(path, sizeof(path), "append");
  test_cleanup(path);

  options = br_file_open_options_make(BR_FILE_OPEN_WRITE | BR_FILE_OPEN_CREATE_NEW);
  assert(br_file_open(&file, test_path_view(path), options).status == BR_STATUS_OK);
  stream = br_file_as_stream(&file);
  query_result = br_query(stream);
  assert(query_result.status == BR_STATUS_OK);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_READ)) == 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_WRITE)) != 0u);
  assert(br_read(stream, data, 1u).status == BR_STATUS_NOT_SUPPORTED);
  assert(br_write_full(stream, "base", 4u).status == BR_STATUS_OK);
  assert(br_file_close(&file).status == BR_STATUS_OK);

  options = br_file_open_options_make(BR_FILE_OPEN_WRITE | BR_FILE_OPEN_APPEND);
  assert(br_file_open(&file, test_path_view(path), options).status == BR_STATUS_OK);
  stream = br_file_as_stream(&file);
  query_result = br_query(stream);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_WRITE_AT)) == 0u);
  assert(br_seek(stream, 0, BR_SEEK_FROM_START).status == BR_STATUS_OK);
  assert(br_write_full(stream, "+", 1u).status == BR_STATUS_OK);
  assert(br_write_at(stream, "!", 1u, 0).status == BR_STATUS_INVALID_STATE);
  assert(br_file_close(&file).status == BR_STATUS_OK);

  options = br_file_open_options_make(BR_FILE_OPEN_READ);
  assert(br_file_open(&file, test_path_view(path), options).status == BR_STATUS_OK);
  stream = br_file_as_stream(&file);
  query_result = br_query(stream);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_READ)) != 0u);
  assert((query_result.modes & br_io_mode_bit(BR_IO_MODE_WRITE)) == 0u);
  memset(data, 0, sizeof(data));
  io_result = br_read_full(stream, data, 5u);
  assert(io_result.status == BR_STATUS_OK);
  assert(memcmp(data, "base+", 5u) == 0);
  assert(br_file_close(&file).status == BR_STATUS_OK);

  options = br_file_open_options_make(BR_FILE_OPEN_WRITE | BR_FILE_OPEN_TRUNCATE);
  assert(br_file_open(&file, test_path_view(path), options).status == BR_STATUS_OK);
  assert(br_size(br_file_as_stream(&file)).size == 0);
  assert(br_file_close(&file).status == BR_STATUS_OK);

  test_cleanup(path);
}

static void test_file_stream_destroy(void) {
  br_file file = BR_FILE_INIT;
  br_file_open_options options;
  char path[128];

  test_path(path, sizeof(path), "destroy");
  test_cleanup(path);
  options = br_file_open_options_make(BR_FILE_OPEN_WRITE | BR_FILE_OPEN_CREATE_NEW);
  assert(br_file_open(&file, test_path_view(path), options).status == BR_STATUS_OK);
  assert(br_destroy(br_file_as_stream(&file)).status == BR_STATUS_OK);
  assert(!br_file_is_open(&file));
  test_cleanup(path);
}

static void test_platform_paths(void) {
  br_file file = BR_FILE_INIT;
  br_file_open_options options;

  options = br_file_open_options_make(BR_FILE_OPEN_WRITE | BR_FILE_OPEN_CREATE_NEW);
#if defined(_WIN32)
  {
    const char path[] = "bedrock-wtf8-\xed\xa0\x80.tmp";
    const WCHAR native_path[] = {'b',
                                 'e',
                                 'd',
                                 'r',
                                 'o',
                                 'c',
                                 'k',
                                 '-',
                                 'w',
                                 't',
                                 'f',
                                 '8',
                                 '-',
                                 (WCHAR)0xd800,
                                 '.',
                                 't',
                                 'm',
                                 'p',
                                 0};
    const char malformed[] = "\xed\xa0";

    (void)DeleteFileW(native_path);
    assert(br_file_open(&file, test_path_view(path), options).status == BR_STATUS_OK);
    assert(br_file_close(&file).status == BR_STATUS_OK);
    assert(DeleteFileW(native_path) != 0);

    assert(
      br_file_open(&file, br_string_view_make(malformed, sizeof(malformed) - 1u), options).status ==
      BR_STATUS_INVALID_ENCODING);
  }
#else
  {
#if defined(__APPLE__)
    const char path[] = "bedrock-\xc3\xa4.tmp";
#else
    const char path[] = {
      'b', 'e', 'd', 'r', 'o', 'c', 'k', '-', (char)0xff, '.', 't', 'm', 'p', '\0'};
#endif

    (void)unlink(path);
    assert(br_file_open(&file, test_path_view(path), options).status == BR_STATUS_OK);
    assert(br_write_full(br_file_as_stream(&file), "x", 1u).status == BR_STATUS_OK);
    assert(br_file_close(&file).status == BR_STATUS_OK);
    assert(unlink(path) == 0);
  }
#endif
}

#if defined(_WIN32)
static void test_windows_long_paths(void) {
  br_file file = BR_FILE_INIT;
  br_file_open_options options;
  char base[64];
  char dirs[3][512];
  char path[512];
  char absolute[512];
  char extended[520];
  DWORD count;
  size_t extended_len;
  size_t i;

  count = (DWORD)snprintf(base, sizeof(base), "bedrock-os-file-%ld-long", test_process_id());
  assert(count > 0u);
  assert((size_t)count < sizeof(base));

  (void)RemoveDirectoryA(base);
  assert(CreateDirectoryA(base, NULL) != 0);
  memcpy(path, base, strlen(base) + 1u);
  for (i = 0u; i < BR_ARRAY_COUNT(dirs); i += 1u) {
    test_windows_make_component(path, sizeof(path), (char)('a' + (char)i), 55u);
    memcpy(dirs[i], path, strlen(path) + 1u);
    assert(CreateDirectoryA(path, NULL) != 0);
  }
  test_windows_make_component(path, sizeof(path), 'z', 80u);
  assert(strlen(path) > 248u);

  options = br_file_open_options_make(BR_FILE_OPEN_WRITE | BR_FILE_OPEN_CREATE_NEW);
  assert(br_file_open(&file, test_path_view(path), options).status == BR_STATUS_OK);
  assert(br_write_full(br_file_as_stream(&file), "long", 4u).status == BR_STATUS_OK);
  assert(br_file_close(&file).status == BR_STATUS_OK);

  count = GetFullPathNameA(path, (DWORD)sizeof(absolute), absolute, NULL);
  assert(count > 0u);
  assert((size_t)count < sizeof(absolute));
  options = br_file_open_options_make(BR_FILE_OPEN_READ);
  assert(br_file_open(&file, test_path_view(absolute), options).status == BR_STATUS_OK);
  assert(br_file_close(&file).status == BR_STATUS_OK);

  extended_len = test_windows_extended_path(path, extended, sizeof(extended));
  assert(br_file_open(&file, br_string_view_make(extended, extended_len), options).status ==
         BR_STATUS_OK);
  assert(br_file_close(&file).status == BR_STATUS_OK);

  assert(DeleteFileA(extended) != 0);
  for (i = BR_ARRAY_COUNT(dirs); i > 0u; i -= 1u) {
    assert(RemoveDirectoryA(dirs[i - 1u]) != 0);
  }
  assert(RemoveDirectoryA(base) != 0);
}
#endif

int main(void) {
  test_file_validation_and_errors();
  test_file_create_read_write_and_positioned_io();
  test_file_read_only_create();
  test_file_truncate_append_and_capabilities();
  test_file_stream_destroy();
  test_platform_paths();
#if defined(_WIN32)
  test_windows_long_paths();
#endif
  return 0;
}
