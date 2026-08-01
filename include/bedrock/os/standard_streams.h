#ifndef BEDROCK_OS_STANDARD_STREAMS_H
#define BEDROCK_OS_STANDARD_STREAMS_H

#include <bedrock/io/io.h>

BR_EXTERN_C_BEGIN

/*
Borrowed, unbuffered byte streams for the process standard handles.

The returned values are copyable and do not own the underlying descriptor or
handle. Each operation resolves the current process handle, so redirection
performed after obtaining the stream is observed. `CLOSE` and `FLUSH` are not
supported; `DESTROY` succeeds without closing anything.

These streams are byte-transparent for files and pipes. They do not perform
terminal encoding conversion.
*/
br_reader br_stdin(void);
br_writer br_stdout(void);
br_writer br_stderr(void);

BR_EXTERN_C_END

#endif
