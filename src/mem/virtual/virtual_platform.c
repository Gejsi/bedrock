#include "internal.h"

br__vm_platform_memory_block *
br__vm_platform_memory_alloc(usize to_commit, usize to_reserve, br_status *status) {
  br_vm_region_result region;
  br_status commit_status;
  br__vm_platform_memory_block *block;

  if (status == NULL) {
    return NULL;
  }

  to_reserve = br_max_size(to_reserve, sizeof(br__vm_platform_memory_block));
  to_commit = br_max_size(to_commit, sizeof(br__vm_platform_memory_block));
  to_commit = br_min_size(to_commit, to_reserve);

  region = br_vm_reserve(to_reserve);
  if (region.status != BR_STATUS_OK) {
    *status = region.status;
    return NULL;
  }

  commit_status = br_vm_commit(region.value.data, to_commit);
  if (commit_status != BR_STATUS_OK) {
    br_vm_release(region.value.data, region.value.size);
    *status = commit_status;
    return NULL;
  }

  block = (br__vm_platform_memory_block *)(void *)region.value.data;
  memset(block, 0, sizeof(*block));
  block->committed_total = to_commit;
  block->reserved_total = to_reserve;
  *status = BR_STATUS_OK;
  return block;
}

void br__vm_platform_memory_free(br__vm_platform_memory_block *block) {
  if (block == NULL) {
    return;
  }

  br_vm_release(block, block->reserved_total);
}

br_status br__vm_platform_memory_commit(br__vm_platform_memory_block *block, usize to_commit) {
  br_status status;

  if (block == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (to_commit < block->committed_total) {
    return BR_STATUS_OK;
  }
  if (to_commit > block->reserved_total) {
    return BR_STATUS_OUT_OF_MEMORY;
  }

  status = br_vm_commit(block, to_commit);
  if (status != BR_STATUS_OK) {
    return status;
  }

  block->committed_total = to_commit;
  return BR_STATUS_OK;
}
