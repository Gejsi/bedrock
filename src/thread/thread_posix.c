#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* pthread_setname_np on glibc */
#endif

#include <bedrock/thread/thread.h>

#if !defined(_WIN32)

#include <pthread.h>
#include <sched.h>
#include <string.h>

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <pthread_np.h>
#endif

/* Lifecycle states for the handle's atomic `state` field. INERT is 0 so a
   zero-initialized handle is inert (join/detach return INVALID_STATE). */
enum { BR__THREAD_INERT = 0, BR__THREAD_FRESH, BR__THREAD_JOINED, BR__THREAD_DETACHED };

/* Longest thread name any target honors is macOS's 64; the buffer holds a
   truncated copy. Linux further truncates to 15 at apply time. */
#define BR__THREAD_NAME_CAP 64u

/*
Startup control block. Lives on the CREATOR's stack inside br_thread_create and
must not be touched by the spawned thread after it posts the handshake, because
the creator's frame (and this block) can be gone the instant the handshake wait
returns.
*/
typedef struct br__thread_start {
  br_thread_fn fn;
  void *arg;
  char name[BR__THREAD_NAME_CAP];
  bool has_name;
  br_sema handshake;
} br__thread_start;

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

static void *br__thread_trampoline(void *raw) {
  br__thread_start *start = (br__thread_start *)raw;
  br_thread_fn fn;
  void *arg;
  char name[BR__THREAD_NAME_CAP];
  bool has_name;
  int code;

  /* Step 1: copy everything out of the creator-stack control block. This is the
     ONLY read of the block, and it happens before the handshake post. */
  fn = start->fn;
  arg = start->arg;
  has_name = start->has_name;
  if (has_name) {
    memcpy(name, start->name, sizeof(name));
  }

  /* Step 2: post the handshake. After this, `start` may be freed at any instant
     (its frame belongs to the creator), so it is NEVER touched again.
     br_sema_post's last user-space access of the sema is its RELEASE atomic
     add; the trailing futex wake passes only the address to the kernel, and the
     creator's waiter is loop-based, so posting into about-to-die memory is
     safe. This is the sem_post/sem_destroy discipline. */
  br_sema_post(&start->handshake, 1u);

  /* Step 3: name self (best-effort). Step 4: run. Step 5: return code via the
     substrate. The br_thread handle is never referenced anywhere here. */
  if (has_name) {
    br__thread_apply_name(name);
  }
  code = fn(arg);
  return (void *)(intptr_t)code;
}

static br_status
br__thread_spawn(br_thread *thread, br_thread_fn fn, void *arg, const br_thread_options *options) {
  br__thread_start start;
  pthread_attr_t attr;
  pthread_attr_t *attr_ptr = NULL;
  pthread_t native;
  int rc;

  if (thread == NULL || fn == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  /* Leave the handle inert until spawn succeeds. */
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

  if (options != NULL && options->stack_size != 0u) {
    if (pthread_attr_init(&attr) == 0) {
      (void)pthread_attr_setstacksize(&attr, options->stack_size);
      attr_ptr = &attr;
    }
  }

  rc = pthread_create(&native, attr_ptr, br__thread_trampoline, &start);

  if (attr_ptr != NULL) {
    (void)pthread_attr_destroy(attr_ptr);
  }

  if (rc != 0) {
    br_sema_destroy(&start.handshake);
    /* Resource exhaustion (EAGAIN) and other failures map to OOM. Handle stays
       inert. */
    return BR_STATUS_OUT_OF_MEMORY;
  }

  /* Wait for the trampoline to copy the control block. After this returns the
     block is done being read and may die with our frame. */
  br_sema_wait(&start.handshake);
  br_sema_destroy(&start.handshake);

  /* Creator-side identity write for the self-join guard: the thread never
     writes its own id into the handle (that would violate the core invariant).
     `native` is what pthread_create handed us. */
  thread->native = native;
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
  void *retval = NULL;

  if (thread == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  /* Self-join check BEFORE any state transition: a rejected self-join must
     leave the handle FRESH and joinable by another thread. Compare against the
     creator-written native handle only. */
  if (br_atomic_load_explicit(&thread->state, BR_ATOMIC_ACQUIRE) == (u32)BR__THREAD_FRESH &&
      pthread_equal(pthread_self(), thread->native)) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  expected = (u32)BR__THREAD_FRESH;
  if (!br_atomic_compare_exchange_strong(&thread->state, &expected, (u32)BR__THREAD_JOINED)) {
    /* Not joinable: double-join, join-after-detach, or never created. */
    return BR_STATUS_INVALID_STATE;
  }

  (void)pthread_join(thread->native, &retval);
  if (exit_code != NULL) {
    *exit_code = (int)(intptr_t)retval;
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

  (void)pthread_detach(thread->native);
  return BR_STATUS_OK;
}

void br_thread_yield(void) {
  (void)sched_yield();
}

#endif /* !defined(_WIN32) */
