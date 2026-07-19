#include "internal.h"

#if defined(BR__VM_TARGET_POSIX)

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

static br_status br__vm_status_from_posix_errno(int err) {
  switch (err) {
    case EACCES:
    case EPERM:
      return BR_STATUS_INVALID_STATE;
    case EINVAL:
    case ENOTSUP:
      return BR_STATUS_INVALID_ARGUMENT;
    case ENOMEM:
      return BR_STATUS_OUT_OF_MEMORY;
    default:
      return BR_STATUS_INVALID_STATE;
  }
}

usize br__vm_platform_page_size_query(void) {
#if defined(_SC_PAGESIZE)
  long page_size = sysconf(_SC_PAGESIZE);

  return page_size > 0 ? (usize)page_size : 4096u;
#else
  return 4096u;
#endif
}

br_status br__vm_platform_commit(void *ptr, usize size) {
  return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0 ? BR_STATUS_OK
                                                          : br__vm_status_from_posix_errno(errno);
}

void br__vm_platform_release(void *ptr, usize size) {
  (void)munmap(ptr, size);
}

bool br__vm_platform_protect(void *ptr, usize size, br_vm_protect_flags flags) {
  int protect = PROT_NONE;

  if ((flags & BR_VM_PROTECT_READ) != 0u) {
    protect |= PROT_READ;
  }
  if ((flags & BR_VM_PROTECT_WRITE) != 0u) {
    protect |= PROT_WRITE;
  }
  if ((flags & BR_VM_PROTECT_EXECUTE) != 0u) {
    protect |= PROT_EXEC;
  }

  return mprotect(ptr, size, protect) == 0;
}

#endif
