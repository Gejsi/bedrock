#if defined(_WIN32)
int main(void) {
  return 0;
}
#else

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <bedrock.h>

#if defined(__linux__)
#include <sys/syscall.h>
#endif

/*
Evidence-only build options. The normal Linux build uses its actual syscall
number. On the macOS evidence host, all native headers are loaded before the
Linux source branch is selected; the syscall remains a TU-local simulation.
No option or shim is added to production source or process-global libc.
*/
#if defined(BR_TEST_LINUX_SHIM) && !defined(__linux__)
#include <sys/syscall.h>
#define __linux__ 1
#define SYS_copy_file_range 16777215
#define TEST_NATIVE_FORCED_LINUX 1
#endif
#if defined(BR_TEST_NO_COPY_SYSCALL)
#undef SYS_copy_file_range
#endif
#if defined(__linux__) && defined(SYS_copy_file_range)
#define TEST_NATIVE_COPY_ACTIVE 1
#endif
#if defined(BR_TEST_SMALL_TRANSFER_LIMIT)
#undef INT64_MAX
#define INT64_MAX 31
#endif

typedef struct test_native_step {
  long count;
  int error;
} test_native_step;

typedef struct test_native_control {
  int src_fd;
  int dst_fd;
  int stat_fail_fd;
  int stat_error;
  unsigned stat_interrupts;
  int stat_interrupt_fd;
  usize stat_calls;
  int flags_fail_fd;
  int flags_error;
  unsigned flags_interrupts;
  int flags_interrupt_fd;
  usize flags_calls;
  test_native_step steps[8];
  usize step_count;
  usize next_step;
  unsigned long requests[8];
  usize copy_calls;
  bool real_copy;
} test_native_control;

static test_native_control test_native;
static int (*const test_native_real_fstat)(int, struct stat *) = fstat;
#if defined(TEST_NATIVE_COPY_ACTIVE) && !defined(TEST_NATIVE_FORCED_LINUX)
static long (*const test_native_real_syscall)(long, ...) = syscall;
#endif

static int test_native_fstat(int fd, struct stat *info);
#if defined(TEST_NATIVE_COPY_ACTIVE)
static int test_native_fcntl(int fd, int command, ...);
static long test_native_syscall(long number, ...);
#endif

#define br__file_platform_open test_native_platform_open
#define br__file_platform_close test_native_platform_close
#define br__file_platform_read test_native_platform_read
#define br__file_platform_read_at test_native_platform_read_at
#define br__file_platform_write test_native_platform_write
#define br__file_platform_write_at test_native_platform_write_at
#define br__file_platform_seek test_native_platform_seek
#define br__file_platform_size test_native_platform_size
#define br__file_platform_transfer test_native_platform_transfer
#undef fstat
#define fstat test_native_fstat
#if defined(TEST_NATIVE_COPY_ACTIVE)
#define fcntl test_native_fcntl
#define syscall test_native_syscall
#endif

/* -Itests lets the evidence copy resolve the same path as a promoted test TU. */
#include "../src/os/file_posix.c"

#undef fstat
#undef fcntl
#undef syscall
#undef br__file_platform_open
#undef br__file_platform_close
#undef br__file_platform_read
#undef br__file_platform_read_at
#undef br__file_platform_write
#undef br__file_platform_write_at
#undef br__file_platform_seek
#undef br__file_platform_size
#undef br__file_platform_transfer

typedef struct test_native_pair {
  br_file src;
  br_file dst;
  char src_path[512];
  char dst_path[512];
  u8 input[64];
  u8 initial_output[96];
} test_native_pair;

static int test_native_fstat(int fd, struct stat *info) {
  test_native.stat_calls += 1u;
  if (test_native.stat_interrupts > 0u &&
      (test_native.stat_interrupt_fd < 0 || test_native.stat_interrupt_fd == fd)) {
    test_native.stat_interrupts -= 1u;
    errno = EINTR;
    return -1;
  }
  if (fd == test_native.stat_fail_fd) {
    errno = test_native.stat_error;
    return -1;
  }
  return test_native_real_fstat(fd, info);
}

#if defined(TEST_NATIVE_COPY_ACTIVE)
static int test_native_fcntl(int fd, int command, ...) {
  assert(command == F_GETFL);
  test_native.flags_calls += 1u;
  if (test_native.flags_interrupts > 0u &&
      (test_native.flags_interrupt_fd < 0 || test_native.flags_interrupt_fd == fd)) {
    test_native.flags_interrupts -= 1u;
    errno = EINTR;
    return -1;
  }
  if (fd == test_native.flags_fail_fd) {
    errno = test_native.flags_error;
    return -1;
  }
  return fcntl(fd, command);
}

static long test_native_syscall(long number, ...) {
  va_list args;
  long src_fd;
  long src_offset;
  long dst_fd;
  long dst_offset;
  unsigned long len;
  long flags;
  test_native_step step;
  u8 data[64];

  va_start(args, number);
  src_fd = va_arg(args, long);
  src_offset = va_arg(args, long);
  dst_fd = va_arg(args, long);
  dst_offset = va_arg(args, long);
  len = va_arg(args, unsigned long);
  flags = va_arg(args, long);
  va_end(args);
  assert(number == SYS_copy_file_range);
  assert(src_fd == test_native.src_fd && dst_fd == test_native.dst_fd);
  assert(src_offset == 0L && dst_offset == 0L && flags == 0L);
  assert(len > 0u && len <= 1024u * 1024u && len <= (unsigned long)SSIZE_MAX);
  assert(test_native.copy_calls < BR_ARRAY_COUNT(test_native.requests));
  test_native.requests[test_native.copy_calls++] = len;

#if !defined(TEST_NATIVE_FORCED_LINUX)
  if (test_native.real_copy) {
    return test_native_real_syscall(number, src_fd, src_offset, dst_fd, dst_offset, len, flags);
  }
#endif
  assert(test_native.next_step < test_native.step_count);
  step = test_native.steps[test_native.next_step++];
  if (step.count < 0) {
    errno = step.error;
    return -1;
  }
  if ((unsigned long)step.count > len) {
    return step.count; /* An impossible kernel result must be terminal, not fallback. */
  }
  assert((unsigned long)step.count <= sizeof(data));
  if (step.count > 0) {
    assert(read((int)src_fd, data, (size_t)step.count) == step.count);
    assert(write((int)dst_fd, data, (size_t)step.count) == step.count);
  }
  return step.count;
}
#endif

static void test_native_reset(int src_fd, int dst_fd) {
  memset(&test_native, 0, sizeof(test_native));
  test_native.src_fd = src_fd;
  test_native.dst_fd = dst_fd;
  test_native.stat_fail_fd = -1;
  test_native.stat_interrupt_fd = -1;
  test_native.flags_fail_fd = -1;
  test_native.flags_interrupt_fd = -1;
}

static void test_native_pair_open(test_native_pair *pair) {
  const char *temp = getenv("TMPDIR");
  int src_fd;
  int dst_fd;
  int len;

  if (temp == NULL) {
    temp = "/tmp";
  }
  memset(pair, 0, sizeof(*pair));
  len = snprintf(pair->src_path, sizeof(pair->src_path), "%s/br-native-src-XXXXXX", temp);
  assert(len > 0 && (usize)len < sizeof(pair->src_path));
  len = snprintf(pair->dst_path, sizeof(pair->dst_path), "%s/br-native-dst-XXXXXX", temp);
  assert(len > 0 && (usize)len < sizeof(pair->dst_path));
  src_fd = mkstemp(pair->src_path);
  dst_fd = mkstemp(pair->dst_path);
  assert(src_fd >= 0 && dst_fd >= 0);
  for (usize i = 0u; i < sizeof(pair->input); ++i) {
    pair->input[i] = (u8)(i + 1u);
  }
  memset(pair->initial_output, 0xa5, sizeof(pair->initial_output));
  assert(write(src_fd, pair->input, sizeof(pair->input)) == (ssize_t)sizeof(pair->input));
  assert(write(dst_fd, pair->initial_output, sizeof(pair->initial_output)) ==
         (ssize_t)sizeof(pair->initial_output));
  assert(lseek(src_fd, 3, SEEK_SET) == 3);
  assert(lseek(dst_fd, 4, SEEK_SET) == 4);
  pair->src.handle = (uintptr_t)(unsigned int)src_fd + 1u;
  pair->dst.handle = (uintptr_t)(unsigned int)dst_fd + 1u;
  pair->src.flags = BR_FILE_OPEN_READ;
  pair->dst.flags = BR_FILE_OPEN_WRITE;
  test_native_reset(src_fd, dst_fd);
}

static void test_native_pair_close(test_native_pair *pair) {
  assert(close(br__file_fd(&pair->src)) == 0);
  assert(close(br__file_fd(&pair->dst)) == 0);
  assert(unlink(pair->src_path) == 0);
  assert(unlink(pair->dst_path) == 0);
}

static void test_native_assert_none(br_native_error error) {
  assert(error.domain == BR_ERROR_DOMAIN_NONE && error.code == 0u);
}

static void test_native_assert_progress(test_native_pair *pair, usize copied) {
  u8 output[96];
  struct stat info;
  int src_fd = br__file_fd(&pair->src);
  int dst_fd = br__file_fd(&pair->dst);

  assert(lseek(src_fd, 0, SEEK_CUR) == (off_t)(3u + copied));
  assert(lseek(dst_fd, 0, SEEK_CUR) == (off_t)(4u + copied));
  assert(pread(dst_fd, output, sizeof(output), 0) == (ssize_t)sizeof(output));
  assert(fstat(dst_fd, &info) == 0 && info.st_size == (off_t)sizeof(output));
  assert(memcmp(output, pair->initial_output, 4u) == 0);
  assert(memcmp(output + 4u, pair->input + 3u, copied) == 0);
  assert(memcmp(output + 4u + copied,
                pair->initial_output + 4u + copied,
                sizeof(output) - 4u - copied) == 0);
}

/* Hide native peer identity so promotion still exercises ordinary confirmation. */
static br_i64_result test_native_ordinary_file_proc(
  void *context, br_io_mode mode, void *data, usize len, i64 offset, br_seek_from whence) {
  br_file *file = context;
  br_io_result result;

  BR_UNUSED(offset);
  BR_UNUSED(whence);
  switch (mode) {
    case BR_IO_MODE_READ:
      result = br_read(br_file_as_stream(file), data, len);
      break;
    case BR_IO_MODE_WRITE:
      result = br_write(br_file_as_stream(file), data, len);
      break;
    case BR_IO_MODE_QUERY:
      return br_stream_query_utility(br_io_mode_bit(BR_IO_MODE_READ) |
                                     br_io_mode_bit(BR_IO_MODE_WRITE));
    default:
      return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
  }
  return br_i64_result_make_error((i64)result.count,
                                  br_io_error_make(result.status, result.native_error));
}

static br_i64_result test_native_ordinary_copy(test_native_pair *pair) {
  return br_copy(br_stream_make(&pair->dst, test_native_ordinary_file_proc),
                 br_stream_make(&pair->src, test_native_ordinary_file_proc));
}

static void test_native_identity_before_append(void) {
  test_native_pair pair;
  br_file peer;
  br__file_transfer_result result;
  char link_path[520];
  int link_fd;
  usize stat_calls;

  test_native_pair_open(&pair);
  result = test_native_platform_transfer(&pair.src, &pair.src);
  assert(result.result.status == BR_STATUS_INVALID_ARGUMENT && !result.fallback);
  assert(result.result.value == 0 && test_native.stat_calls == 0u && test_native.copy_calls == 0u);
  test_native_assert_none(result.result.native_error);
  test_native_assert_progress(&pair, 0u);

  assert(snprintf(link_path, sizeof(link_path), "%s-link", pair.src_path) > 0);
  assert(link(pair.src_path, link_path) == 0);
  link_fd = open(link_path, O_WRONLY | O_APPEND);
  assert(link_fd >= 0 && lseek(link_fd, 7, SEEK_SET) == 7);
  peer = (br_file)BR_FILE_INIT;
  peer.handle = (uintptr_t)(unsigned int)link_fd + 1u;
  peer.flags = BR_FILE_OPEN_WRITE | BR_FILE_OPEN_APPEND;
  stat_calls = test_native.stat_calls;
  result = test_native_platform_transfer(&peer, &pair.src);
  assert(result.result.status == BR_STATUS_INVALID_ARGUMENT && !result.fallback);
  assert(result.result.value == 0 && test_native.stat_calls == stat_calls + 2u);
  assert(test_native.flags_calls == 0u && test_native.copy_calls == 0u);
  test_native_assert_none(result.result.native_error);
  assert(lseek(link_fd, 0, SEEK_CUR) == 7);
  test_native_assert_progress(&pair, 0u);
  assert(close(link_fd) == 0 && unlink(link_path) == 0);
  test_native_pair_close(&pair);
}

static void test_native_stat_errors(void) {
  static const int errors[] = {EBADF, EIO, ENOSYS};

  for (usize i = 0u; i < BR_ARRAY_COUNT(errors); ++i) {
    for (usize dst = 0u; dst < 2u; ++dst) {
      test_native_pair pair;
      br__file_transfer_result result;

      test_native_pair_open(&pair);
      test_native.stat_interrupts = 2u;
      test_native.stat_fail_fd = dst == 0u ? test_native.src_fd : test_native.dst_fd;
      test_native.stat_interrupt_fd = test_native.stat_fail_fd;
      test_native.stat_error = errors[i];
      result = test_native_platform_transfer(&pair.dst, &pair.src);
      assert(!result.fallback && result.result.value == 0);
      assert(result.result.status == br__os_error_from_errno(errors[i]).status);
      assert(result.result.native_error.domain == BR_ERROR_DOMAIN_POSIX_ERRNO);
      assert(result.result.native_error.code == (u32)errors[i]);
      assert(test_native.copy_calls == 0u && test_native.flags_calls == 0u);
      assert(test_native.stat_interrupts == 0u);
      test_native_assert_progress(&pair, 0u);
      test_native_pair_close(&pair);
    }
  }
}

static void test_native_append_and_nonregular(void) {
  for (usize native = 0u; native < 2u; ++native) {
    test_native_pair pair;
    br__file_transfer_result result;
    int dst_fd;

    test_native_pair_open(&pair);
    dst_fd = br__file_fd(&pair.dst);
    assert(fcntl(dst_fd, F_SETFL, fcntl(dst_fd, F_GETFL) | O_APPEND) == 0);
    if (native == 0u) {
      pair.dst.flags |= BR_FILE_OPEN_APPEND;
    }
    result = test_native_platform_transfer(&pair.dst, &pair.src);
    assert(result.fallback && result.result.status == BR_STATUS_OK && result.result.value == 0);
    assert(test_native.stat_calls == 2u && test_native.copy_calls == 0u);
    test_native_assert_none(result.result.native_error);
    test_native_assert_progress(&pair, 0u);
    {
      br_i64_result copied = test_native_ordinary_copy(&pair);
      u8 output[157];

      assert(copied.status == BR_STATUS_OK && copied.value == 61);
      assert(pread(dst_fd, output, sizeof(output), 0) == (ssize_t)sizeof(output));
      assert(memcmp(output, pair.initial_output, sizeof(pair.initial_output)) == 0);
      assert(memcmp(output + sizeof(pair.initial_output), pair.input + 3u, 61u) == 0);
      assert(lseek(dst_fd, 0, SEEK_CUR) == (off_t)sizeof(output));
    }
    test_native_pair_close(&pair);
  }
  for (usize native = 0u; native < 2u; ++native) {
    test_native_pair pair;
    br__file_transfer_result result;
    int src_fd;

    test_native_pair_open(&pair);
    src_fd = br__file_fd(&pair.src);
    assert(fcntl(src_fd, F_SETFL, fcntl(src_fd, F_GETFL) | O_APPEND) == 0);
    if (native == 0u) {
      pair.src.flags |= BR_FILE_OPEN_WRITE | BR_FILE_OPEN_APPEND;
    }
    result = test_native_platform_transfer(&pair.dst, &pair.src);
    assert(result.fallback && result.result.status == BR_STATUS_OK && result.result.value == 0);
    assert(test_native.stat_calls == 2u && test_native.copy_calls == 0u);
    test_native_assert_none(result.result.native_error);
    test_native_assert_progress(&pair, 0u);
    test_native_pair_close(&pair);
  }
  for (usize dst = 0u; dst < 2u; ++dst) {
    test_native_pair pair;
    br_file pipe_file = BR_FILE_INIT;
    int descriptors[2];
    br__file_transfer_result result;

    test_native_pair_open(&pair);
    assert(pipe(descriptors) == 0);
    pipe_file.handle = (uintptr_t)(unsigned int)descriptors[dst] + 1u;
    pipe_file.flags = dst == 0u ? BR_FILE_OPEN_READ : BR_FILE_OPEN_WRITE;
    result = dst == 0u ? test_native_platform_transfer(&pair.dst, &pipe_file)
                       : test_native_platform_transfer(&pipe_file, &pair.src);
    assert(result.fallback && result.result.status == BR_STATUS_OK && result.result.value == 0);
    assert(test_native.copy_calls == 0u);
    test_native_assert_none(result.result.native_error);
    test_native_assert_progress(&pair, 0u);
    assert(close(descriptors[0]) == 0 && close(descriptors[1]) == 0);
    test_native_pair_close(&pair);
  }
}

#if defined(TEST_NATIVE_COPY_ACTIVE)
static void test_native_set_steps(const test_native_step *steps, usize count) {
  assert(count <= BR_ARRAY_COUNT(test_native.steps));
  memcpy(test_native.steps, steps, count * sizeof(*steps));
  test_native.step_count = count;
}

static void test_native_partial_then_fallback(void) {
  static const int unsupported[] = {0, ENOSYS, EXDEV, EOPNOTSUPP};

  for (usize i = 0u; i < BR_ARRAY_COUNT(unsupported); ++i) {
    for (usize progress = 0u; progress < 2u; ++progress) {
      test_native_pair pair;
      br__file_transfer_result result;
      test_native_step steps[3] = {{3, 0}, {2, 0}, {0, 0}};

      test_native_pair_open(&pair);
      if (unsupported[i] != 0) {
        steps[2] = (test_native_step){-1, unsupported[i]};
      }
      test_native_set_steps(steps + (progress == 0u ? 2u : 0u), progress == 0u ? 1u : 3u);
      test_native.stat_interrupts = 1u;
      test_native.flags_interrupts = 1u;
      result = test_native_platform_transfer(&pair.dst, &pair.src);
      assert(result.fallback && result.result.status == BR_STATUS_OK);
      assert(result.result.value == (progress == 0u ? 0 : 5));
      assert(test_native.next_step == test_native.step_count);
      test_native_assert_none(result.result.native_error);
      test_native_assert_progress(&pair, (usize)result.result.value);
#if !defined(BR_TEST_SMALL_TRANSFER_LIMIT)
      /* Actual public ordinary copying must confirm a zero and finish at current cursors. */
      {
        br_i64_result ordinary = test_native_ordinary_copy(&pair);

        assert(ordinary.status == BR_STATUS_OK);
        assert(result.result.value + ordinary.value == 61);
        test_native_assert_progress(&pair, 61u);
      }
#endif
      test_native_pair_close(&pair);
    }
  }
}

static void test_native_errors_keep_progress(void) {
  static const int errors[] = {EIO, ENOSPC, EINVAL, EBADF, EPERM, EOVERFLOW};

  for (usize i = 0u; i < BR_ARRAY_COUNT(errors); ++i) {
    for (usize progress = 0u; progress < 2u; ++progress) {
      test_native_pair pair;
      br__file_transfer_result result;
      test_native_step steps[3] = {{3, 0}, {2, 0}, {-1, 0}};

      test_native_pair_open(&pair);
      steps[2].error = errors[i];
      test_native_set_steps(steps + (progress == 0u ? 2u : 0u), progress == 0u ? 1u : 3u);
      result = test_native_platform_transfer(&pair.dst, &pair.src);
      assert(!result.fallback && result.result.value == (progress == 0u ? 0 : 5));
      assert(result.result.status == br__os_error_from_errno(errors[i]).status);
      assert(result.result.native_error.domain == BR_ERROR_DOMAIN_POSIX_ERRNO);
      assert(result.result.native_error.code == (u32)errors[i]);
      assert(test_native.next_step == test_native.step_count);
      test_native_assert_progress(&pair, (usize)result.result.value);
      test_native_pair_close(&pair);
    }
  }
}

static void test_native_eintr_and_flag_errors(void) {
  test_native_pair pair;
  br__file_transfer_result result;
  static const test_native_step steps[] = {{-1, EINTR}, {3, 0}, {-1, EINTR}, {2, 0}, {0, 0}};

  test_native_pair_open(&pair);
  test_native_set_steps(steps, BR_ARRAY_COUNT(steps));
  result = test_native_platform_transfer(&pair.dst, &pair.src);
  assert(result.fallback && result.result.status == BR_STATUS_OK && result.result.value == 5);
  assert(test_native.copy_calls == 5u);
  assert(test_native.requests[0] == test_native.requests[1]);
  assert(test_native.requests[2] == test_native.requests[3]);
  test_native_assert_none(result.result.native_error);
  test_native_assert_progress(&pair, 5u);
  test_native_pair_close(&pair);
  for (usize unsupported = 0u; unsupported < 2u; ++unsupported) {
    int error = unsupported == 0u ? EBADF : EOPNOTSUPP;
    for (usize dst = 0u; dst < 2u; ++dst) {
      test_native_pair_open(&pair);
      test_native.flags_interrupts = 2u;
      test_native.flags_fail_fd = dst == 0u ? test_native.src_fd : test_native.dst_fd;
      test_native.flags_interrupt_fd = test_native.flags_fail_fd;
      test_native.flags_error = error;
      result = test_native_platform_transfer(&pair.dst, &pair.src);
      assert(!result.fallback && result.result.value == 0);
      assert(result.result.status == br__os_error_from_errno(error).status);
      assert(result.result.native_error.domain == BR_ERROR_DOMAIN_POSIX_ERRNO);
      assert(result.result.native_error.code == (u32)error);
      assert(test_native.copy_calls == 0u && test_native.flags_interrupts == 0u);
      test_native_assert_progress(&pair, 0u);
      test_native_pair_close(&pair);
    }
  }
}

static void test_native_limits(void) {
  test_native_pair pair;
  br__file_transfer_result result;
  usize chunk = br__file_posix_transfer_chunk(0);

  assert(chunk > 0u && chunk <= 1024u * 1024u && chunk <= (usize)SSIZE_MAX);
  assert(br__file_posix_transfer_chunk(INT64_MAX - 2) == 2u);
  assert(br__file_posix_transfer_chunk(INT64_MAX - 1) == 1u);
  assert(br__file_posix_transfer_chunk(INT64_MAX) == 0u);
  test_native_pair_open(&pair);
  {
    test_native_step step = {(long)chunk + 1, 0};

    test_native_set_steps(&step, 1u);
  }
  result = test_native_platform_transfer(&pair.dst, &pair.src);
  assert(!result.fallback && result.result.status == BR_STATUS_INVALID_STATE);
  assert(result.result.value == 0 && test_native.requests[0] == (unsigned long)chunk);
  test_native_assert_none(result.result.native_error);
  test_native_assert_progress(&pair, 0u);
  test_native_pair_close(&pair);
#if defined(BR_TEST_SMALL_TRANSFER_LIMIT)
  test_native_pair_open(&pair);
  {
    static const test_native_step steps[] = {{11, 0}, {9, 0}, {11, 0}};

    test_native_set_steps(steps, BR_ARRAY_COUNT(steps));
  }
  result = test_native_platform_transfer(&pair.dst, &pair.src);
  assert(result.fallback && result.result.status == BR_STATUS_OK && result.result.value == 31);
  assert(test_native.copy_calls == 3u && test_native.next_step == 3u);
  assert(test_native.requests[0] == 31u);
  assert(test_native.requests[1] == 20u);
  assert(test_native.requests[2] == 11u);
  test_native_assert_none(result.result.native_error);
  test_native_assert_progress(&pair, 31u);
  test_native_pair_close(&pair);
#endif
}

#if !defined(TEST_NATIVE_FORCED_LINUX) && !defined(BR_TEST_SMALL_TRANSFER_LIMIT)
static void test_native_real_linux_copy(void) {
  test_native_pair pair;
  br__file_transfer_result result;
  br_i64_result ordinary;

  test_native_pair_open(&pair);
  test_native.real_copy = true;
  result = test_native_platform_transfer(&pair.dst, &pair.src);
  assert(result.fallback && result.result.status == BR_STATUS_OK);
  assert(result.result.value >= 0 && result.result.value <= 61);
  test_native_assert_none(result.result.native_error);
  test_native_assert_progress(&pair, (usize)result.result.value);
  ordinary = test_native_ordinary_copy(&pair);
  assert(ordinary.status == BR_STATUS_OK && result.result.value + ordinary.value == 61);
  test_native_assert_progress(&pair, 61u);
  printf("real Linux copy_file_range accepted %lld bytes; ordinary confirmation accepted %lld\n",
         (long long)result.result.value,
         (long long)ordinary.value);
  test_native_pair_close(&pair);
}
#endif
#else
static void test_native_platform_without_copy_syscall(void) {
  test_native_pair pair;
  br__file_transfer_result result;

  test_native_pair_open(&pair);
  test_native.stat_interrupts = 2u;
  result = test_native_platform_transfer(&pair.dst, &pair.src);
  assert(result.fallback && result.result.status == BR_STATUS_OK && result.result.value == 0);
  assert(test_native.stat_calls == 4u && test_native.copy_calls == 0u);
  test_native_assert_none(result.result.native_error);
  test_native_assert_progress(&pair, 0u);
  test_native_pair_close(&pair);
}
#endif

int main(void) {
  test_native_identity_before_append();
  test_native_stat_errors();
  test_native_append_and_nonregular();
#if defined(TEST_NATIVE_COPY_ACTIVE)
  test_native_partial_then_fallback();
  test_native_errors_keep_progress();
  test_native_eintr_and_flag_errors();
  test_native_limits();
#if !defined(TEST_NATIVE_FORCED_LINUX) && !defined(BR_TEST_SMALL_TRANSFER_LIMIT)
  test_native_real_linux_copy();
#endif
#else
  test_native_platform_without_copy_syscall();
#endif
  puts("native platform transfer assertions passed");
  return 0;
}

#endif
