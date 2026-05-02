#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <bedrock/sync/primitives.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__FreeBSD__)
#include <pthread_np.h>
#elif defined(__NetBSD__)
#include <lwp.h>
#elif defined(__OpenBSD__)
#include <unistd.h>
#elif defined(__HAIKU__)
#include <OS.h>
#endif

br_thread_id br_current_thread_id(void) {
#if defined(_WIN32)
  return (br_thread_id)GetCurrentThreadId();
#elif defined(__linux__)
  long tid = syscall(SYS_gettid);
  return tid > 0 ? (br_thread_id)tid : BR_THREAD_ID_INVALID;
#elif defined(__APPLE__) && defined(__MACH__)
  u64 tid = 0;
  if (pthread_threadid_np(NULL, &tid) != 0) {
    return BR_THREAD_ID_INVALID;
  }
  return (br_thread_id)tid;
#elif defined(__FreeBSD__)
  return (br_thread_id)pthread_getthreadid_np();
#elif defined(__NetBSD__)
  return (br_thread_id)_lwp_self();
#elif defined(__OpenBSD__)
  return (br_thread_id)getthrid();
#elif defined(__HAIKU__)
  return (br_thread_id)find_thread(NULL);
#else
  return BR_THREAD_ID_INVALID;
#endif
}
