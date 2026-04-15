#ifndef BEDROCK_MEM_VIRTUAL_INTERNAL_H
#define BEDROCK_MEM_VIRTUAL_INTERNAL_H

#include <bedrock/mem/virtual_arena.h>

#if defined(_WIN32)
#define BR__VM_BACKEND_WINDOWS 1
#elif defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) ||   \
  defined(__OpenBSD__) || defined(__unix__)
#define BR__VM_BACKEND_POSIX 1
#else
#define BR__VM_BACKEND_OTHER 1
#endif

struct br_virtual_arena_block {
  struct br_virtual_arena_block *prev;
  u8 *base;
  usize used;
  usize committed;
  usize reserved;
};

typedef struct br__vm_platform_memory_block {
  br_virtual_arena_block block;
  usize committed_total;
  usize reserved_total;
} br__vm_platform_memory_block;

br_vm_region_result br__vm_region_result(u8 *data, usize size, br_status status);
br_vm_mapped_file_result
br__vm_mapped_file_result(u8 *data, usize size, br_vm_map_file_error error);

usize br__vm_cached_page_size(void);
usize br__vm_platform_page_size_query(void);

br_vm_region_result br__vm_platform_reserve(usize size);
br_status br__vm_platform_commit(void *ptr, usize size);
void br__vm_platform_decommit(void *ptr, usize size);
void br__vm_platform_release(void *ptr, usize size);
bool br__vm_platform_protect(void *ptr, usize size, br_vm_protect_flags flags);

br_vm_mapped_file_result br__vm_platform_map_file(const char *path, br_vm_map_file_flags flags);
void br__vm_platform_unmap_file(br_vm_mapped_file mapping);

br__vm_platform_memory_block *
br__vm_platform_memory_alloc(usize to_commit, usize to_reserve, br_status *status);
void br__vm_platform_memory_free(br__vm_platform_memory_block *block);
br_status br__vm_platform_memory_commit(br__vm_platform_memory_block *block, usize to_commit);

#endif
