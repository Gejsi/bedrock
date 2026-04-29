#include <bedrock/types.h>

#if !defined(_WIN32) && !defined(__unix__) && !defined(__APPLE__)

#include <bedrock/sync/primitives.h>

br_status br_mutex_init(br_mutex *mutex) {
  BR_UNUSED(mutex);
  return BR_STATUS_NOT_SUPPORTED;
}

void br_mutex_destroy(br_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_mutex_lock(br_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_mutex_unlock(br_mutex *mutex) {
  BR_UNUSED(mutex);
}

bool br_mutex_try_lock(br_mutex *mutex) {
  BR_UNUSED(mutex);
  return false;
}

br_status br_rw_mutex_init(br_rw_mutex *mutex) {
  BR_UNUSED(mutex);
  return BR_STATUS_NOT_SUPPORTED;
}

void br_rw_mutex_destroy(br_rw_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_rw_mutex_lock(br_rw_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_rw_mutex_unlock(br_rw_mutex *mutex) {
  BR_UNUSED(mutex);
}

bool br_rw_mutex_try_lock(br_rw_mutex *mutex) {
  BR_UNUSED(mutex);
  return false;
}

void br_rw_mutex_shared_lock(br_rw_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_rw_mutex_shared_unlock(br_rw_mutex *mutex) {
  BR_UNUSED(mutex);
}

bool br_rw_mutex_try_shared_lock(br_rw_mutex *mutex) {
  BR_UNUSED(mutex);
  return false;
}

br_status br_recursive_mutex_init(br_recursive_mutex *mutex) {
  BR_UNUSED(mutex);
  return BR_STATUS_NOT_SUPPORTED;
}

void br_recursive_mutex_destroy(br_recursive_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_recursive_mutex_lock(br_recursive_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_recursive_mutex_unlock(br_recursive_mutex *mutex) {
  BR_UNUSED(mutex);
}

bool br_recursive_mutex_try_lock(br_recursive_mutex *mutex) {
  BR_UNUSED(mutex);
  return false;
}

br_status br_cond_init(br_cond *cond) {
  BR_UNUSED(cond);
  return BR_STATUS_NOT_SUPPORTED;
}

void br_cond_destroy(br_cond *cond) {
  BR_UNUSED(cond);
}

void br_cond_wait(br_cond *cond, br_mutex *mutex) {
  BR_UNUSED(cond);
  BR_UNUSED(mutex);
}

void br_cond_signal(br_cond *cond) {
  BR_UNUSED(cond);
}

void br_cond_broadcast(br_cond *cond) {
  BR_UNUSED(cond);
}

#else
typedef u8 br__sync_other_translation_unit;
#endif
