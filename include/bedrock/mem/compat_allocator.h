#ifndef BEDROCK_MEM_COMPAT_ALLOCATOR_H
#define BEDROCK_MEM_COMPAT_ALLOCATOR_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_compat_allocator {
  br_allocator parent;
} br_compat_allocator;

/*
Bedrock keeps Odin's compat allocator behavior close, with these intentional
C-side adaptations:
- explicit `parent` allocator defaults to heap when unset, instead of using
  Odin's ambient context allocator
- the wrapper only targets Bedrock's current alloc/free/resize/reset ABI, so
  there is no public generic query-features/query-info surface yet
- invalid pointers cannot be generically validated before reading the compat
  header, so ownership checking is still delegated to the parent allocator
*/

void br_compat_allocator_init(br_compat_allocator *compat, br_allocator parent);
br_allocator br_compat_allocator_allocator(br_compat_allocator *compat);

BR_EXTERN_C_END

#endif
