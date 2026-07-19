#ifndef BEDROCK_THREAD_THREAD_H
#define BEDROCK_THREAD_THREAD_H

#include <bedrock/base.h>
#include <bedrock/sync/primitives.h>
#include <bedrock/sync/thread.h>

#if !defined(_WIN32)
#include <pthread.h>
#endif

BR_EXTERN_C_BEGIN

/*
A thin, explicit OS-thread wrapper: thread lifecycle only. Blocking and
coordination live in the sync module. The handle is caller-allocated (place it
on the stack or embed it in your own struct); it needs no allocator because the
OS owns the thread stack.

The spawned thread holds no reference to this handle after it starts, so a
detached caller may safely let the handle leave scope while the thread runs.
*/

/*
Thread entry point. Returns an int exit code, retrievable via br_thread_join.
Richer results travel through the `arg` struct (write a field, read it after
join).
*/
typedef int (*br_thread_fn)(void *arg);

typedef struct br_thread {
  /* Opaque to consumers. Carries ONLY: the native thread handle, the native
     thread id (for the self-join guard), and an atomic lifecycle state. It
     carries NO user data and NO exit code. */
  br_atomic_u32 state;
#if defined(_WIN32)
  void *native;            /* HANDLE from _beginthreadex */
  unsigned long native_id; /* thread id from _beginthreadex's thrdaddr out-param */
#else
  pthread_t native; /* creator-written; used for the self-join comparison */
#endif
} br_thread;

typedef struct br_thread_options {
  size_t stack_size; /* 0 = OS default */
  const char *name;  /* best-effort debug name, may be NULL; COPIED at create */
} br_thread_options;

/*
Spawn a thread running `fn(arg)`. On success `*thread` is a live, joinable
handle; on failure it is left inert (as if zero-initialized) and no thread
starts. `fn` and `thread` must be non-NULL (else BR_STATUS_INVALID_ARGUMENT);
resource exhaustion returns BR_STATUS_OUT_OF_MEMORY.
*/
br_status br_thread_create(br_thread *thread, br_thread_fn fn, void *arg);

/*
Like br_thread_create, with options. A stack size or name the platform cannot
honor never fails the create — options are best-effort.
*/
br_status br_thread_create_ex(br_thread *thread,
                              br_thread_fn fn,
                              void *arg,
                              const br_thread_options *options);

/*
Wait for the thread to finish and transition the handle to JOINED. If
`exit_code` is non-NULL it receives the value `fn` returned. Returns
BR_STATUS_INVALID_ARGUMENT if a thread joins its own handle (deadlock
avoidance), and BR_STATUS_INVALID_STATE if the handle is not joinable
(already joined, detached, or never created).
*/
br_status br_thread_join(br_thread *thread, int *exit_code);

/*
Detach the thread and transition the handle to DETACHED: the thread runs to
completion on its own and its resources are reclaimed automatically. Its exit
code becomes unretrievable. Returns BR_STATUS_INVALID_STATE if the handle is not
joinable.
*/
br_status br_thread_detach(br_thread *thread);

/*
Hint to the scheduler to yield the remainder of the current time slice.
*/
void br_thread_yield(void);

BR_EXTERN_C_END

#endif
