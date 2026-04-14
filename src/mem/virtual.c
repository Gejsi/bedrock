#include <bedrock/mem/virtual.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

static br_vm_region_result br__vm_region_result(u8 *data, usize size, br_status status) {
  br_vm_region_result result;

  result.value.data = data;
  result.value.size = size;
  result.status = status;
  return result;
}

static usize br__vm_cached_page_size(void) {
  static usize cached_page_size = 0u;

  if (cached_page_size != 0u) {
    return cached_page_size;
  }

#if defined(_WIN32)
  {
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    cached_page_size = (usize)info.dwPageSize;
  }
#elif defined(_SC_PAGESIZE)
  {
    long page_size = sysconf(_SC_PAGESIZE);

    cached_page_size = page_size > 0 ? (usize)page_size : 4096u;
  }
#else
  cached_page_size = 4096u;
#endif

  return cached_page_size;
}

#if !defined(_WIN32)
static br_status br__vm_status_from_errno(int err) {
  switch (err) {
    case EINVAL:
      return BR_STATUS_INVALID_ARGUMENT;
    case ENOMEM:
      return BR_STATUS_OUT_OF_MEMORY;
    default:
      return BR_STATUS_INVALID_STATE;
  }
}
#endif

usize br_vm_page_size(void) {
#if defined(_WIN32) || defined(MAP_PRIVATE)
  return br__vm_cached_page_size();
#else
  return 0u;
#endif
}

br_vm_region_result br_vm_reserve(usize size) {
  if (size == 0u) {
    return br__vm_region_result(NULL, 0u, BR_STATUS_OK);
  }

#if defined(_WIN32)
  {
    void *ptr = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);

    if (ptr == NULL) {
      return br__vm_region_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
    }

    return br__vm_region_result((u8 *)ptr, size, BR_STATUS_OK);
  }
#elif defined(MAP_PRIVATE)
  {
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif
    void *ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ptr == MAP_FAILED) {
      return br__vm_region_result(NULL, 0u, br__vm_status_from_errno(errno));
    }

    return br__vm_region_result((u8 *)ptr, size, BR_STATUS_OK);
  }
#else
  return br__vm_region_result(NULL, 0u, BR_STATUS_NOT_SUPPORTED);
#endif
}

br_status br_vm_commit(void *ptr, usize size) {
  if (size == 0u) {
    return BR_STATUS_OK;
  }
  if (ptr == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

#if defined(_WIN32)
  return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL ? BR_STATUS_OK
                                                                     : BR_STATUS_OUT_OF_MEMORY;
#elif defined(MAP_PRIVATE)
  return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0 ? BR_STATUS_OK
                                                          : br__vm_status_from_errno(errno);
#else
  BR_UNUSED(ptr);
  BR_UNUSED(size);
  return BR_STATUS_NOT_SUPPORTED;
#endif
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

void br_vm_decommit(void *ptr, usize size) {
  if (ptr == NULL || size == 0u) {
    return;
  }

#if defined(_WIN32)
  (void)VirtualFree(ptr, size, MEM_DECOMMIT);
#elif defined(MAP_PRIVATE)
  (void)mprotect(ptr, size, PROT_NONE);
#if defined(MADV_FREE)
  (void)madvise(ptr, size, MADV_FREE);
#elif defined(POSIX_MADV_DONTNEED)
  (void)posix_madvise(ptr, size, POSIX_MADV_DONTNEED);
#elif defined(MADV_DONTNEED)
  (void)madvise(ptr, size, MADV_DONTNEED);
#endif
#else
  BR_UNUSED(ptr);
  BR_UNUSED(size);
#endif
}

void br_vm_release(void *ptr, usize size) {
  if (ptr == NULL || size == 0u) {
    return;
  }

#if defined(_WIN32)
  BR_UNUSED(size);
  (void)VirtualFree(ptr, 0u, MEM_RELEASE);
#elif defined(MAP_PRIVATE)
  (void)munmap(ptr, size);
#else
  BR_UNUSED(ptr);
  BR_UNUSED(size);
#endif
}

bool br_vm_protect(void *ptr, usize size, br_vm_protect_flags flags) {
  if (size == 0u) {
    return true;
  }
  if (ptr == NULL) {
    return false;
  }

#if defined(_WIN32)
  {
    DWORD protect = PAGE_NOACCESS;
    DWORD old_protect = 0u;

    switch (flags) {
      case BR_VM_PROTECT_NONE:
        protect = PAGE_NOACCESS;
        break;
      case BR_VM_PROTECT_READ:
        protect = PAGE_READONLY;
        break;
      case BR_VM_PROTECT_WRITE:
        protect = PAGE_WRITECOPY;
        break;
      case BR_VM_PROTECT_READ | BR_VM_PROTECT_WRITE:
        protect = PAGE_READWRITE;
        break;
      case BR_VM_PROTECT_EXECUTE:
        protect = PAGE_EXECUTE;
        break;
      case BR_VM_PROTECT_EXECUTE | BR_VM_PROTECT_READ:
        protect = PAGE_EXECUTE_READ;
        break;
      case BR_VM_PROTECT_EXECUTE | BR_VM_PROTECT_WRITE:
        protect = PAGE_EXECUTE_WRITECOPY;
        break;
      case BR_VM_PROTECT_EXECUTE | BR_VM_PROTECT_READ | BR_VM_PROTECT_WRITE:
        protect = PAGE_EXECUTE_READWRITE;
        break;
      default:
        return false;
    }

    return VirtualProtect(ptr, size, protect, &old_protect) != 0;
  }
#elif defined(MAP_PRIVATE)
  {
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
#else
  BR_UNUSED(ptr);
  BR_UNUSED(size);
  BR_UNUSED(flags);
  return false;
#endif
}
