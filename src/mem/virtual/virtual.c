#include "internal.h"

br_vm_region_result br_vm_reserve(usize size) {
  if (size == 0u) {
    return br__vm_region_result(NULL, 0u, BR_STATUS_OK);
  }

  return br__vm_platform_reserve(size);
}

br_status br_vm_commit(void *ptr, usize size) {
  if (size == 0u) {
    return BR_STATUS_OK;
  }
  if (ptr == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  return br__vm_platform_commit(ptr, size);
}

void br_vm_decommit(void *ptr, usize size) {
  if (ptr == NULL || size == 0u) {
    return;
  }

  br__vm_platform_decommit(ptr, size);
}

void br_vm_release(void *ptr, usize size) {
  if (ptr == NULL || size == 0u) {
    return;
  }

  br__vm_platform_release(ptr, size);
}

bool br_vm_protect(void *ptr, usize size, br_vm_protect_flags flags) {
  if (size == 0u) {
    return true;
  }
  if (ptr == NULL) {
    return false;
  }

  return br__vm_platform_protect(ptr, size, flags);
}
