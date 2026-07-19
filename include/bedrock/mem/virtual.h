#ifndef BEDROCK_MEM_VIRTUAL_H
#define BEDROCK_MEM_VIRTUAL_H

#include <bedrock/base.h>

BR_EXTERN_C_BEGIN

typedef struct br_vm_region {
  uint8_t *data;
  size_t size;
} br_vm_region;

typedef struct br_vm_region_result {
  br_vm_region value;
  br_status status;
} br_vm_region_result;

typedef struct br_vm_mapped_file {
  uint8_t *data;
  size_t size;
} br_vm_mapped_file;

typedef enum br_vm_map_file_error {
  BR_VM_MAP_FILE_ERROR_NONE = 0,
  BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT,
  BR_VM_MAP_FILE_ERROR_OPEN_FAILURE,
  BR_VM_MAP_FILE_ERROR_STAT_FAILURE,
  BR_VM_MAP_FILE_ERROR_NEGATIVE_SIZE,
  BR_VM_MAP_FILE_ERROR_TOO_LARGE_SIZE,
  BR_VM_MAP_FILE_ERROR_MAP_FAILURE
} br_vm_map_file_error;

typedef struct br_vm_mapped_file_result {
  br_vm_mapped_file value;
  br_vm_map_file_error error;
} br_vm_mapped_file_result;

typedef uint32_t br_vm_protect_flags;
typedef uint32_t br_vm_map_file_flags;

enum {
  BR_VM_PROTECT_READ = 1u << 0,
  BR_VM_PROTECT_WRITE = 1u << 1,
  BR_VM_PROTECT_EXECUTE = 1u << 2
};

#define BR_VM_PROTECT_NONE ((br_vm_protect_flags)0u)

enum { BR_VM_MAP_FILE_READ = 1u << 0, BR_VM_MAP_FILE_WRITE = 1u << 1 };

/* Returns 0 when Bedrock does not have a VM backend for the current platform. */
size_t br_vm_page_size(void);

/*
Reserves a page-backed region without making it readable or writable yet on
platforms that support that distinction.
*/
br_vm_region_result br_vm_reserve(size_t size);

/* Reserve and immediately commit a region. */
br_vm_region_result br_vm_reserve_commit(size_t size);

/* Commit previously reserved pages so they can be read and written. */
br_status br_vm_commit(void *ptr, size_t size);

/* Decommit previously committed pages while keeping the reservation alive. */
void br_vm_decommit(void *ptr, size_t size);

/* Release an entire reserved region back to the operating system. */
void br_vm_release(void *ptr, size_t size);

/*
Change page protection flags for an already reserved region. Bedrock currently
exposes the raw primitive but not Odin's higher-level overflow-protected memory
block flags yet.
*/
bool br_vm_protect(void *ptr, size_t size, br_vm_protect_flags flags);

/*
This currently maps from a filesystem path rather than an already-open file
handle. That is a Bedrock v1 simplification; Odin exposes both path and file
entry points.
*/
br_vm_mapped_file_result br_vm_map_file(const char *path, br_vm_map_file_flags flags);
void br_vm_unmap_file(br_vm_mapped_file mapping);

BR_EXTERN_C_END

#endif
