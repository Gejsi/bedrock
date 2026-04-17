#include <assert.h>
#include <string.h>

#include <bedrock.h>

static void test_buddy_init_validation(void) {
  br_buddy_allocator buddy;
  static _Alignas(64) u8 aligned[256];
  static _Alignas(64) u8 too_small[32];

  memset(&buddy, 0, sizeof(buddy));

  assert(br_buddy_allocator_init(NULL, aligned, sizeof(aligned), 16u) ==
         BR_STATUS_INVALID_ARGUMENT);
  assert(br_buddy_allocator_init(&buddy, NULL, sizeof(aligned), 16u) == BR_STATUS_INVALID_ARGUMENT);
  assert(br_buddy_allocator_init(&buddy, aligned, 192u, 16u) == BR_STATUS_INVALID_ARGUMENT);
  assert(br_buddy_allocator_init(&buddy, aligned, sizeof(aligned), 24u) ==
         BR_STATUS_INVALID_ARGUMENT);
  assert(br_buddy_allocator_init(&buddy, too_small, sizeof(too_small), 16u) ==
         BR_STATUS_INVALID_ARGUMENT);
  assert(br_buddy_allocator_init(&buddy, aligned + 1, 128u, 16u) == BR_STATUS_INVALID_ARGUMENT);
}

static void test_buddy_basic_alloc_and_coalescence(void) {
  br_buddy_allocator buddy;
  br_alloc_result a;
  br_alloc_result b;
  br_alloc_result whole;
  static _Alignas(64) u8 buffer[256];

  memset(&buddy, 0, sizeof(buddy));
  assert(br_buddy_allocator_init(&buddy, buffer, sizeof(buffer), 16u) == BR_STATUS_OK);

  a = br_buddy_allocator_alloc_uninit(&buddy, 1u);
  b = br_buddy_allocator_alloc_uninit(&buddy, 1u);
  assert(a.status == BR_STATUS_OK);
  assert(b.status == BR_STATUS_OK);
  assert(a.ptr != NULL);
  assert(b.ptr != NULL);
  assert(a.ptr != b.ptr);
  assert((((uptr)a.ptr) & 15u) == 0u);
  assert((((uptr)b.ptr) & 15u) == 0u);

  assert(br_buddy_allocator_free(&buddy, a.ptr) == BR_STATUS_OK);
  assert(br_buddy_allocator_free(&buddy, b.ptr) == BR_STATUS_OK);

  whole = br_buddy_allocator_alloc_uninit(&buddy, 200u);
  assert(whole.status == BR_STATUS_OK);
  assert(whole.ptr != NULL);

  br_buddy_allocator_free_all(&buddy);
  whole = br_buddy_allocator_alloc_uninit(&buddy, 200u);
  assert(whole.status == BR_STATUS_OK);
  assert(whole.ptr != NULL);
}

static void test_buddy_allocator_adapter_resize(void) {
  br_buddy_allocator buddy;
  br_allocator allocator;
  br_alloc_result a;
  br_alloc_result same;
  br_alloc_result grown;
  static _Alignas(64) u8 buffer[256];

  memset(&buddy, 0, sizeof(buddy));
  assert(br_buddy_allocator_init(&buddy, buffer, sizeof(buffer), 16u) == BR_STATUS_OK);

  allocator = br_buddy_allocator_allocator(&buddy);
  a = br_allocator_alloc(allocator, 24u, 8u);
  assert(a.status == BR_STATUS_OK);
  assert(a.ptr != NULL);
  memset(a.ptr, 0x5a, a.size);

  same = br_allocator_resize(allocator, a.ptr, a.size, a.size, 8u);
  assert(same.status == BR_STATUS_OK);
  assert(same.ptr == a.ptr);

  grown = br_allocator_resize(allocator, a.ptr, a.size, 40u, 8u);
  assert(grown.status == BR_STATUS_OK);
  assert(grown.ptr != NULL);
  assert(memcmp(grown.ptr, a.ptr, a.size) == 0);
  assert(((u8 *)grown.ptr)[a.size] == 0u);
  assert(br_allocator_free(allocator, grown.ptr, grown.size) == BR_STATUS_OK);
}

static void test_buddy_invalid_pointer_free(void) {
  br_buddy_allocator buddy;
  static _Alignas(64) u8 buffer[256];

  memset(&buddy, 0, sizeof(buddy));
  assert(br_buddy_allocator_init(&buddy, buffer, sizeof(buffer), 16u) == BR_STATUS_OK);
  assert(br_buddy_allocator_free(&buddy, buffer + 8) == BR_STATUS_INVALID_ARGUMENT);
}

int main(void) {
  test_buddy_init_validation();
  test_buddy_basic_alloc_and_coalescence();
  test_buddy_allocator_adapter_resize();
  test_buddy_invalid_pointer_free();
  return 0;
}
