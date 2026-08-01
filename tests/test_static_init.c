#include <assert.h>

#include <bedrock.h>

static br_atomic_u32 test_atomic = BR_ATOMIC_INIT(7u);
static br_futex test_futex = BR_FUTEX_INIT(8u);
static br_atomic_mutex test_atomic_mutex = BR_ATOMIC_MUTEX_INIT;
static br_atomic_sema test_atomic_sema = BR_ATOMIC_SEMA_INIT(9u);
static br_atomic_rw_mutex test_atomic_rw_mutex = BR_ATOMIC_RW_MUTEX_INIT;
static br_atomic_recursive_mutex test_atomic_recursive_mutex = BR_ATOMIC_RECURSIVE_MUTEX_INIT;
static br_atomic_cond test_atomic_cond = BR_ATOMIC_COND_INIT;

static br_mutex test_mutex = BR_MUTEX_INIT;
static br_rw_mutex test_rw_mutex = BR_RW_MUTEX_INIT;
static br_recursive_mutex test_recursive_mutex = BR_RECURSIVE_MUTEX_INIT;
static br_cond test_cond = BR_COND_INIT;
static br_sema test_sema = BR_SEMA_INIT(10u);

static br_wait_group test_wait_group = BR_WAIT_GROUP_INIT;
static br_once test_once = BR_ONCE_INIT;
static br_auto_reset_event test_auto_reset_event = BR_AUTO_RESET_EVENT_INIT;
static br_parker test_parker = BR_PARKER_INIT;
static br_one_shot_event test_one_shot_event = BR_ONE_SHOT_EVENT_INIT;
static br_ticket_mutex test_ticket_mutex = BR_TICKET_MUTEX_INIT;
static br_thread test_thread = BR_THREAD_INIT;

int main(void) {
  assert(br_atomic_load(&test_atomic) == 7u);
  assert(br_atomic_load(&test_futex) == 8u);
  assert(br_atomic_load(&test_atomic_mutex.state) == BR_ATOMIC_MUTEX_UNLOCKED);
  assert(br_atomic_load(&test_atomic_sema.count) == 9u);
  assert(br_atomic_load(&test_atomic_rw_mutex.state) == 0u);
  assert(br_atomic_load(&test_atomic_recursive_mutex.owner) == BR_THREAD_ID_INVALID);
  assert(test_atomic_recursive_mutex.recursion == 0u);
  assert(br_atomic_load(&test_atomic_cond.state) == 0u);

  assert(br_atomic_load(&test_mutex.impl.state) == BR_ATOMIC_MUTEX_UNLOCKED);
  assert(br_atomic_load(&test_rw_mutex.impl.state) == 0u);
  assert(br_atomic_load(&test_recursive_mutex.impl.owner) == BR_THREAD_ID_INVALID);
  assert(br_atomic_load(&test_cond.impl.state) == 0u);
  assert(br_atomic_load(&test_sema.impl.count) == 10u);

  assert(test_wait_group.counter == 0);
  assert(!br_atomic_load(&test_once.done));
  assert(br_atomic_load(&test_auto_reset_event.status) == 0);
  assert(br_atomic_load(&test_parker.state) == 0u);
  assert(br_atomic_load(&test_one_shot_event.state) == 0u);
  assert(br_atomic_load(&test_ticket_mutex.ticket) == 0u);
  assert(br_atomic_load(&test_ticket_mutex.serving) == 0u);
  assert(br_atomic_load(&test_thread.state) == 0u);
  assert(test_thread.control == NULL);
  return 0;
}
