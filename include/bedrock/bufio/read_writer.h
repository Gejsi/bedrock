#ifndef BEDROCK_BUFIO_READ_WRITER_H
#define BEDROCK_BUFIO_READ_WRITER_H

#include <bedrock/bufio/reader.h>
#include <bedrock/bufio/writer.h>

BR_EXTERN_C_BEGIN

typedef struct br_bufio_read_writer {
  br_bufio_reader *reader;
  br_bufio_writer *writer;
} br_bufio_read_writer;

/*
Pair a buffered reader and writer into one stream-like value.
*/
void br_bufio_read_writer_init(br_bufio_read_writer *read_writer,
                               br_bufio_reader *reader,
                               br_bufio_writer *writer);

br_stream br_bufio_read_writer_as_stream(br_bufio_read_writer *read_writer);

BR_EXTERN_C_END

#endif
