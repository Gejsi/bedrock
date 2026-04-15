#include "internal.h"

#if defined(BR__VM_TARGET_OTHER)

usize br__vm_platform_page_size_query(void) {
  return 0u;
}

br_vm_region_result br__vm_platform_reserve(usize size) {
  BR_UNUSED(size);
  return br__vm_region_result(NULL, 0u, BR_STATUS_NOT_SUPPORTED);
}

br_status br__vm_platform_commit(void *ptr, usize size) {
  BR_UNUSED(ptr);
  BR_UNUSED(size);
  return BR_STATUS_NOT_SUPPORTED;
}

void br__vm_platform_decommit(void *ptr, usize size) {
  BR_UNUSED(ptr);
  BR_UNUSED(size);
}

void br__vm_platform_release(void *ptr, usize size) {
  BR_UNUSED(ptr);
  BR_UNUSED(size);
}

bool br__vm_platform_protect(void *ptr, usize size, br_vm_protect_flags flags) {
  BR_UNUSED(ptr);
  BR_UNUSED(size);
  BR_UNUSED(flags);
  return false;
}

br_vm_mapped_file_result br__vm_platform_map_file(const char *path, br_vm_map_file_flags flags) {
  BR_UNUSED(path);
  BR_UNUSED(flags);
  return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_MAP_FAILURE);
}

void br__vm_platform_unmap_file(br_vm_mapped_file mapping) {
  BR_UNUSED(mapping);
}

#endif
