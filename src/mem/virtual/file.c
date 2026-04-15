#include "internal.h"

br_vm_mapped_file_result br_vm_map_file(const char *path, br_vm_map_file_flags flags) {
  if (path == NULL || (flags & (BR_VM_MAP_FILE_READ | BR_VM_MAP_FILE_WRITE)) == 0u) {
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_INVALID_ARGUMENT);
  }

  return br__vm_platform_map_file(path, flags);
}

void br_vm_unmap_file(br_vm_mapped_file mapping) {
  if (mapping.data == NULL || mapping.size == 0u) {
    return;
  }

  br__vm_platform_unmap_file(mapping);
}
