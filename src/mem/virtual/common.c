#include <bedrock/base.h>

#include "internal.h"

br_vm_region_result br__vm_region_result(u8 *data, usize size, br_status status) {
  br_vm_region_result result;

  result.value.data = data;
  result.value.size = size;
  result.status = status;
  return result;
}

br_vm_mapped_file_result
br__vm_mapped_file_result(u8 *data, usize size, br_vm_map_file_error error) {
  br_vm_mapped_file_result result;

  result.value.data = data;
  result.value.size = size;
  result.error = error;
  return result;
}

usize br__vm_cached_page_size(void) {
  static usize cached_page_size = 0u;

  if (cached_page_size == 0u) {
    cached_page_size = br__vm_platform_page_size_query();
  }

  return cached_page_size;
}

usize br_vm_page_size(void) {
  return br__vm_cached_page_size();
}

br_vm_region_result br_vm_reserve_commit(usize size) {
  br_status status;
  br_vm_region_result result;

  result = br_vm_reserve(size);
  if (result.status != BR_STATUS_OK || result.value.data == NULL || size == 0u) {
    return result;
  }

  status = br_vm_commit(result.value.data, size);
  if (status != BR_STATUS_OK) {
    br_vm_release(result.value.data, result.value.size);
    return br__vm_region_result(NULL, 0u, status);
  }

  return result;
}
