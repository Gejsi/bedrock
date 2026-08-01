#include <assert.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

#include <bedrock.h>

static void test_standard_stream_modes(void) {
  br_stream input;
  br_stream output;
  br_stream error_output;
  br_io_query_result query;
  uint8_t byte;

  input = br_stdin();
  output = br_stdout();
  error_output = br_stderr();
  assert(br_stream_is_valid(input));
  assert(br_stream_is_valid(output));
  assert(br_stream_is_valid(error_output));

  query = br_query(input);
  assert(query.status == BR_STATUS_OK);
  assert(query.modes == (br_io_mode_bit(BR_IO_MODE_READ) | br_io_mode_bit(BR_IO_MODE_DESTROY) |
                         br_io_mode_bit(BR_IO_MODE_QUERY)));

  query = br_query(output);
  assert(query.status == BR_STATUS_OK);
  assert(query.modes == (br_io_mode_bit(BR_IO_MODE_WRITE) | br_io_mode_bit(BR_IO_MODE_DESTROY) |
                         br_io_mode_bit(BR_IO_MODE_QUERY)));

  query = br_query(error_output);
  assert(query.status == BR_STATUS_OK);
  assert(query.modes == (br_io_mode_bit(BR_IO_MODE_WRITE) | br_io_mode_bit(BR_IO_MODE_DESTROY) |
                         br_io_mode_bit(BR_IO_MODE_QUERY)));

  assert(br_write(input, "x", 1u).status == BR_STATUS_NOT_SUPPORTED);
  assert(br_read(output, &byte, 1u).status == BR_STATUS_NOT_SUPPORTED);
  assert(br_read(error_output, &byte, 1u).status == BR_STATUS_NOT_SUPPORTED);
  assert(br_read(input, NULL, 0u).status == BR_STATUS_OK);
  assert(br_write(output, NULL, 0u).status == BR_STATUS_OK);
  assert(br_write(error_output, NULL, 0u).status == BR_STATUS_OK);
  assert(br_close(input).status == BR_STATUS_NOT_SUPPORTED);
  assert(br_close(output).status == BR_STATUS_NOT_SUPPORTED);
  assert(br_flush(output).status == BR_STATUS_NOT_SUPPORTED);
  assert(br_destroy(input).status == BR_STATUS_OK);
  assert(br_destroy(output).status == BR_STATUS_OK);
  assert(br_destroy(error_output).status == BR_STATUS_OK);
}

#if defined(_WIN32)

static void test_native_write_all(HANDLE handle, const void *data, DWORD len) {
  const uint8_t *cursor;
  DWORD total;

  cursor = (const uint8_t *)data;
  total = 0u;
  while (total < len) {
    DWORD count;

    count = 0u;
    assert(WriteFile(handle, cursor + total, len - total, &count, NULL) != 0);
    assert(count > 0u);
    total += count;
  }
}

static void test_native_read_all(HANDLE handle, void *data, DWORD len) {
  uint8_t *cursor;
  DWORD total;

  cursor = (uint8_t *)data;
  total = 0u;
  while (total < len) {
    DWORD count;

    count = 0u;
    assert(ReadFile(handle, cursor + total, len - total, &count, NULL) != 0);
    assert(count > 0u);
    total += count;
  }
}

static void test_standard_input_redirection(void) {
  static const char input[] = "redirected stdin";
  br_stream stream;
  br_io_result result;
  HANDLE original;
  HANDLE read_handle;
  HANDLE write_handle;
  char buffer[sizeof(input) - 1u];

  stream = br_stdin();
  original = GetStdHandle(STD_INPUT_HANDLE);
  assert(CreatePipe(&read_handle, &write_handle, NULL, 0u) != 0);
  test_native_write_all(write_handle, input, (DWORD)(sizeof(input) - 1u));
  assert(CloseHandle(write_handle) != 0);
  assert(SetStdHandle(STD_INPUT_HANDLE, read_handle) != 0);

  result = br_read_full(stream, buffer, sizeof(buffer));
  assert(result.status == BR_STATUS_OK);
  assert(result.count == sizeof(buffer));
  assert(memcmp(buffer, input, sizeof(buffer)) == 0);
  result = br_read(stream, buffer, 1u);
  assert(result.status == BR_STATUS_EOF);

  assert(SetStdHandle(STD_INPUT_HANDLE, original) != 0);
  assert(CloseHandle(read_handle) != 0);
}

static void test_standard_output_redirection(DWORD standard_id, br_stream stream) {
  static const char output[] = "redirected output";
  br_io_result result;
  HANDLE original;
  HANDLE read_handle;
  HANDLE write_handle;
  char buffer[sizeof(output) - 1u];

  original = GetStdHandle(standard_id);
  assert(CreatePipe(&read_handle, &write_handle, NULL, 0u) != 0);
  assert(SetStdHandle(standard_id, write_handle) != 0);

  result = br_write_full(stream, output, sizeof(output) - 1u);
  assert(result.status == BR_STATUS_OK);
  assert(result.count == sizeof(output) - 1u);

  assert(SetStdHandle(standard_id, original) != 0);
  assert(CloseHandle(write_handle) != 0);
  test_native_read_all(read_handle, buffer, (DWORD)sizeof(buffer));
  assert(memcmp(buffer, output, sizeof(buffer)) == 0);
  assert(CloseHandle(read_handle) != 0);
}

static void test_missing_standard_input(void) {
  br_io_result result;
  HANDLE original;
  uint8_t byte;

  original = GetStdHandle(STD_INPUT_HANDLE);
  assert(SetStdHandle(STD_INPUT_HANDLE, NULL) != 0);
  result = br_read(br_stdin(), &byte, 1u);
  assert(SetStdHandle(STD_INPUT_HANDLE, original) != 0);

  assert(result.status == BR_STATUS_INVALID_STATE);
  assert(result.native_error.domain == BR_ERROR_DOMAIN_WIN32);
  assert(result.native_error.code == (uint32_t)ERROR_INVALID_HANDLE);
}

#else

static void test_native_write_all(int fd, const void *data, size_t len) {
  const uint8_t *cursor;
  size_t total;

  cursor = (const uint8_t *)data;
  total = 0u;
  while (total < len) {
    ssize_t count;

    count = write(fd, cursor + total, len - total);
    assert(count > 0);
    total += (size_t)count;
  }
}

static void test_native_read_all(int fd, void *data, size_t len) {
  uint8_t *cursor;
  size_t total;

  cursor = (uint8_t *)data;
  total = 0u;
  while (total < len) {
    ssize_t count;

    count = read(fd, cursor + total, len - total);
    assert(count > 0);
    total += (size_t)count;
  }
}

static void test_standard_input_redirection(void) {
  static const char input[] = "redirected stdin";
  br_stream stream;
  br_io_result result;
  int original;
  int pipe_fds[2];
  char buffer[sizeof(input) - 1u];

  stream = br_stdin();
  original = dup(STDIN_FILENO);
  assert(original >= 0);
  assert(pipe(pipe_fds) == 0);
  test_native_write_all(pipe_fds[1], input, sizeof(input) - 1u);
  assert(close(pipe_fds[1]) == 0);
  assert(dup2(pipe_fds[0], STDIN_FILENO) == STDIN_FILENO);
  assert(close(pipe_fds[0]) == 0);

  result = br_read_full(stream, buffer, sizeof(buffer));
  assert(result.status == BR_STATUS_OK);
  assert(result.count == sizeof(buffer));
  assert(memcmp(buffer, input, sizeof(buffer)) == 0);
  result = br_read(stream, buffer, 1u);
  assert(result.status == BR_STATUS_EOF);

  assert(dup2(original, STDIN_FILENO) == STDIN_FILENO);
  assert(close(original) == 0);
}

static void test_standard_output_redirection(int standard_fd, br_stream stream) {
  static const char output[] = "redirected output";
  br_io_result result;
  int original;
  int pipe_fds[2];
  char buffer[sizeof(output) - 1u];

  original = dup(standard_fd);
  assert(original >= 0);
  assert(pipe(pipe_fds) == 0);
  assert(dup2(pipe_fds[1], standard_fd) == standard_fd);
  assert(close(pipe_fds[1]) == 0);

  result = br_write_full(stream, output, sizeof(output) - 1u);
  assert(result.status == BR_STATUS_OK);
  assert(result.count == sizeof(output) - 1u);

  assert(dup2(original, standard_fd) == standard_fd);
  assert(close(original) == 0);
  test_native_read_all(pipe_fds[0], buffer, sizeof(buffer));
  assert(memcmp(buffer, output, sizeof(buffer)) == 0);
  assert(close(pipe_fds[0]) == 0);
}

static void test_missing_standard_input(void) {
  br_io_result result;
  int original;
  uint8_t byte;

  original = dup(STDIN_FILENO);
  assert(original >= 0);
  assert(close(STDIN_FILENO) == 0);
  result = br_read(br_stdin(), &byte, 1u);
  assert(dup2(original, STDIN_FILENO) == STDIN_FILENO);
  assert(close(original) == 0);

  assert(result.status == BR_STATUS_INVALID_STATE);
  assert(result.native_error.domain == BR_ERROR_DOMAIN_POSIX_ERRNO);
  assert(result.native_error.code == (uint32_t)EBADF);
}

#endif

int main(void) {
  test_standard_stream_modes();
  test_standard_input_redirection();
#if defined(_WIN32)
  test_standard_output_redirection(STD_OUTPUT_HANDLE, br_stdout());
  test_standard_output_redirection(STD_ERROR_HANDLE, br_stderr());
#else
  test_standard_output_redirection(STDOUT_FILENO, br_stdout());
  test_standard_output_redirection(STDERR_FILENO, br_stderr());
#endif
  test_missing_standard_input();
  return 0;
}
