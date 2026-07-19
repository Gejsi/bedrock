#include <bedrock/thread/thread.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602 /* Windows 8 / Server 2012: SetThreadDescription is 1607+ */
#endif

#include <windows.h>

#include <process.h> /* _beginthreadex */
#include <string.h>

/* Lifecycle states; INERT is 0 so a zero-initialized handle is inert. */
enum { BR__THREAD_INERT = 0, BR__THREAD_FRESH, BR__THREAD_JOINED, BR__THREAD_DETACHED };

#define BR__THREAD_NAME_CAP 64u

typedef struct br__thread_start {
  br_thread_fn fn;
  void *arg;
  char name[BR__THREAD_NAME_CAP];
  bool has_name;
  br_sema handshake;
} br__thread_start;

static void br__thread_apply_name(const char *name) {
  /* SetThreadDescription links directly via kernel32 on Windows 10 1607+; if the
     OS is older the call is simply absent and naming is skipped. Resolve it
     dynamically so we never hard-depend on it. */
  typedef HRESULT(WINAPI * set_desc_fn)(HANDLE, PCWSTR);
  HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
  set_desc_fn set_desc;
  WCHAR wide[BR__THREAD_NAME_CAP];
  int n;

  if (kernel == NULL) {
    return;
  }
  set_desc = (set_desc_fn)(void (*)(void))GetProcAddress(kernel, "SetThreadDescription");
  if (set_desc == NULL) {
    return;
  }

  n = MultiByteToWideChar(CP_UTF8, 0, name, -1, wide, (int)BR__THREAD_NAME_CAP);
  if (n <= 0) {
    return;
  }
  (void)set_desc(GetCurrentThread(), wide);
}

static unsigned __stdcall br__thread_trampoline(void *raw) {
  br__thread_start *start = (br__thread_start *)raw;
  br_thread_fn fn;
  void *arg;
  char name[BR__THREAD_NAME_CAP];
  bool has_name;

  /* Step 1: copy out of the creator-stack control block (the only touch). */
  fn = start->fn;
  arg = start->arg;
  has_name = start->has_name;
  if (has_name) {
    memcpy(name, start->name, sizeof(name));
  }

  /* Step 2: post the handshake. `start` must NOT be touched afterward: its frame
     belongs to the creator and can die the instant the wait returns.
     br_sema_post's last user-space access is its RELEASE atomic add; the wake
     (WakeByAddressSingle path) passes only the address to the kernel and the
     creator's waiter is loop-based, so posting into about-to-die memory is
     safe. */
  br_sema_post(&start->handshake, 1u);

  /* Steps 3-5: name self, run, return the int through the substrate. The
     br_thread handle is never referenced. */
  if (has_name) {
    br__thread_apply_name(name);
  }
  return (unsigned)fn(arg);
}

static br_status
br__thread_spawn(br_thread *thread, br_thread_fn fn, void *arg, const br_thread_options *options) {
  br__thread_start start;
  uintptr_t handle;
  unsigned thread_id = 0u;
  unsigned stack_size = 0u;

  if (thread == NULL || fn == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  br_atomic_store_explicit(&thread->state, (u32)BR__THREAD_INERT, BR_ATOMIC_RELAXED);

  start.fn = fn;
  start.arg = arg;
  start.has_name = false;
  if (options != NULL && options->name != NULL) {
    size_t i;
    for (i = 0u; i + 1u < BR__THREAD_NAME_CAP && options->name[i] != '\0'; ++i) {
      start.name[i] = options->name[i];
    }
    start.name[i] = '\0';
    start.has_name = true;
  }
  br_sema_init(&start.handshake, 0u);

  if (options != NULL && options->stack_size != 0u && options->stack_size <= (size_t)0xffffffffu) {
    stack_size = (unsigned)options->stack_size;
  }

  /* NEVER CreateThread: _beginthreadex initializes per-thread CRT state that a
     C library and its callers rely on (errno, strtok, locale). thrdaddr gives
     the creator the thread id for the self-join guard. */
  handle = _beginthreadex(NULL, stack_size, br__thread_trampoline, &start, 0u, &thread_id);
  if (handle == 0u) {
    br_sema_destroy(&start.handshake);
    return BR_STATUS_OUT_OF_MEMORY;
  }

  br_sema_wait(&start.handshake);
  br_sema_destroy(&start.handshake);

  /* Creator-side identity write for the self-join guard. */
  thread->native = (void *)handle;
  thread->native_id = (unsigned long)thread_id;
  br_atomic_store_explicit(&thread->state, (u32)BR__THREAD_FRESH, BR_ATOMIC_RELEASE);
  return BR_STATUS_OK;
}

br_status br_thread_create(br_thread *thread, br_thread_fn fn, void *arg) {
  return br__thread_spawn(thread, fn, arg, NULL);
}

br_status br_thread_create_ex(br_thread *thread,
                              br_thread_fn fn,
                              void *arg,
                              const br_thread_options *options) {
  return br__thread_spawn(thread, fn, arg, options);
}

br_status br_thread_join(br_thread *thread, int *exit_code) {
  u32 expected;
  DWORD code = 0;

  if (thread == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  /* Self-join check BEFORE the state transition (compare creator-captured id). */
  if (br_atomic_load_explicit(&thread->state, BR_ATOMIC_ACQUIRE) == (u32)BR__THREAD_FRESH &&
      GetCurrentThreadId() == (DWORD)thread->native_id) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  expected = (u32)BR__THREAD_FRESH;
  if (!br_atomic_compare_exchange_strong(&thread->state, &expected, (u32)BR__THREAD_JOINED)) {
    return BR_STATUS_INVALID_STATE;
  }

  (void)WaitForSingleObject((HANDLE)thread->native, INFINITE);
  (void)GetExitCodeThread((HANDLE)thread->native, &code);
  (void)CloseHandle((HANDLE)thread->native);
  if (exit_code != NULL) {
    *exit_code = (int)code;
  }
  return BR_STATUS_OK;
}

br_status br_thread_detach(br_thread *thread) {
  u32 expected;

  if (thread == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  expected = (u32)BR__THREAD_FRESH;
  if (!br_atomic_compare_exchange_strong(&thread->state, &expected, (u32)BR__THREAD_DETACHED)) {
    return BR_STATUS_INVALID_STATE;
  }

  /* Closing the only handle is Win32's detach: the thread runs to completion and
     the OS reclaims it. */
  (void)CloseHandle((HANDLE)thread->native);
  return BR_STATUS_OK;
}

void br_thread_yield(void) {
  (void)SwitchToThread();
}

#endif /* defined(_WIN32) */
