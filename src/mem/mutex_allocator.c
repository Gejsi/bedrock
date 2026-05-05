#include <bedrock/mem/mutex_allocator.h>

static br_alloc_result br__mutex_allocator_result(void *ptr, usize size, br_status status) {
  br_alloc_result result;

  result.ptr = ptr;
  result.size = size;
  result.status = status;
  return result;
}

static br_allocator br__mutex_allocator_backing(const br_mutex_allocator *mutex_allocator) {
  if (mutex_allocator != NULL && mutex_allocator->backing.fn != NULL) {
    return mutex_allocator->backing;
  }

  return br_allocator_heap();
}

static br_alloc_result br__mutex_allocator_fn(void *ctx, const br_alloc_request *req) {
  br_mutex_allocator *mutex_allocator = (br_mutex_allocator *)ctx;
  br_allocator backing;
  br_alloc_result result;

  if (mutex_allocator == NULL || req == NULL) {
    return br__mutex_allocator_result(NULL, 0u, BR_STATUS_INVALID_ARGUMENT);
  }

  backing = br__mutex_allocator_backing(mutex_allocator);
  br_mutex_lock(&mutex_allocator->mutex);
  result = br_allocator_call(backing, req);
  br_mutex_unlock(&mutex_allocator->mutex);
  return result;
}

void br_mutex_allocator_init(br_mutex_allocator *mutex_allocator, br_allocator backing) {
  if (mutex_allocator == NULL) {
    return;
  }

  if (backing.fn == NULL) {
    backing = br_allocator_heap();
  }

  mutex_allocator->backing = backing;
  mutex_allocator->mutex = (br_mutex)BR_MUTEX_INIT;
}

br_allocator br_mutex_allocator_allocator(br_mutex_allocator *mutex_allocator) {
  br_allocator allocator;

  allocator.fn = br__mutex_allocator_fn;
  allocator.ctx = mutex_allocator;
  return allocator;
}
