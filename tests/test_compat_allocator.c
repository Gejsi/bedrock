#include <assert.h>
#include <string.h>

#include <bedrock.h>

typedef struct test_in_place_allocator {
  u8 *buffer;
  usize capacity;
  bool in_use;
} test_in_place_allocator;

static br_alloc_result test_in_place_allocator_fn(void *ctx, const br_alloc_request *req) {
  test_in_place_allocator *allocator = (test_in_place_allocator *)ctx;
  br_alloc_result result = {0};

  if (allocator == NULL || req == NULL) {
    result.status = BR_STATUS_INVALID_ARGUMENT;
    return result;
  }

  switch (req->op) {
    case BR_ALLOC_OP_ALLOC:
    case BR_ALLOC_OP_ALLOC_UNINIT:
      if (req->size > allocator->capacity) {
        result.status = BR_STATUS_OUT_OF_MEMORY;
        return result;
      }
      allocator->in_use = true;
      if (req->op == BR_ALLOC_OP_ALLOC) {
        memset(allocator->buffer, 0, req->size);
      }
      result.ptr = allocator->buffer;
      result.size = req->size;
      result.status = BR_STATUS_OK;
      return result;
    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
      if (req->ptr == NULL) {
        br_alloc_request alloc_req = *req;

        alloc_req.op = req->op == BR_ALLOC_OP_RESIZE ? BR_ALLOC_OP_ALLOC : BR_ALLOC_OP_ALLOC_UNINIT;
        return test_in_place_allocator_fn(ctx, &alloc_req);
      }
      if (req->ptr != allocator->buffer) {
        result.status = BR_STATUS_INVALID_ARGUMENT;
        return result;
      }
      if (req->size > allocator->capacity) {
        result.status = BR_STATUS_OUT_OF_MEMORY;
        return result;
      }
      if (req->op == BR_ALLOC_OP_RESIZE && req->size > req->old_size) {
        memset(allocator->buffer + req->old_size, 0, req->size - req->old_size);
      }
      allocator->in_use = req->size != 0u;
      result.ptr = req->size != 0u ? allocator->buffer : NULL;
      result.size = req->size;
      result.status = BR_STATUS_OK;
      return result;
    case BR_ALLOC_OP_FREE:
      allocator->in_use = false;
      result.status = BR_STATUS_OK;
      return result;
    case BR_ALLOC_OP_RESET:
      allocator->in_use = false;
      result.status = BR_STATUS_OK;
      return result;
  }

  result.status = BR_STATUS_INVALID_ARGUMENT;
  return result;
}

static void test_compat_wraps_stack_resize_and_free(void) {
  br_stack stack;
  br_compat_allocator compat;
  br_allocator compat_allocator;
  br_alloc_result direct;
  br_alloc_result alloc;
  br_alloc_result grown;
  static _Alignas(64) u8 backing[256];

  memset(&stack, 0, sizeof(stack));
  br_stack_init(&stack, backing, sizeof(backing));

  direct = br_stack_alloc(&stack, 16u, 8u);
  assert(direct.status == BR_STATUS_OK);
  assert(br_stack_resize(&stack, direct.ptr, 123u, 24u, 8u).status == BR_STATUS_INVALID_ARGUMENT);

  br_stack_free_all(&stack);
  br_compat_allocator_init(&compat, br_stack_allocator(&stack));
  compat_allocator = br_compat_allocator_allocator(&compat);

  alloc = br_allocator_alloc(compat_allocator, 16u, 8u);
  assert(alloc.status == BR_STATUS_OK);
  assert(alloc.ptr != NULL);
  memset(alloc.ptr, 0x4d, alloc.size);

  grown = br_allocator_resize(compat_allocator, alloc.ptr, 123u, 24u, 8u);
  assert(grown.status == BR_STATUS_OK);
  assert(grown.ptr != NULL);
  assert(memcmp(grown.ptr, alloc.ptr, alloc.size) == 0);
  assert(((u8 *)grown.ptr)[alloc.size] == 0u);
  assert(br_allocator_free(compat_allocator, grown.ptr, 999u) == BR_STATUS_OK);
}

static void test_compat_shifts_payload_when_alignment_grows(void) {
  test_in_place_allocator parent_state;
  br_compat_allocator compat;
  br_allocator compat_allocator;
  br_alloc_result alloc;
  br_alloc_result grown;
  static _Alignas(64) u8 parent_buffer[128];
  usize i;

  memset(&parent_state, 0, sizeof(parent_state));
  memset(parent_buffer, 0, sizeof(parent_buffer));
  parent_state.buffer = parent_buffer;
  parent_state.capacity = sizeof(parent_buffer);

  br_compat_allocator_init(&compat,
                           (br_allocator){.fn = test_in_place_allocator_fn, .ctx = &parent_state});
  compat_allocator = br_compat_allocator_allocator(&compat);

  alloc = br_allocator_alloc_uninit(compat_allocator, 16u, 8u);
  assert(alloc.status == BR_STATUS_OK);
  assert(alloc.ptr == parent_buffer + 16);
  for (i = 0u; i < alloc.size; ++i) {
    ((u8 *)alloc.ptr)[i] = (u8)(0x80u + i);
  }

  grown = br_allocator_resize(compat_allocator, alloc.ptr, 0u, 24u, 32u);
  assert(grown.status == BR_STATUS_OK);
  assert(grown.ptr == parent_buffer + 32);
  for (i = 0u; i < alloc.size; ++i) {
    assert(((u8 *)grown.ptr)[i] == (u8)(0x80u + i));
  }
  for (i = alloc.size; i < grown.size; ++i) {
    assert(((u8 *)grown.ptr)[i] == 0u);
  }
}

static void test_compat_default_parent_uses_heap(void) {
  br_compat_allocator compat;
  br_allocator compat_allocator;
  br_alloc_result alloc;

  memset(&compat, 0, sizeof(compat));
  br_compat_allocator_init(&compat, (br_allocator){0});
  compat_allocator = br_compat_allocator_allocator(&compat);

  alloc = br_allocator_alloc(compat_allocator, 32u, 16u);
  assert(alloc.status == BR_STATUS_OK);
  assert(alloc.ptr != NULL);
  assert(br_allocator_free(compat_allocator, alloc.ptr, 0u) == BR_STATUS_OK);
}

int main(void) {
  test_compat_wraps_stack_resize_and_free();
  test_compat_shifts_payload_when_alignment_grows();
  test_compat_default_parent_uses_heap();
  return 0;
}
