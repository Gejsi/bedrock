#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* pthread_setname_np on glibc */
#endif

#include <bedrock/thread/thread.h>
#include <bedrock/mem/alloc.h>

#if !defined(_WIN32)

#include <errno.h>
#include <pthread.h>
#include <sched.h>

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <pthread_np.h>
#endif

/* INERT is 0 so a zero-initialized handle is inert. JOINING and DETACHING claim
   the native handle while the corresponding OS operation is in progress. */
enum {
  BR__THREAD_INERT = 0,
  BR__THREAD_FRESH,
  BR__THREAD_JOINING,
  BR__THREAD_JOINED,
  BR__THREAD_DETACHING,
  BR__THREAD_DETACHED
};

/* Keep one portable copied-name limit. macOS accepts at most 63 bytes plus NUL;
   Linux further truncates to 15 bytes at apply time. */
#define BR__THREAD_NAME_CAP 64u

typedef struct br__thread_control {
  br_atomic_u32 refs;
  br_thread_fn fn;
  void *arg;
  char name[BR__THREAD_NAME_CAP];
  bool has_name;
  int result;
  pthread_t native;
} br__thread_control;

static void br__thread_control_free(br__thread_control *control) {
  BR_UNUSED(br_allocator_free(br_allocator_heap(), control, sizeof(*control)));
}

static void br__thread_control_release(br__thread_control *control) {
  if (br_atomic_sub_explicit(&control->refs, 1u, BR_ATOMIC_ACQ_REL) == 1u) {
    br__thread_control_free(control);
  }
}

static pthread_key_t br__thread_control_key;
static pthread_once_t br__thread_control_key_once = PTHREAD_ONCE_INIT;
static int br__thread_control_key_error = 0;

static void br__thread_control_destructor(void *raw) {
  br__thread_control_release((br__thread_control *)raw);
}

static void br__thread_control_key_create(void) {
  br__thread_control_key_error =
    pthread_key_create(&br__thread_control_key, br__thread_control_destructor);
}

static br_status br__thread_create_error(int error) {
  switch (error) {
    case EAGAIN:
#if defined(ENOMEM) && ENOMEM != EAGAIN
    case ENOMEM:
#endif
      return BR_STATUS_OUT_OF_MEMORY;
    default:
      return BR_STATUS_INVALID_STATE;
  }
}

static void br__thread_apply_name(const char *name) {
#if defined(__linux__)
  /* Linux caps the name at 15 bytes + NUL; copy into a small buffer to truncate. */
  char buf[16];
  size_t i;
  for (i = 0u; i + 1u < sizeof(buf) && name[i] != '\0'; ++i) {
    buf[i] = name[i];
  }
  buf[i] = '\0';
  (void)pthread_setname_np(pthread_self(), buf);
#elif defined(__APPLE__) && defined(__MACH__)
  /* macOS names only the calling thread; that is why naming happens here. */
  (void)pthread_setname_np(name);
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
  pthread_set_name_np(pthread_self(), name);
#elif defined(__NetBSD__)
  (void)pthread_setname_np(pthread_self(), "%s", (void *)name);
#else
  (void)name; /* naming unsupported on this target; ignored, never fatal */
#endif
}

static void br__thread_join_rollback(void *raw) {
  br_thread *thread = (br_thread *)raw;
  br_atomic_store_explicit(&thread->state, (u32)BR__THREAD_FRESH, BR_ATOMIC_RELEASE);
}

static void br__thread_run(br__thread_control *control) {
  if (control->has_name) {
    br__thread_apply_name(control->name);
  }
  control->result = control->fn(control->arg);
}

static void *br__thread_trampoline(void *raw) {
  br__thread_control *control = (br__thread_control *)raw;

  if (pthread_setspecific(br__thread_control_key, control) != 0) {
    /* Preserve ownership even if the TLS slot cannot be installed. This rare
       path still releases on normal return, pthread_exit, or cancellation. */
    pthread_cleanup_push(br__thread_control_destructor, control);
    br__thread_run(control);
    pthread_cleanup_pop(1);
    return NULL;
  }

  br__thread_run(control);
  return NULL;
}

static br_status
br__thread_spawn(br_thread *thread, br_thread_fn fn, void *arg, const br_thread_options *options) {
  br_alloc_result allocation;
  br__thread_control *control;
  pthread_attr_t attr;
  pthread_attr_t *attr_ptr = NULL;
  int rc;

  if (thread == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  /* Leave the handle inert until spawn succeeds. */
  br_atomic_init(&thread->state, (u32)BR__THREAD_INERT);
  thread->control = NULL;
  if (fn == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  rc = pthread_once(&br__thread_control_key_once, br__thread_control_key_create);
  if (rc != 0) {
    return br__thread_create_error(rc);
  }
  if (br__thread_control_key_error != 0) {
    return br__thread_create_error(br__thread_control_key_error);
  }

  if (options != NULL && options->stack_size != 0u) {
    rc = pthread_attr_init(&attr);
    if (rc != 0) {
      return br__thread_create_error(rc);
    }
    rc = pthread_attr_setstacksize(&attr, options->stack_size);
    if (rc != 0) {
      (void)pthread_attr_destroy(&attr);
      return BR_STATUS_OUT_OF_RANGE;
    }
    attr_ptr = &attr;
  }

  allocation =
    br_allocator_alloc_uninit(br_allocator_heap(), sizeof(*control), _Alignof(br__thread_control));
  if (allocation.status != BR_STATUS_OK) {
    if (attr_ptr != NULL) {
      (void)pthread_attr_destroy(attr_ptr);
    }
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

  rc = pthread_create(&control->native, attr_ptr, br__thread_trampoline, control);

  if (attr_ptr != NULL) {
    (void)pthread_attr_destroy(attr_ptr);
  }

  if (rc != 0) {
    br__thread_control_free(control);
    if (rc == EINVAL && attr_ptr != NULL) {
      return BR_STATUS_OUT_OF_RANGE;
    }
    return br__thread_create_error(rc);
  }

  /* The handle and worker now own one control-block reference each. */
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
  int rc;

  if (thread == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  expected = (u32)BR__THREAD_FRESH;
  if (!br_atomic_compare_exchange_strong(&thread->state, &expected, (u32)BR__THREAD_JOINING)) {
    /* Not joinable: double-join, join-after-detach, or never created. */
    return BR_STATUS_INVALID_STATE;
  }

  control = (br__thread_control *)thread->control;
  if (pthread_equal(pthread_self(), control->native)) {
    br_atomic_store_explicit(&thread->state, (u32)BR__THREAD_FRESH, BR_ATOMIC_RELEASE);
    return BR_STATUS_INVALID_ARGUMENT;
  }

  pthread_cleanup_push(br__thread_join_rollback, thread);
  rc = pthread_join(control->native, NULL);
  pthread_cleanup_pop(0);
  if (rc != 0) {
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
  int rc;

  if (thread == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  expected = (u32)BR__THREAD_FRESH;
  if (!br_atomic_compare_exchange_strong(&thread->state, &expected, (u32)BR__THREAD_DETACHING)) {
    return BR_STATUS_INVALID_STATE;
  }

  control = (br__thread_control *)thread->control;
  rc = pthread_detach(control->native);
  if (rc != 0) {
    br_atomic_store_explicit(&thread->state, (u32)BR__THREAD_FRESH, BR_ATOMIC_RELEASE);
    return BR_STATUS_INVALID_STATE;
  }
  thread->control = NULL;
  br_atomic_store_explicit(&thread->state, (u32)BR__THREAD_DETACHED, BR_ATOMIC_RELEASE);
  br__thread_control_release(control);
  return BR_STATUS_OK;
}

void br_thread_yield(void) {
  (void)sched_yield();
}

#endif /* !defined(_WIN32) */
