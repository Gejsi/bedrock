#include <assert.h>

#include <bedrock.h>

static void test_atomic_load_store(void) {
  br_atomic_i32 value;

  br_atomic_init(&value, 1);
  assert(br_atomic_load(&value) == 1);

  br_atomic_store(&value, 7);
  assert(br_atomic_load_explicit(&value, BR_ATOMIC_RELAXED) == 7);
}

static void test_atomic_fetch_ops(void) {
  br_atomic_u32 bits;

  br_atomic_init(&bits, 3u);
  assert(br_atomic_add(&bits, 4u) == 3u);
  assert(br_atomic_load(&bits) == 7u);

  assert(br_atomic_sub_explicit(&bits, 2u, BR_ATOMIC_RELAXED) == 7u);
  assert(br_atomic_load(&bits) == 5u);

  assert(br_atomic_and(&bits, 6u) == 5u);
  assert(br_atomic_load(&bits) == 4u);

  assert(br_atomic_or_explicit(&bits, 3u, BR_ATOMIC_RELAXED) == 4u);
  assert(br_atomic_load(&bits) == 7u);

  assert(br_atomic_xor(&bits, 6u) == 7u);
  assert(br_atomic_load(&bits) == 1u);
}

static void test_atomic_exchange(void) {
  br_atomic_i64 value;

  br_atomic_init(&value, 12);
  assert(br_atomic_exchange(&value, 42) == 12);
  assert(br_atomic_load(&value) == 42);
}

static void test_atomic_compare_exchange(void) {
  br_atomic_u32 value;
  u32 expected;

  br_atomic_init(&value, 10u);

  expected = 10u;
  assert(br_atomic_compare_exchange_strong(&value, &expected, 20u));
  assert(expected == 10u);
  assert(br_atomic_load(&value) == 20u);

  expected = 10u;
  assert(!br_atomic_compare_exchange_weak_explicit(
    &value, &expected, 30u, BR_ATOMIC_ACQ_REL, BR_ATOMIC_ACQUIRE));
  assert(expected == 20u);
  assert(br_atomic_load(&value) == 20u);
}

static void test_atomic_fences_and_relax(void) {
  br_atomic_signal_fence(BR_ATOMIC_SEQ_CST);
  br_atomic_thread_fence(BR_ATOMIC_SEQ_CST);
  br_cpu_relax();
}

int main(void) {
  test_atomic_load_store();
  test_atomic_fetch_ops();
  test_atomic_exchange();
  test_atomic_compare_exchange();
  test_atomic_fences_and_relax();
  return 0;
}
