#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(__linux__)
#include <errno.h>
#endif

#include <bedrock.h>

#include "../src/io/io_internal.h"

static void test_transfer_path(char *path, usize cap, const char *suffix) {
  long process_id;
  int count;

#if defined(_WIN32)
  process_id = (long)_getpid();
#else
  process_id = (long)getpid();
#endif
  count = snprintf(path, cap, "bedrock-file-transfer-%ld-%s.tmp", process_id, suffix);
  assert(count > 0);
  assert((usize)count < cap);
  (void)remove(path);
}

static void test_transfer_create(br_file *file, const char *path, const void *data, usize len) {
  br_file_open_options options;
  br_io_result written;
  br_stream stream;

  options =
    br_file_open_options_make(BR_FILE_OPEN_READ | BR_FILE_OPEN_WRITE | BR_FILE_OPEN_CREATE_NEW);
  assert(br_file_open(file, br_string_view_from_cstr(path), options).status == BR_STATUS_OK);
  stream = br_file_as_stream(file);
  written = br_write_full(stream, data, len);
  assert(written.status == BR_STATUS_OK);
  assert(written.count == len);
  assert(br_seek(stream, 0, BR_SEEK_FROM_START).status == BR_STATUS_OK);
}

static void test_transfer_contents(br_file *file, const void *expected, usize len) {
  u8 data[64];
  br_stream stream;
  br_io_result read_result;

  assert(len <= sizeof(data));
  stream = br_file_as_stream(file);
  assert(br_size(stream).size == (i64)len);
  read_result = br_read_at(stream, data, len, 0);
  assert(read_result.status == BR_STATUS_OK);
  assert(read_result.count == len);
  assert(memcmp(data, expected, len) == 0);
}

static br_i64_result test_transfer_call(br_stream file, br_io_mode mode, br_stream peer) {
  br_io_transfer_request request;

  request.peer = peer;
  /* The typed transfer protocol leaves offset and whence unused. */
  return file.procedure(file.context, mode, &request, sizeof(request), 123, BR_SEEK_FROM_END);
}

static void test_file_transfer_cursors_and_destination_tail(void) {
  unsigned entry;

  for (entry = 0u; entry < 3u; entry += 1u) {
    br_file src = BR_FILE_INIT;
    br_file dst = BR_FILE_INIT;
    br_stream source;
    br_stream destination;
    br_i64_result copied;
    char source_path[128];
    char destination_path[128];

    test_transfer_path(source_path, sizeof(source_path), "cursor-src");
    test_transfer_path(destination_path, sizeof(destination_path), "cursor-dst");
    test_transfer_create(&src, source_path, "abcdef", 6u);
    test_transfer_create(&dst, destination_path, "0123456789", 10u);
    source = br_file_as_stream(&src);
    destination = br_file_as_stream(&dst);
    assert(br_seek(source, 2, BR_SEEK_FROM_START).status == BR_STATUS_OK);
    assert(br_seek(destination, 3, BR_SEEK_FROM_START).status == BR_STATUS_OK);
    if (entry == 0u) {
      copied = br_copy(destination, source);
    } else if (entry == 1u) {
      copied = test_transfer_call(source, BR_IO_MODE_WRITE_TO, destination);
    } else {
      copied = test_transfer_call(destination, BR_IO_MODE_READ_FROM, source);
    }
    assert(copied.status == BR_STATUS_OK);
    assert(copied.value == 4);
    assert(copied.native_error.domain == BR_ERROR_DOMAIN_NONE);
    assert(copied.native_error.code == 0u);
    assert(br_seek(source, 0, BR_SEEK_FROM_CURRENT).offset == 6);
    assert(br_seek(destination, 0, BR_SEEK_FROM_CURRENT).offset == 7);
    test_transfer_contents(&src, "abcdef", 6u);
    test_transfer_contents(&dst, "012cdef789", 10u);
    assert(br_seek(source, 0, BR_SEEK_FROM_CURRENT).offset == 6);
    assert(br_seek(destination, 0, BR_SEEK_FROM_CURRENT).offset == 7);
    copied = br_copy(destination, source);
    assert(copied.status == BR_STATUS_OK);
    assert(copied.value == 0);
    assert(br_seek(destination, 0, BR_SEEK_FROM_CURRENT).offset == 7);
    assert(br_file_close(&src).status == BR_STATUS_OK);
    assert(br_file_close(&dst).status == BR_STATUS_OK);
    assert(remove(source_path) == 0);
    assert(remove(destination_path) == 0);
  }
}

static void test_file_transfer_empty_and_large(void) {
  br_file src = BR_FILE_INIT;
  br_file dst = BR_FILE_INIT;
  br_stream source;
  br_stream destination;
  br_i64_result copied;
  br_io_result read_result;
  u8 data[8197];
  u8 actual[8197];
  char source_path[128];
  char destination_path[128];
  usize i;

  test_transfer_path(source_path, sizeof(source_path), "large-src");
  test_transfer_path(destination_path, sizeof(destination_path), "large-dst");
  test_transfer_create(&src, source_path, NULL, 0u);
  test_transfer_create(&dst, destination_path, "retained", 8u);
  source = br_file_as_stream(&src);
  destination = br_file_as_stream(&dst);
  copied = br_copy(destination, source);
  assert(copied.status == BR_STATUS_OK);
  assert(copied.value == 0);
  test_transfer_contents(&dst, "retained", 8u);
  assert(br_seek(source, 0, BR_SEEK_FROM_CURRENT).offset == 0);
  assert(br_seek(destination, 0, BR_SEEK_FROM_CURRENT).offset == 0);
  for (i = 0u; i < sizeof(data); i += 1u) {
    data[i] = (u8)((i * 37u + i / 19u) & 255u);
  }
  assert(br_write_full(source, data, sizeof(data)).status == BR_STATUS_OK);
  assert(br_seek(source, 0, BR_SEEK_FROM_START).status == BR_STATUS_OK);
  copied = br_copy(destination, source);
  assert(copied.status == BR_STATUS_OK);
  assert(copied.value == (i64)sizeof(data));
  assert(br_seek(source, 0, BR_SEEK_FROM_CURRENT).offset == (i64)sizeof(data));
  assert(br_seek(destination, 0, BR_SEEK_FROM_CURRENT).offset == (i64)sizeof(data));
  read_result = br_read_at(destination, actual, sizeof(actual), 0);
  assert(read_result.status == BR_STATUS_OK);
  assert(read_result.count == sizeof(actual));
  assert(memcmp(actual, data, sizeof(data)) == 0);
  assert(br_file_close(&src).status == BR_STATUS_OK);
  assert(br_file_close(&dst).status == BR_STATUS_OK);
  assert(remove(source_path) == 0);
  assert(remove(destination_path) == 0);
}

static void test_file_transfer_append(void) {
  br_file src = BR_FILE_INIT;
  br_file dst = BR_FILE_INIT;
  br_file_open_options options;
  br_stream source;
  br_stream destination;
  br_i64_result copied;
  char source_path[128];
  char destination_path[128];

  test_transfer_path(source_path, sizeof(source_path), "append-src");
  test_transfer_path(destination_path, sizeof(destination_path), "append-dst");
  test_transfer_create(&src, source_path, "abcdef", 6u);
  test_transfer_create(&dst, destination_path, "prefix", 6u);
  assert(br_file_close(&dst).status == BR_STATUS_OK);
  options = br_file_open_options_make(BR_FILE_OPEN_WRITE | BR_FILE_OPEN_APPEND);
  assert(br_file_open(&dst, br_string_view_from_cstr(destination_path), options).status ==
         BR_STATUS_OK);
  source = br_file_as_stream(&src);
  destination = br_file_as_stream(&dst);
  assert((br_query(destination).modes & br_io_mode_bit(BR_IO_MODE_READ_FROM)) != 0u);
  assert(br_seek(source, 2, BR_SEEK_FROM_START).status == BR_STATUS_OK);
  assert(br_seek(destination, 1, BR_SEEK_FROM_START).status == BR_STATUS_OK);
  copied = test_transfer_call(destination, BR_IO_MODE_READ_FROM, source);
  assert(copied.status == BR_STATUS_OK);
  assert(copied.value == 4);
  assert(br_seek(source, 0, BR_SEEK_FROM_CURRENT).offset == 6);
  assert(br_seek(destination, 0, BR_SEEK_FROM_CURRENT).offset == 10);
  assert(br_file_close(&src).status == BR_STATUS_OK);
  assert(br_file_close(&dst).status == BR_STATUS_OK);
  options = br_file_open_options_make(BR_FILE_OPEN_READ);
  assert(br_file_open(&dst, br_string_view_from_cstr(destination_path), options).status ==
         BR_STATUS_OK);
  test_transfer_contents(&dst, "prefixcdef", 10u);
  assert(br_file_close(&dst).status == BR_STATUS_OK);
  assert(remove(source_path) == 0);
  assert(remove(destination_path) == 0);
}

static void test_file_transfer_access_directions(void) {
  br_file src = BR_FILE_INIT;
  br_file dst = BR_FILE_INIT;
  br_file_open_options options;
  br_i64_result copied;
  char source_path[128];
  char destination_path[128];

  test_transfer_path(source_path, sizeof(source_path), "access-src");
  test_transfer_path(destination_path, sizeof(destination_path), "access-dst");
  test_transfer_create(&src, source_path, "source", 6u);
  test_transfer_create(&dst, destination_path, "targetTAIL", 10u);
  assert(br_file_close(&src).status == BR_STATUS_OK);
  assert(br_file_close(&dst).status == BR_STATUS_OK);
  options = br_file_open_options_make(BR_FILE_OPEN_READ);
  assert(br_file_open(&src, br_string_view_from_cstr(source_path), options).status == BR_STATUS_OK);
  assert(br_file_open(&dst, br_string_view_from_cstr(destination_path), options).status ==
         BR_STATUS_OK);
  copied = br_copy(br_file_as_stream(&dst), br_file_as_stream(&src));
  assert(copied.status == BR_STATUS_INVALID_ARGUMENT && copied.value == 0);
  assert(br_seek(br_file_as_stream(&src), 0, BR_SEEK_FROM_CURRENT).offset == 0);
  assert(br_seek(br_file_as_stream(&dst), 0, BR_SEEK_FROM_CURRENT).offset == 0);
  test_transfer_contents(&dst, "targetTAIL", 10u);
  assert(br_file_close(&dst).status == BR_STATUS_OK);

  options = br_file_open_options_make(BR_FILE_OPEN_WRITE);
  assert(br_file_open(&dst, br_string_view_from_cstr(destination_path), options).status ==
         BR_STATUS_OK);
  copied = br_copy(br_file_as_stream(&dst), br_file_as_stream(&src));
  assert(copied.status == BR_STATUS_OK && copied.value == 6);
  assert(br_seek(br_file_as_stream(&src), 0, BR_SEEK_FROM_CURRENT).offset == 6);
  assert(br_seek(br_file_as_stream(&dst), 0, BR_SEEK_FROM_CURRENT).offset == 6);
  assert(br_file_close(&src).status == BR_STATUS_OK);
  assert(br_file_open(&src, br_string_view_from_cstr(source_path), options).status == BR_STATUS_OK);
  copied = br_copy(br_file_as_stream(&dst), br_file_as_stream(&src));
  assert(copied.status == BR_STATUS_INVALID_ARGUMENT && copied.value == 0);
  assert(br_seek(br_file_as_stream(&src), 0, BR_SEEK_FROM_CURRENT).offset == 0);
  assert(br_seek(br_file_as_stream(&dst), 0, BR_SEEK_FROM_CURRENT).offset == 6);
  assert(br_file_close(&src).status == BR_STATUS_OK);
  assert(br_file_close(&dst).status == BR_STATUS_OK);
  options = br_file_open_options_make(BR_FILE_OPEN_READ);
  assert(br_file_open(&dst, br_string_view_from_cstr(destination_path), options).status ==
         BR_STATUS_OK);
  test_transfer_contents(&dst, "sourceTAIL", 10u);
  assert(br_file_close(&dst).status == BR_STATUS_OK);
  assert(remove(source_path) == 0);
  assert(remove(destination_path) == 0);
}

static usize test_foreign_calls;

static br_i64_result test_foreign_proc(
  void *context, br_io_mode mode, void *data, usize len, i64 offset, br_seek_from whence) {
  BR_UNUSED(context);
  BR_UNUSED(mode);
  BR_UNUSED(data);
  BR_UNUSED(len);
  BR_UNUSED(offset);
  BR_UNUSED(whence);
  test_foreign_calls += 1u;
  return br_i64_result_make(0, BR_STATUS_INVALID_STATE);
}

static void test_file_transfer_validation_and_query(void) {
  br_file file = BR_FILE_INIT;
  br_file peer = BR_FILE_INIT;
  br_stream stream;
  br_stream foreign;
  br_io_transfer_request request;
  br_i64_result result;
  br_io_mode_set modes;
  br_file_open_options options;
  char path[128];

  test_transfer_path(path, sizeof(path), "validation");
  test_transfer_create(&file, path, "original", 8u);
  stream = br_file_as_stream(&file);
  modes = br_query(stream).modes;
  assert((modes & br_io_mode_bit(BR_IO_MODE_WRITE_TO)) != 0u);
  assert((modes & br_io_mode_bit(BR_IO_MODE_READ_FROM)) != 0u);
  request.peer = br_file_as_stream(&peer);
  result = stream.procedure(
    stream.context, BR_IO_MODE_WRITE_TO, NULL, sizeof(request), 0, BR_SEEK_FROM_START);
  assert(result.status == BR_STATUS_INVALID_ARGUMENT);
  result = stream.procedure(
    stream.context, BR_IO_MODE_WRITE_TO, &request, sizeof(request) - 1u, 0, BR_SEEK_FROM_START);
  assert(result.status == BR_STATUS_INVALID_ARGUMENT);
  result = test_transfer_call(stream, BR_IO_MODE_WRITE_TO, request.peer);
  assert(result.status == BR_STATUS_INVALID_STATE);
  assert(result.value == 0);
  result = test_transfer_call(stream, BR_IO_MODE_WRITE_TO, br_stream_make(NULL, NULL));
  assert(result.status == BR_STATUS_INVALID_ARGUMENT);
  result = test_transfer_call(stream, BR_IO_MODE_WRITE_TO, stream);
  assert(result.status == BR_STATUS_INVALID_ARGUMENT);
  assert(result.value == 0);
  assert(br_copy(stream, stream).status == BR_STATUS_INVALID_ARGUMENT);
  foreign = br_stream_make((void *)(uptr)1u, test_foreign_proc);
  test_foreign_calls = 0u;
  result = test_transfer_call(stream, BR_IO_MODE_WRITE_TO, foreign);
  assert(result.status == BR_STATUS_NOT_SUPPORTED);
  assert(result.value == 0);
  result = test_transfer_call(stream, BR_IO_MODE_READ_FROM, foreign);
  assert(result.status == BR_STATUS_NOT_SUPPORTED);
  assert(result.value == 0);
  assert(test_foreign_calls == 0u);
  assert(br_seek(stream, 0, BR_SEEK_FROM_CURRENT).offset == 0);
  test_transfer_contents(&file, "original", 8u);
  assert(br_file_close(&file).status == BR_STATUS_OK);

  options = br_file_open_options_make(BR_FILE_OPEN_READ);
  assert(br_file_open(&file, br_string_view_from_cstr(path), options).status == BR_STATUS_OK);
  modes = br_query(br_file_as_stream(&file)).modes;
  assert((modes & br_io_mode_bit(BR_IO_MODE_WRITE_TO)) != 0u);
  assert((modes & br_io_mode_bit(BR_IO_MODE_READ_FROM)) == 0u);
  options = br_file_open_options_make(BR_FILE_OPEN_WRITE);
  assert(br_file_open(&peer, br_string_view_from_cstr(path), options).status == BR_STATUS_OK);
  modes = br_query(br_file_as_stream(&peer)).modes;
  assert((modes & br_io_mode_bit(BR_IO_MODE_WRITE_TO)) == 0u);
  assert((modes & br_io_mode_bit(BR_IO_MODE_READ_FROM)) != 0u);
  result =
    test_transfer_call(br_file_as_stream(&peer), BR_IO_MODE_READ_FROM, br_file_as_stream(&peer));
  assert(result.status == BR_STATUS_INVALID_ARGUMENT);
  result =
    test_transfer_call(br_file_as_stream(&file), BR_IO_MODE_WRITE_TO, br_file_as_stream(&file));
  assert(result.status == BR_STATUS_INVALID_ARGUMENT);
  assert(br_file_close(&file).status == BR_STATUS_OK);
  assert(br_file_close(&peer).status == BR_STATUS_OK);
  assert(remove(path) == 0);
}

static void test_file_transfer_native_identity(void) {
  br_file src = BR_FILE_INIT;
  br_file dst = BR_FILE_INIT;
  br_file_open_options options;
  br_i64_result copied;
  char source_path[128];
  char link_path[128];
  unsigned entry;

  test_transfer_path(source_path, sizeof(source_path), "identity-src");
  test_transfer_path(link_path, sizeof(link_path), "identity-link");
  test_transfer_create(&src, source_path, "same native file", 16u);
  assert(br_file_close(&src).status == BR_STATUS_OK);
  options = br_file_open_options_make(BR_FILE_OPEN_READ);
  assert(br_file_open(&src, br_string_view_from_cstr(source_path), options).status == BR_STATUS_OK);
#if defined(_WIN32)
  assert(CreateHardLinkA(link_path, source_path, NULL));
#else
  assert(link(source_path, link_path) == 0);
#endif
  for (entry = 0u; entry < 4u; entry += 1u) {
    const char *path = (entry & 1u) == 0u ? source_path : link_path;
    br_stream source = br_file_as_stream(&src);
    br_stream destination;

    options =
      br_file_open_options_make(BR_FILE_OPEN_WRITE | (entry < 2u ? 0u : BR_FILE_OPEN_APPEND));
    assert(br_file_open(&dst, br_string_view_from_cstr(path), options).status == BR_STATUS_OK);
    destination = br_file_as_stream(&dst);
    assert(br_seek(source, 1, BR_SEEK_FROM_START).status == BR_STATUS_OK);
    assert(br_seek(destination, 3, BR_SEEK_FROM_START).status == BR_STATUS_OK);
    copied = br_copy(destination, source);
    assert(copied.status == BR_STATUS_INVALID_ARGUMENT);
    assert(copied.value == 0);
    assert(copied.native_error.domain == BR_ERROR_DOMAIN_NONE);
    assert(br_seek(source, 0, BR_SEEK_FROM_CURRENT).offset == 1);
    assert(br_seek(destination, 0, BR_SEEK_FROM_CURRENT).offset == 3);
    copied = test_transfer_call(destination, BR_IO_MODE_READ_FROM, source);
    assert(copied.status == BR_STATUS_INVALID_ARGUMENT);
    assert(copied.value == 0);
    test_transfer_contents(&src, "same native file", 16u);
    assert(br_file_close(&dst).status == BR_STATUS_OK);
  }
  assert(br_file_close(&src).status == BR_STATUS_OK);
  assert(remove(source_path) == 0);
  assert(remove(link_path) == 0);
}

static void test_file_transfer_memory_and_buffered_composition(void) {
  br_file src = BR_FILE_INIT;
  br_file dst = BR_FILE_INIT;
  br_byte_reader memory;
  br_byte_buffer output;
  br_bufio_reader buffered_source;
  br_bufio_writer buffered_destination;
  br_i64_result copied;
  br_bytes_view contents;
  u8 source_buffer[64];
  u8 destination_buffer[64];
  char source_path[128];
  char destination_path[128];

  test_transfer_path(source_path, sizeof(source_path), "composition-src");
  test_transfer_path(destination_path, sizeof(destination_path), "composition-dst");
  test_transfer_create(&src, source_path, NULL, 0u);
  test_transfer_create(&dst, destination_path, NULL, 0u);
  br_byte_reader_init(&memory, br_bytes_view_make("memory", 6u));
  copied = br_copy(br_file_as_stream(&src), br_byte_reader_as_stream(&memory));
  assert(copied.status == BR_STATUS_OK && copied.value == 6);
  assert(br_seek(br_file_as_stream(&src), 0, BR_SEEK_FROM_START).status == BR_STATUS_OK);
  br_byte_buffer_init(&output, br_allocator_heap());
  copied = br_copy(br_byte_buffer_as_stream(&output), br_file_as_stream(&src));
  assert(copied.status == BR_STATUS_OK && copied.value == 6);
  contents = br_byte_buffer_view(&output);
  assert(contents.len == 6u && memcmp(contents.data, "memory", 6u) == 0);
  br_byte_buffer_destroy(&output);
  assert(br_seek(br_file_as_stream(&src), 0, BR_SEEK_FROM_START).status == BR_STATUS_OK);
  assert(br_bufio_reader_init_with_buffer(
           &buffered_source, br_file_as_stream(&src), source_buffer, sizeof(source_buffer)) ==
         BR_STATUS_OK);
  assert(br_bufio_reader_peek(&buffered_source, 3u).status == BR_STATUS_OK);
  copied = br_copy(br_file_as_stream(&dst), br_bufio_reader_as_stream(&buffered_source));
  assert(copied.status == BR_STATUS_OK && copied.value == 6);
  test_transfer_contents(&dst, "memory", 6u);
  br_bufio_reader_destroy(&buffered_source);
  assert(br_seek(br_file_as_stream(&src), 0, BR_SEEK_FROM_START).status == BR_STATUS_OK);
  assert(br_seek(br_file_as_stream(&dst), 0, BR_SEEK_FROM_START).status == BR_STATUS_OK);
  assert(br_bufio_writer_init_with_buffer(&buffered_destination,
                                          br_file_as_stream(&dst),
                                          destination_buffer,
                                          sizeof(destination_buffer)) == BR_STATUS_OK);
  assert(br_bufio_writer_write(&buffered_destination, "pre|", 4u).status == BR_STATUS_OK);
  copied = br_copy(br_bufio_writer_as_stream(&buffered_destination), br_file_as_stream(&src));
  assert(copied.status == BR_STATUS_OK && copied.value == 6);
  assert(br_bufio_writer_flush(&buffered_destination).status == BR_STATUS_OK);
  test_transfer_contents(&dst, "pre|memory", 10u);
  assert(br_bufio_writer_destroy(&buffered_destination).status == BR_STATUS_OK);
  assert(br_file_is_open(&src) && br_file_is_open(&dst));
  assert(br_file_close(&src).status == BR_STATUS_OK);
  assert(br_file_close(&dst).status == BR_STATUS_OK);
  assert(remove(source_path) == 0);
  assert(remove(destination_path) == 0);
}

typedef struct test_copy_continuation {
  const u8 *data;
  usize len;
  usize read;
  usize requested;
  usize read_calls;
  u8 output[8];
  usize written;
  usize write_limit;
  bool fail_write;
  bool no_progress;
} test_copy_continuation;

static br_i64_result test_copy_continuation_proc(
  void *context, br_io_mode mode, void *data, usize len, i64 offset, br_seek_from whence) {
  test_copy_continuation *copy = context;
  usize count;

  BR_UNUSED(offset);
  BR_UNUSED(whence);
  if (mode == BR_IO_MODE_READ) {
    copy->read_calls += 1u;
    copy->requested = len;
    count = br_min_size(copy->len - copy->read, len);
    if (count == 0u) {
      return br_i64_result_make(0, BR_STATUS_EOF);
    }
    memcpy(data, copy->data + copy->read, count);
    copy->read += count;
    return br_i64_result_make((i64)count, BR_STATUS_OK);
  }
  if (mode == BR_IO_MODE_WRITE) {
    br_error error;

    if (copy->no_progress) {
      return br_i64_result_make(0, BR_STATUS_OK);
    }
    if (copy->fail_write && copy->written > 0u) {
      error = br_error_make_native(BR_STATUS_IO_ERROR, BR_ERROR_DOMAIN_POSIX_ERRNO, 5u);
      return br_i64_result_make_error(0, error);
    }
    count = br_min_size(len, copy->write_limit);
    assert(count <= sizeof(copy->output) - copy->written);
    memcpy(copy->output + copy->written, data, count);
    copy->written += count;
    return br_i64_result_make((i64)count, BR_STATUS_OK);
  }
  return br_i64_result_make(0, BR_STATUS_NOT_SUPPORTED);
}

static void test_file_transfer_aggregate_count_limit(void) {
  test_copy_continuation src = {0};
  test_copy_continuation dst = {0};
  br_stream source = br_stream_make(&src, test_copy_continuation_proc);
  br_stream destination = br_stream_make(&dst, test_copy_continuation_proc);
  br_i64_result result;
  u8 scratch[8];

  src.data = (const u8 *)"abcd";
  src.len = 4u;
  dst.write_limit = 1u;
  result = br__io_copy_buffer(destination, source, scratch, sizeof(scratch), INT64_MAX - 3);
  assert(result.status == BR_STATUS_OUT_OF_RANGE && result.value == INT64_MAX);
  assert(src.read == 3u && src.read_calls == 1u && src.requested == 3u);
  assert(dst.written == 3u && memcmp(dst.output, "abc", 3u) == 0);

  src.read = 0u;
  src.read_calls = 0u;
  dst.written = 0u;
  dst.fail_write = true;
  result = br__io_copy_buffer(destination, source, scratch, sizeof(scratch), INT64_MAX - 2);
  assert(result.status == BR_STATUS_IO_ERROR && result.value == INT64_MAX - 1);
  assert(result.native_error.domain == BR_ERROR_DOMAIN_POSIX_ERRNO);
  assert(result.native_error.code == 5u);
  assert(src.read == 2u && src.read_calls == 1u && src.requested == 2u);
  assert(dst.written == 1u && dst.output[0] == (u8)'a');

  src.read = 0u;
  src.read_calls = 0u;
  dst.written = 0u;
  dst.fail_write = false;
  dst.no_progress = true;
  result = br__io_copy_buffer(destination, source, scratch, sizeof(scratch), 7);
  assert(result.status == BR_STATUS_NO_PROGRESS && result.value == 7);
  assert(src.read == 4u && dst.written == 0u);

  src.read = 0u;
  src.read_calls = 0u;
  result = br__io_copy_buffer(destination, source, scratch, sizeof(scratch), INT64_MAX);
  assert(result.status == BR_STATUS_OUT_OF_RANGE && result.value == INT64_MAX);
  assert(src.read_calls == 0u);
  result = br__io_copy_buffer(destination, source, scratch, sizeof(scratch), -1);
  assert(result.status == BR_STATUS_INVALID_ARGUMENT && result.value == 0);
}

#if defined(__linux__)
static void test_file_transfer_native_failure(void) {
  br_file src = BR_FILE_INIT;
  br_file dst = BR_FILE_INIT;
  br_file_open_options options = br_file_open_options_make(BR_FILE_OPEN_WRITE);
  br_i64_result copied;
  char source_path[128];

  test_transfer_path(source_path, sizeof(source_path), "native-failure");
  test_transfer_create(&src, source_path, "failure", 7u);
  assert(br_file_open(&dst, BR_STR_LIT("/dev/full"), options).status == BR_STATUS_OK);
  copied = br_copy(br_file_as_stream(&dst), br_file_as_stream(&src));
  assert(copied.status == BR_STATUS_NO_SPACE);
  assert(copied.value == 0);
  assert(copied.native_error.domain == BR_ERROR_DOMAIN_POSIX_ERRNO);
  assert(copied.native_error.code == (u32)ENOSPC);
  assert(br_seek(br_file_as_stream(&src), 0, BR_SEEK_FROM_CURRENT).offset == 7);
  assert(br_file_close(&src).status == BR_STATUS_OK);
  assert(br_file_close(&dst).status == BR_STATUS_OK);
  assert(remove(source_path) == 0);
}
#endif

int main(void) {
  test_file_transfer_cursors_and_destination_tail();
  test_file_transfer_empty_and_large();
  test_file_transfer_append();
  test_file_transfer_access_directions();
  test_file_transfer_validation_and_query();
  test_file_transfer_native_identity();
  test_file_transfer_memory_and_buffered_composition();
  test_file_transfer_aggregate_count_limit();
#if defined(__linux__)
  test_file_transfer_native_failure();
#endif
  return 0;
}
