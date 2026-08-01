#ifndef BEDROCK_TESTS_STRICT_ALLOCATOR_H
#define BEDROCK_TESTS_STRICT_ALLOCATOR_H

#include <bedrock/mem/alloc.h>

typedef struct test_strict_allocation {
  void *ptr;
  size_t size;
} test_strict_allocation;

typedef struct test_strict_allocator {
  test_strict_allocation allocations[16];
  size_t allocation_count;
  size_t size_errors;
} test_strict_allocator;

static br_alloc_result test_strict_alloc_result(void *ptr, size_t size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static size_t test_strict_find(const test_strict_allocator *strict, const void *ptr) {
  size_t i;

  for (i = 0u; i < strict->allocation_count; i += 1u) {
    if (strict->allocations[i].ptr == ptr) {
      return i;
    }
  }
  return SIZE_MAX;
}

static br_alloc_result test_strict_allocator_proc(void *context, const br_alloc_request *request) {
  test_strict_allocator *strict = (test_strict_allocator *)context;
  br_alloc_result result;
  size_t index;

  switch (request->op) {
    case BR_ALLOC_OP_ALLOC:
    case BR_ALLOC_OP_ALLOC_UNINIT:
      result = br_allocator_call(br_allocator_heap(), request);
      if (result.status != BR_STATUS_OK || result.ptr == NULL) {
        return result;
      }
      if (strict->allocation_count == BR_ARRAY_COUNT(strict->allocations)) {
        (void)br_allocator_free(br_allocator_heap(), result.ptr, result.size);
        return test_strict_alloc_result(NULL, 0u, BR_STATUS_OUT_OF_MEMORY);
      }
      strict->allocations[strict->allocation_count].ptr = result.ptr;
      strict->allocations[strict->allocation_count].size = result.size;
      strict->allocation_count += 1u;
      return result;

    case BR_ALLOC_OP_RESIZE:
    case BR_ALLOC_OP_RESIZE_UNINIT:
    case BR_ALLOC_OP_FREE:
      if (request->ptr == NULL) {
        return br_allocator_call(br_allocator_heap(), request);
      }
      index = test_strict_find(strict, request->ptr);
      if (index == SIZE_MAX || strict->allocations[index].size != request->old_size) {
        strict->size_errors += 1u;
        return test_strict_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
      }

      result = br_allocator_call(br_allocator_heap(), request);
      if (result.status != BR_STATUS_OK) {
        return result;
      }
      if (request->op == BR_ALLOC_OP_FREE || request->size == 0u) {
        strict->allocation_count -= 1u;
        strict->allocations[index] = strict->allocations[strict->allocation_count];
      } else {
        strict->allocations[index].ptr = result.ptr;
        strict->allocations[index].size = result.size;
      }
      return result;

    case BR_ALLOC_OP_RESET:
      return test_strict_alloc_result(NULL, 0u, BR_STATUS_NOT_SUPPORTED);
  }

  return test_strict_alloc_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
}

static br_allocator test_strict_allocator_make(test_strict_allocator *strict) {
  br_allocator allocator;

  allocator.fn = test_strict_allocator_proc;
  allocator.ctx = strict;
  return allocator;
}

#endif
