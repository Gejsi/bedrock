#include <bedrock/sync/primitives.h>

br_status br_mutex_init(br_mutex *mutex) {
  if (mutex == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  br_atomic_mutex_init(&mutex->impl);
  return BR_STATUS_OK;
}

void br_mutex_destroy(br_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_mutex_lock(br_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_mutex_lock(&mutex->impl);
}

void br_mutex_unlock(br_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_mutex_unlock(&mutex->impl);
}

bool br_mutex_try_lock(br_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return br_atomic_mutex_try_lock(&mutex->impl);
}

br_status br_rw_mutex_init(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  br_atomic_rw_mutex_init(&mutex->impl);
  return BR_STATUS_OK;
}

void br_rw_mutex_destroy(br_rw_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_rw_mutex_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_rw_mutex_lock(&mutex->impl);
}

void br_rw_mutex_unlock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_rw_mutex_unlock(&mutex->impl);
}

bool br_rw_mutex_try_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return br_atomic_rw_mutex_try_lock(&mutex->impl);
}

void br_rw_mutex_shared_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_rw_mutex_shared_lock(&mutex->impl);
}

void br_rw_mutex_shared_unlock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_rw_mutex_shared_unlock(&mutex->impl);
}

bool br_rw_mutex_try_shared_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return br_atomic_rw_mutex_try_shared_lock(&mutex->impl);
}

br_status br_recursive_mutex_init(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  br_atomic_recursive_mutex_init(&mutex->impl);
  return BR_STATUS_OK;
}

void br_recursive_mutex_destroy(br_recursive_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_recursive_mutex_lock(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_recursive_mutex_lock(&mutex->impl);
}

void br_recursive_mutex_unlock(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  br_atomic_recursive_mutex_unlock(&mutex->impl);
}

bool br_recursive_mutex_try_lock(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return br_atomic_recursive_mutex_try_lock(&mutex->impl);
}

br_status br_cond_init(br_cond *cond) {
  if (cond == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  br_atomic_cond_init(&cond->impl);
  return BR_STATUS_OK;
}

void br_cond_destroy(br_cond *cond) {
  BR_UNUSED(cond);
}

void br_cond_wait(br_cond *cond, br_mutex *mutex) {
  if (cond == NULL || mutex == NULL) {
    return;
  }
  BR_UNUSED(br_atomic_cond_wait(&cond->impl, &mutex->impl));
}

bool br_cond_wait_with_timeout(br_cond *cond, br_mutex *mutex, br_duration duration) {
  if (cond == NULL || mutex == NULL || duration <= 0) {
    return false;
  }
  return br_atomic_cond_wait_with_timeout(&cond->impl, &mutex->impl, duration);
}

void br_cond_signal(br_cond *cond) {
  if (cond == NULL) {
    return;
  }
  br_atomic_cond_signal(&cond->impl);
}

void br_cond_broadcast(br_cond *cond) {
  if (cond == NULL) {
    return;
  }
  br_atomic_cond_broadcast(&cond->impl);
}

br_status br_sema_init(br_sema *sema, u32 count) {
  if (sema == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  br_atomic_sema_init(&sema->impl, count);
  return BR_STATUS_OK;
}

void br_sema_destroy(br_sema *sema) {
  BR_UNUSED(sema);
}

void br_sema_post(br_sema *sema, u32 count) {
  if (sema == NULL) {
    return;
  }
  br_atomic_sema_post(&sema->impl, count);
}

void br_sema_wait(br_sema *sema) {
  if (sema == NULL) {
    return;
  }
  br_atomic_sema_wait(&sema->impl);
}

bool br_sema_wait_with_timeout(br_sema *sema, br_duration duration) {
  if (sema == NULL) {
    return false;
  }
  return br_atomic_sema_wait_with_timeout(&sema->impl, duration);
}
