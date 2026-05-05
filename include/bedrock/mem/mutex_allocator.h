#ifndef BEDROCK_MEM_MUTEX_ALLOCATOR_H
#define BEDROCK_MEM_MUTEX_ALLOCATOR_H

#include <bedrock/mem/alloc.h>
#include <bedrock/sync/primitives.h>

BR_EXTERN_C_BEGIN

/*
Mutex allocator serializes every request forwarded to the backing allocator.
This mirrors Odin's `Mutex_Allocator`, with explicit backing selection instead
of an ambient context allocator.
*/
typedef struct br_mutex_allocator {
  br_allocator backing;
  br_mutex mutex;
} br_mutex_allocator;

void br_mutex_allocator_init(br_mutex_allocator *mutex_allocator, br_allocator backing);
br_allocator br_mutex_allocator_allocator(br_mutex_allocator *mutex_allocator);

BR_EXTERN_C_END

#endif
