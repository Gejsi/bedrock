#ifndef BEDROCK_IO_IO_H
#define BEDROCK_IO_IO_H

#include <bedrock/base.h>

BR_EXTERN_C_BEGIN

/*
Shared byte-count plus status result used by Bedrock IO traits.
*/
typedef struct br_io_result {
  usize count;
  br_status status;
} br_io_result;

/*
Shared seek result used by Bedrock IO traits.
*/
typedef struct br_io_seek_result {
  i64 offset;
  br_status status;
} br_io_seek_result;

/*
Seek origin shared by byte and string readers and the generic seeker trait.
*/
typedef enum br_seek_from {
  BR_SEEK_FROM_START = 0,
  BR_SEEK_FROM_CURRENT = 1,
  BR_SEEK_FROM_END = 2
} br_seek_from;

typedef br_io_result (*br_reader_read_fn)(void *context, void *dst, usize dst_len);
typedef br_io_result (*br_writer_write_fn)(void *context, const void *src, usize src_len);
typedef br_io_seek_result (*br_seeker_seek_fn)(void *context, i64 offset, br_seek_from whence);

typedef struct br_reader {
  void *context;
  br_reader_read_fn read;
} br_reader;

typedef struct br_writer {
  void *context;
  br_writer_write_fn write;
} br_writer;

typedef struct br_seeker {
  void *context;
  br_seeker_seek_fn seek;
} br_seeker;

static inline br_io_result br_io_result_make(usize count, br_status status) {
  br_io_result result;

  result.count = count;
  result.status = status;
  return result;
}

static inline br_io_seek_result br_io_seek_result_make(i64 offset, br_status status) {
  br_io_seek_result result;

  result.offset = offset;
  result.status = status;
  return result;
}

static inline br_reader br_reader_make(void *context, br_reader_read_fn read) {
  br_reader reader;

  reader.context = context;
  reader.read = read;
  return reader;
}

static inline br_writer br_writer_make(void *context, br_writer_write_fn write) {
  br_writer writer;

  writer.context = context;
  writer.write = write;
  return writer;
}

static inline br_seeker br_seeker_make(void *context, br_seeker_seek_fn seek) {
  br_seeker seeker;

  seeker.context = context;
  seeker.seek = seek;
  return seeker;
}

static inline bool br_reader_is_valid(br_reader reader) {
  return reader.read != NULL;
}

static inline bool br_writer_is_valid(br_writer writer) {
  return writer.write != NULL;
}

static inline bool br_seeker_is_valid(br_seeker seeker) {
  return seeker.seek != NULL;
}

/*
Read bytes using a generic reader trait.
*/
br_io_result br_reader_read(br_reader reader, void *dst, usize dst_len);

/*
Write bytes using a generic writer trait.
*/
br_io_result br_writer_write(br_writer writer, const void *src, usize src_len);

/*
Seek using a generic seeker trait.
*/
br_io_seek_result br_seeker_seek(br_seeker seeker, i64 offset, br_seek_from whence);

BR_EXTERN_C_END

#endif
