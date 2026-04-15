#include "internal.h"

#if defined(BR__VM_TARGET_FREEBSD)

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

static br_status br__vm_status_from_freebsd_errno(int err) {
  switch (err) {
    case EINVAL:
      return BR_STATUS_INVALID_ARGUMENT;
    case ENOMEM:
      return BR_STATUS_OUT_OF_MEMORY;
    default:
      return BR_STATUS_INVALID_STATE;
  }
}

static int br__vm_freebsd_prot_max(int prot) {
  return prot << 16;
}

br_vm_region_result br__vm_platform_reserve(usize size) {
  void *ptr;

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

  ptr = mmap(NULL,
             size,
             br__vm_freebsd_prot_max(PROT_READ | PROT_WRITE | PROT_EXEC),
             MAP_PRIVATE | MAP_ANONYMOUS,
             -1,
             0);
  if (ptr == MAP_FAILED) {
    return br__vm_region_result(NULL, 0u, br__vm_status_from_freebsd_errno(errno));
  }

  return br__vm_region_result((u8 *)ptr, size, BR_STATUS_OK);
}

void br__vm_platform_decommit(void *ptr, usize size) {
  enum { BR__VM_FREEBSD_MADV_FREE = 5 };

  (void)mprotect(ptr, size, PROT_NONE);
  (void)posix_madvise(ptr, size, BR__VM_FREEBSD_MADV_FREE);
}

#endif
