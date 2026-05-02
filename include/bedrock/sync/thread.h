#ifndef BEDROCK_SYNC_THREAD_H
#define BEDROCK_SYNC_THREAD_H

#include <bedrock/base.h>

BR_EXTERN_C_BEGIN

typedef u64 br_thread_id;

#define BR_THREAD_ID_INVALID ((br_thread_id)0)

br_thread_id br_current_thread_id(void);

BR_EXTERN_C_END

#endif
