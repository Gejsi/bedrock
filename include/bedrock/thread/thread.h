#ifndef BEDROCK_THREAD_THREAD_H
#define BEDROCK_THREAD_THREAD_H

#include <bedrock/base.h>
#include <bedrock/sync/atomic.h>

BR_EXTERN_C_BEGIN

/*
A thin, explicit OS-thread wrapper: thread lifecycle only. Blocking and
coordination live in the sync module. The handle is caller-allocated (place it
on the stack or embed it in your own struct).

The spawned thread never receives a reference to this handle, so a detached
caller may safely let the handle leave scope while the thread runs.

The handle storage must remain alive until every operation using it has
returned. In particular, if join and detach race, the losing call may return
before the winning call has finished using the handle.
*/

/*
Thread entry point. Returns an int exit code, retrievable via br_thread_join.
Richer results travel through the `arg` struct (write a field, read it after
join). The callback must return normally; calling a platform-native thread-exit
function bypasses Bedrock's result and cleanup contract.
*/
typedef int (*br_thread_fn)(void *arg);

typedef struct br_thread {
  /* Opaque implementation fields. Do not inspect or copy a live handle. */
  br_atomic_u32 state;
  void *control;
} br_thread;

#define BR_THREAD_INIT {.state = BR_ATOMIC_INIT(0u), .control = NULL}

typedef struct br_thread_options {
  size_t stack_size; /* 0 = OS default */
  const char *name;  /* best-effort debug name, may be NULL; COPIED at create */
} br_thread_options;

/*
Spawn a thread running `fn(arg)`. On success `*thread` is a live, joinable
handle; on failure it is left inert (as if zero-initialized) and no thread
starts. `fn` and `thread` must be non-NULL (else BR_STATUS_INVALID_ARGUMENT);
resource exhaustion returns BR_STATUS_OUT_OF_MEMORY. `thread` must not already
identify a live joinable thread. The callback may begin before this function
returns; it must synchronize with the creator before accessing `thread`.
*/
br_status br_thread_create(br_thread *thread, br_thread_fn fn, void *arg);

/*
Like br_thread_create, with options. Thread names are best-effort. A nonzero
stack size is load-bearing: unsupported or invalid values fail rather than
silently falling back to the platform default.
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
