#ifndef BEDROCK_MEM_COMPAT_ALLOCATOR_H
#define BEDROCK_MEM_COMPAT_ALLOCATOR_H

#include <bedrock/mem/alloc.h>

BR_EXTERN_C_BEGIN

typedef struct br_compat_allocator {
  br_allocator parent;
} br_compat_allocator;

/*
Adapts a bedrock allocator to the sizeless-free allocation ABI that foreign C
libraries expect. Bedrock's own allocator interface carries `old_size` on every
free and resize; many C libraries instead take malloc/free/realloc-style
callbacks that receive only a pointer. This wrapper bridges the two by recording
each allocation's size in a header prefix, so a caller can free or resize an
allocation WITHOUT tracking or passing its original size.

Design choices:
- the explicit `parent` allocator defaults to heap when unset, instead of
  relying on an ambient allocator
- the wrapper targets bedrock's current alloc/free/resize/reset ABI, so there is
  no public generic query-features/query-info surface yet
- an arbitrary pointer cannot be generically validated before its header prefix
  is read, so ownership checking is delegated to the parent allocator
*/

void br_compat_allocator_init(br_compat_allocator *compat, br_allocator parent);
br_allocator br_compat_allocator_allocator(br_compat_allocator *compat);

BR_EXTERN_C_END

#endif
