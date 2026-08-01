#include <bedrock/thread/thread.h>
#include <bedrock/mem/alloc.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602 /* Windows 8 / Server 2012: SetThreadDescription is 1607+ */
#endif

#include <windows.h>

#include <errno.h>
#include <process.h> /* _beginthreadex */

enum {
  BR__THREAD_INERT = 0,
  BR__THREAD_FRESH,
  BR__THREAD_JOINING,
  BR__THREAD_JOINED,
  BR__THREAD_DETACHING,
  BR__THREAD_DETACHED
};

/* Keep one portable copied-name limit, matching macOS's 63 bytes plus NUL. */
#define BR__THREAD_NAME_CAP 64u

typedef struct br__thread_control {
  br_atomic_u32 refs;
  br_thread_fn fn;
  void *arg;
  char name[BR__THREAD_NAME_CAP];
  bool has_name;
  int result;
  HANDLE native;
} br__thread_control;

static _Thread_local br__thread_control *br__thread_current_control = NULL;

static void br__thread_control_free(br__thread_control *control) {
  BR_UNUSED(br_allocator_free(br_allocator_heap(), control, sizeof(*control)));
}

static void br__thread_control_release(br__thread_control *control) {
  if (br_atomic_sub_explicit(&control->refs, 1u, BR_ATOMIC_ACQ_REL) == 1u) {
    br__thread_control_free(control);
  }
}

static br_status br__thread_create_error(int error) {
  switch (error) {
    case EAGAIN:
    case EACCES:
      return BR_STATUS_OUT_OF_MEMORY;
    case EINVAL:
      return BR_STATUS_OUT_OF_RANGE;
    default:
      return BR_STATUS_INVALID_STATE;
  }
}

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
  br__thread_control *control = (br__thread_control *)raw;

  br__thread_current_control = control;
  if (control->has_name) {
    br__thread_apply_name(control->name);
  }
  control->result = control->fn(control->arg);
  br__thread_current_control = NULL;
  br__thread_control_release(control);
  return 0u;
}

static br_status
br__thread_spawn(br_thread *thread, br_thread_fn fn, void *arg, const br_thread_options *options) {
  br_alloc_result allocation;
  br__thread_control *control;
  uintptr_t handle;
  unsigned stack_size = 0u;
  unsigned creation_flags = 0u;
  int error;

  if (thread == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  br_atomic_init(&thread->state, (u32)BR__THREAD_INERT);
  thread->control = NULL;
  if (fn == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  if (options != NULL && options->stack_size > (size_t)UINT32_MAX) {
    return BR_STATUS_OUT_OF_RANGE;
  }
  if (options != NULL) {
    stack_size = (unsigned)options->stack_size;
    if (stack_size != 0u) {
      creation_flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
    }
  }

  allocation =
    br_allocator_alloc_uninit(br_allocator_heap(), sizeof(*control), _Alignof(br__thread_control));
  if (allocation.status != BR_STATUS_OK) {
    return allocation.status;
  }
  control = (br__thread_control *)allocation.ptr;
  br_atomic_init(&control->refs, 2u);
  control->fn = fn;
  control->arg = arg;
  control->has_name = false;
  control->result = 0;
  if (options != NULL && options->name != NULL) {
    size_t i;
    for (i = 0u; i + 1u < BR__THREAD_NAME_CAP && options->name[i] != '\0'; ++i) {
      control->name[i] = options->name[i];
    }
    control->name[i] = '\0';
    control->has_name = true;
  }

  /* NEVER CreateThread: _beginthreadex initializes per-thread CRT state that a
     C library and its callers rely on (errno, strtok, locale). */
  handle = _beginthreadex(NULL, stack_size, br__thread_trampoline, control, creation_flags, NULL);
  if (handle == 0u) {
    error = errno;
    br__thread_control_free(control);
    return br__thread_create_error(error);
  }

  control->native = (HANDLE)handle;
  thread->control = control;
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
  br__thread_control *control;
  u32 expected;
  DWORD wait_result;

  if (thread == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  expected = (u32)BR__THREAD_FRESH;
  if (!br_atomic_compare_exchange_strong(&thread->state, &expected, (u32)BR__THREAD_JOINING)) {
    return BR_STATUS_INVALID_STATE;
  }

  control = (br__thread_control *)thread->control;
  if (br__thread_current_control == control) {
    br_atomic_store_explicit(&thread->state, (u32)BR__THREAD_FRESH, BR_ATOMIC_RELEASE);
    return BR_STATUS_INVALID_ARGUMENT;
  }

  wait_result = WaitForSingleObject(control->native, INFINITE);
  if (wait_result != WAIT_OBJECT_0) {
    br_atomic_store_explicit(&thread->state, (u32)BR__THREAD_FRESH, BR_ATOMIC_RELEASE);
    return BR_STATUS_INVALID_STATE;
  }
  if (!CloseHandle(control->native)) {
    br_atomic_store_explicit(&thread->state, (u32)BR__THREAD_FRESH, BR_ATOMIC_RELEASE);
    return BR_STATUS_INVALID_STATE;
  }
  if (exit_code != NULL) {
    *exit_code = control->result;
  }
  thread->control = NULL;
  br_atomic_store_explicit(&thread->state, (u32)BR__THREAD_JOINED, BR_ATOMIC_RELEASE);
  br__thread_control_release(control);
  return BR_STATUS_OK;
}

br_status br_thread_detach(br_thread *thread) {
  br__thread_control *control;
  u32 expected;

  if (thread == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  expected = (u32)BR__THREAD_FRESH;
  if (!br_atomic_compare_exchange_strong(&thread->state, &expected, (u32)BR__THREAD_DETACHING)) {
    return BR_STATUS_INVALID_STATE;
  }

  control = (br__thread_control *)thread->control;
  /* Closing the only handle is Win32's detach: the thread runs to completion and
     the OS reclaims it. */
  if (!CloseHandle(control->native)) {
    br_atomic_store_explicit(&thread->state, (u32)BR__THREAD_FRESH, BR_ATOMIC_RELEASE);
    return BR_STATUS_INVALID_STATE;
  }
  thread->control = NULL;
  br_atomic_store_explicit(&thread->state, (u32)BR__THREAD_DETACHED, BR_ATOMIC_RELEASE);
  br__thread_control_release(control);
  return BR_STATUS_OK;
}

void br_thread_yield(void) {
  (void)SwitchToThread();
}

#endif /* defined(_WIN32) */
