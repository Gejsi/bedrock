#include "internal.h"

#if defined(BR__VM_TARGET_OPENBSD)

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

static br_status br__vm_status_from_openbsd_errno(int err) {
  switch (err) {
    case EINVAL:
      return BR_STATUS_INVALID_ARGUMENT;
    case ENOMEM:
      return BR_STATUS_OUT_OF_MEMORY;
    default:
      return BR_STATUS_INVALID_STATE;
  }
}

br_vm_region_result br__vm_platform_reserve(usize size) {
  void *ptr;

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

  ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    return br__vm_region_result(NULL, 0u, br__vm_status_from_openbsd_errno(errno));
  }

  return br__vm_region_result((u8 *)ptr, size, BR_STATUS_OK);
}

void br__vm_platform_decommit(void *ptr, usize size) {
  enum { BR__VM_OPENBSD_MADV_FREE = 6 };

  (void)mprotect(ptr, size, PROT_NONE);
  (void)posix_madvise(ptr, size, BR__VM_OPENBSD_MADV_FREE);
}

#endif
