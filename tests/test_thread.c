#include <assert.h>
#include <string.h>

#include <bedrock.h>

/* A worker that writes a value through its arg struct and returns an exit code. */
typedef struct worker_state {
  int input;
  int written; /* thread writes here; main reads after join (happens-before) */
} worker_state;

static int worker_double(void *arg) {
  worker_state *s = (worker_state *)arg;
  s->written = s->input * 2;
  return s->input + 1;
}

static int worker_return_negative(void *arg) {
  BR_UNUSED(arg);
  return -42;
}

static int worker_zero(void *arg) {
  BR_UNUSED(arg);
  return 0;
}

/* For the self-join test: the thread joins its OWN handle and stores the status.
   `done` lets main wait for the self-join attempt to finish BEFORE main joins, so
   main's join can't race ahead and win the CAS (which would turn the worker's
   self-join into INVALID_STATE instead of the INVALID_ARGUMENT we are testing). */
typedef struct self_join_state {
  br_thread *self;
  br_sema ready; /* main posts once the handle is FRESH */
  br_sema done;  /* worker posts after recording its self-join result */
  br_status result;
} self_join_state;

static int worker_self_join(void *arg) {
  self_join_state *s = (self_join_state *)arg;
  br_sema_wait(&s->ready); /* wait until main has stored the FRESH handle */
  s->result = br_thread_join(s->self, NULL);
  br_sema_post(&s->done, 1u);
  return 0;
}

/* For the detach UAF regression: the thread signals it ran, AFTER the creator's
   handle has left scope. Uses a heap-free, caller-owned sema that outlives the
   handle. */
typedef struct detach_state {
  br_sema started;  /* thread posts when it begins */
  br_sema release;  /* main posts to let the thread finish */
  br_sema finished; /* thread posts just before returning */
} detach_state;

static int worker_detached(void *arg) {
  detach_state *s = (detach_state *)arg;
  br_sema_post(&s->started, 1u);
  br_sema_wait(&s->release);
  br_sema_post(&s->finished, 1u);
  return 0;
}

static void test_create_join_exit_code(void) {
  br_thread t;
  worker_state s = {21, 0};
  int code = 0;

  assert(br_thread_create(&t, worker_double, &s) == BR_STATUS_OK);
  assert(br_thread_join(&t, &code) == BR_STATUS_OK);
  assert(code == 22);      /* input + 1 */
  assert(s.written == 42); /* happens-before: join is the edge, no atomic needed */
}

static void test_exit_code_negative(void) {
  br_thread t;
  int code = 0;

  assert(br_thread_create(&t, worker_return_negative, NULL) == BR_STATUS_OK);
  assert(br_thread_join(&t, &code) == BR_STATUS_OK);
  assert(code == -42); /* negative round-trips through the substrate channel */
}

static void test_join_null_exit_code(void) {
  br_thread t;
  assert(br_thread_create(&t, worker_zero, NULL) == BR_STATUS_OK);
  assert(br_thread_join(&t, NULL) == BR_STATUS_OK); /* exit_code may be NULL */
}

static void test_zeroed_handle_is_inert(void) {
  br_thread t;
  memset(&t, 0, sizeof(t));
  assert(br_thread_join(&t, NULL) == BR_STATUS_INVALID_STATE);
  assert(br_thread_detach(&t) == BR_STATUS_INVALID_STATE);
}

static void test_double_join(void) {
  br_thread t;
  assert(br_thread_create(&t, worker_zero, NULL) == BR_STATUS_OK);
  assert(br_thread_join(&t, NULL) == BR_STATUS_OK);
  assert(br_thread_join(&t, NULL) == BR_STATUS_INVALID_STATE);
}

static void test_double_detach(void) {
  br_thread t;
  assert(br_thread_create(&t, worker_zero, NULL) == BR_STATUS_OK);
  assert(br_thread_detach(&t) == BR_STATUS_OK);
  assert(br_thread_detach(&t) == BR_STATUS_INVALID_STATE);
}

static void test_join_after_detach(void) {
  br_thread t;
  assert(br_thread_create(&t, worker_zero, NULL) == BR_STATUS_OK);
  assert(br_thread_detach(&t) == BR_STATUS_OK);
  assert(br_thread_join(&t, NULL) == BR_STATUS_INVALID_STATE);
}

static void test_self_join_rejected(void) {
  br_thread t;
  self_join_state s;
  int code = 0;

  s.self = &t;
  br_sema_init(&s.ready, 0u);
  br_sema_init(&s.done, 0u);
  s.result = BR_STATUS_OK;

  assert(br_thread_create(&t, worker_self_join, &s) == BR_STATUS_OK);
  /* Handle is now FRESH; let the thread attempt to join itself, then wait for
     it to record the result before we join (so we don't win the CAS first). */
  br_sema_post(&s.ready, 1u);
  br_sema_wait(&s.done);
  assert(s.result == BR_STATUS_INVALID_ARGUMENT);

  /* A rejected self-join left the handle FRESH, so THIS thread can still join
     it. */
  assert(br_thread_join(&t, &code) == BR_STATUS_OK);
  br_sema_destroy(&s.ready);
  br_sema_destroy(&s.done);
}

static void test_detach_scope_exit_uaf(void) {
  /* The UAF regression: detach the thread, let its br_thread handle leave scope
     while the thread is still running, then let it finish. Under ASan the
     handle's storage is stack that gets reused; if the thread touched the handle
     post-detach this would be a use-after-free. The core invariant forbids it. */
  detach_state s;
  br_sema_init(&s.started, 0u);
  br_sema_init(&s.release, 0u);
  br_sema_init(&s.finished, 0u);

  {
    br_thread t;
    assert(br_thread_create(&t, worker_detached, &s) == BR_STATUS_OK);
    br_sema_wait(&s.started); /* thread is running */
    assert(br_thread_detach(&t) == BR_STATUS_OK);
    /* t leaves scope here while the thread is still alive and blocked. */
  }

  /* Clobber the stack region the handle occupied, to make any stale thread-side
     access land on poisoned/overwritten memory. */
  {
    volatile unsigned char scratch[512];
    size_t i;
    for (i = 0u; i < sizeof(scratch); ++i) {
      scratch[i] = (unsigned char)i;
    }
    assert(scratch[0] == 0u);
  }

  br_sema_post(&s.release, 1u); /* let the detached thread run to completion */
  br_sema_wait(&s.finished);    /* proof it executed after the handle's scope exit */
  br_sema_destroy(&s.started);
  br_sema_destroy(&s.release);
  br_sema_destroy(&s.finished);
}

static void test_options_stack_and_name(void) {
  br_thread t;
  br_thread_options opts;
  worker_state s = {5, 0};
  int code = 0;

  memset(&opts, 0, sizeof(opts));
  opts.stack_size = 256u * 1024u;
  opts.name = "br-worker";

  assert(br_thread_create_ex(&t, worker_double, &s, &opts) == BR_STATUS_OK);
  assert(br_thread_join(&t, &code) == BR_STATUS_OK);
  assert(code == 6);
  assert(s.written == 10);
}

static void test_options_overlong_name(void) {
  br_thread t;
  br_thread_options opts;
  int code = 0;

  memset(&opts, 0, sizeof(opts));
  /* Longer than any platform's cap (Linux 15, macOS 64): best-effort, must not
     fail the create nor crash. */
  opts.name = "this-name-is-deliberately-far-too-long-for-any-platform-thread-name-limit";

  assert(br_thread_create_ex(&t, worker_return_negative, NULL, &opts) == BR_STATUS_OK);
  assert(br_thread_join(&t, &code) == BR_STATUS_OK);
  assert(code == -42);
}

static void test_create_invalid_args(void) {
  br_thread t;

  /* NULL fn -> INVALID_ARGUMENT, handle left inert (join then INVALID_STATE). */
  memset(&t, 0, sizeof(t));
  assert(br_thread_create(&t, NULL, NULL) == BR_STATUS_INVALID_ARGUMENT);
  assert(br_thread_join(&t, NULL) == BR_STATUS_INVALID_STATE);

  /* NULL thread handle -> INVALID_ARGUMENT. */
  assert(br_thread_create(NULL, worker_zero, NULL) == BR_STATUS_INVALID_ARGUMENT);
}

static void test_many_threads(void) {
  enum { N = 16 };
  br_thread threads[N];
  worker_state states[N];
  int i;

  for (i = 0; i < N; ++i) {
    states[i].input = i;
    states[i].written = 0;
    assert(br_thread_create(&threads[i], worker_double, &states[i]) == BR_STATUS_OK);
  }
  for (i = 0; i < N; ++i) {
    int code = 0;
    assert(br_thread_join(&threads[i], &code) == BR_STATUS_OK);
    assert(code == i + 1);
    assert(states[i].written == i * 2);
  }
}

int main(void) {
  test_create_join_exit_code();
  test_exit_code_negative();
  test_join_null_exit_code();
  test_zeroed_handle_is_inert();
  test_double_join();
  test_double_detach();
  test_join_after_detach();
  test_self_join_rejected();
  test_detach_scope_exit_uaf();
  test_options_stack_and_name();
  test_options_overlong_name();
  test_create_invalid_args();
  test_many_threads();
  br_thread_yield(); /* smoke: exercises the yield entry point */
  return 0;
}
