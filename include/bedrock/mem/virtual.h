#ifndef BEDROCK_MEM_VIRTUAL_H
#define BEDROCK_MEM_VIRTUAL_H

#include <bedrock/base.h>

BR_EXTERN_C_BEGIN

typedef struct br_vm_region {
  u8 *data;
  usize size;
} br_vm_region;

typedef struct br_vm_region_result {
  br_vm_region value;
  br_status status;
} br_vm_region_result;

typedef u32 br_vm_protect_flags;

enum {
  BR_VM_PROTECT_READ = 1u << 0,
  BR_VM_PROTECT_WRITE = 1u << 1,
  BR_VM_PROTECT_EXECUTE = 1u << 2
};

#define BR_VM_PROTECT_NONE ((br_vm_protect_flags)0u)

/* Returns 0 when Bedrock does not have a VM backend for the current platform. */
usize br_vm_page_size(void);

/*
Reserves a page-backed region without making it readable or writable yet on
platforms that support that distinction.
*/
br_vm_region_result br_vm_reserve(usize size);

/* Reserve and immediately commit a region. */
br_vm_region_result br_vm_reserve_commit(usize size);

/* Commit previously reserved pages so they can be read and written. */
br_status br_vm_commit(void *ptr, usize size);

/* Decommit previously committed pages while keeping the reservation alive. */
void br_vm_decommit(void *ptr, usize size);

/* Release an entire reserved region back to the operating system. */
void br_vm_release(void *ptr, usize size);

/*
Change page protection flags for an already reserved region. Bedrock currently
exposes the raw primitive but not Odin's higher-level overflow-protected memory
block flags yet.
*/
bool br_vm_protect(void *ptr, usize size, br_vm_protect_flags flags);

BR_EXTERN_C_END

#endif
