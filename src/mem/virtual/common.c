#include <bedrock/base.h>

#include <stdatomic.h>

#include "internal.h"

br_vm_region_result br__vm_region_result(u8 *data, usize size, br_status status) {
  br_vm_region_result result;

  result.value.data = data;
  result.value.size = size;
  result.status = status;
  return result;
}

usize br__vm_cached_page_size(void) {
  static atomic_size_t cached_page_size = 0u;
  usize page_size;
  usize expected;

  page_size = atomic_load_explicit(&cached_page_size, memory_order_acquire);
  if (page_size != 0u) {
    return page_size;
  }

  page_size = br__vm_platform_page_size_query();
  if (page_size == 0u) {
    return 0u;
  }

  expected = 0u;
  if (!atomic_compare_exchange_strong_explicit(
        &cached_page_size, &expected, page_size, memory_order_release, memory_order_relaxed)) {
    page_size = expected;
  }

  return page_size;
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
