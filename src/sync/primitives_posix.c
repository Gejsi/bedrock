#if !defined(_WIN32)

#include <bedrock/sync/primitives.h>

#include <errno.h>
#include <pthread.h>

static br_status br__sync_status_from_code(int code) {
  if (code == 0) {
    return BR_STATUS_OK;
  }
  if (code == EAGAIN || code == ENOMEM) {
    return BR_STATUS_OUT_OF_MEMORY;
  }
  return BR_STATUS_INVALID_STATE;
}

br_status br_mutex_init(br_mutex *mutex) {
  if (mutex == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  return br__sync_status_from_code(pthread_mutex_init(&mutex->impl, NULL));
}

void br_mutex_destroy(br_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  BR_UNUSED(pthread_mutex_destroy(&mutex->impl));
}

void br_mutex_lock(br_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  BR_UNUSED(pthread_mutex_lock(&mutex->impl));
}

void br_mutex_unlock(br_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  BR_UNUSED(pthread_mutex_unlock(&mutex->impl));
}

bool br_mutex_try_lock(br_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return pthread_mutex_trylock(&mutex->impl) == 0;
}

br_status br_rw_mutex_init(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  return br__sync_status_from_code(pthread_rwlock_init(&mutex->impl, NULL));
}

void br_rw_mutex_destroy(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  BR_UNUSED(pthread_rwlock_destroy(&mutex->impl));
}

void br_rw_mutex_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  BR_UNUSED(pthread_rwlock_wrlock(&mutex->impl));
}

void br_rw_mutex_unlock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  BR_UNUSED(pthread_rwlock_unlock(&mutex->impl));
}

bool br_rw_mutex_try_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return pthread_rwlock_trywrlock(&mutex->impl) == 0;
}

void br_rw_mutex_shared_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  BR_UNUSED(pthread_rwlock_rdlock(&mutex->impl));
}

void br_rw_mutex_shared_unlock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  BR_UNUSED(pthread_rwlock_unlock(&mutex->impl));
}

bool br_rw_mutex_try_shared_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return pthread_rwlock_tryrdlock(&mutex->impl) == 0;
}

br_status br_recursive_mutex_init(br_recursive_mutex *mutex) {
  pthread_mutexattr_t attr;
  int code;

  if (mutex == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }

  code = pthread_mutexattr_init(&attr);
  if (code != 0) {
    return br__sync_status_from_code(code);
  }

  code = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  if (code == 0) {
    code = pthread_mutex_init(&mutex->impl, &attr);
  }
  BR_UNUSED(pthread_mutexattr_destroy(&attr));
  return br__sync_status_from_code(code);
}

void br_recursive_mutex_destroy(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  BR_UNUSED(pthread_mutex_destroy(&mutex->impl));
}

void br_recursive_mutex_lock(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  BR_UNUSED(pthread_mutex_lock(&mutex->impl));
}

void br_recursive_mutex_unlock(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  BR_UNUSED(pthread_mutex_unlock(&mutex->impl));
}

bool br_recursive_mutex_try_lock(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return pthread_mutex_trylock(&mutex->impl) == 0;
}

br_status br_cond_init(br_cond *cond) {
  if (cond == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  return br__sync_status_from_code(pthread_cond_init(&cond->impl, NULL));
}

void br_cond_destroy(br_cond *cond) {
  if (cond == NULL) {
    return;
  }
  BR_UNUSED(pthread_cond_destroy(&cond->impl));
}

void br_cond_wait(br_cond *cond, br_mutex *mutex) {
  if (cond == NULL || mutex == NULL) {
    return;
  }
  BR_UNUSED(pthread_cond_wait(&cond->impl, &mutex->impl));
}

void br_cond_signal(br_cond *cond) {
  if (cond == NULL) {
    return;
  }
  BR_UNUSED(pthread_cond_signal(&cond->impl));
}

void br_cond_broadcast(br_cond *cond) {
  if (cond == NULL) {
    return;
  }
  BR_UNUSED(pthread_cond_broadcast(&cond->impl));
}

#endif
