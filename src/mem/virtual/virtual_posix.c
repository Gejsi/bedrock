#include "internal.h"

#if defined(BR__VM_TARGET_POSIX)

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
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

br_vm_mapped_file_result br__vm_platform_map_file(const char *path, br_vm_map_file_flags flags) {
  int open_flags = O_RDONLY;
  int prot = 0;
  int fd;
  struct stat st;
  void *view;

  if ((flags & BR_VM_MAP_FILE_WRITE) != 0u) {
    open_flags = O_RDWR;
    prot |= PROT_WRITE;
  }
  if ((flags & BR_VM_MAP_FILE_READ) != 0u || (flags & BR_VM_MAP_FILE_WRITE) == 0u) {
    prot |= PROT_READ;
  }

  fd = open(path, open_flags);
  if (fd < 0) {
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_OPEN_FAILURE);
  }

  if (fstat(fd, &st) != 0) {
    close(fd);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_STAT_FAILURE);
  }
  if (st.st_size < 0) {
    close(fd);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_NEGATIVE_SIZE);
  }
  if ((u64)st.st_size > (u64)SIZE_MAX) {
    close(fd);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_TOO_LARGE_SIZE);
  }
  if (st.st_size == 0) {
    close(fd);
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_NONE);
  }

  view = mmap(NULL, (usize)st.st_size, prot, MAP_SHARED, fd, 0);
  close(fd);
  if (view == MAP_FAILED || view == NULL) {
    return br__vm_mapped_file_result(NULL, 0u, BR_VM_MAP_FILE_ERROR_MAP_FAILURE);
  }

  return br__vm_mapped_file_result((u8 *)view, (usize)st.st_size, BR_VM_MAP_FILE_ERROR_NONE);
}

void br__vm_platform_unmap_file(br_vm_mapped_file mapping) {
#if defined(MS_SYNC)
  (void)msync(mapping.data, mapping.size, MS_SYNC);
#endif
  (void)munmap(mapping.data, mapping.size);
}

#endif
