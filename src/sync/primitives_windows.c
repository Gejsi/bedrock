#if defined(_WIN32)

#include <bedrock/sync/primitives.h>

br_status br_mutex_init(br_mutex *mutex) {
  if (mutex == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  InitializeSRWLock(&mutex->impl);
  return BR_STATUS_OK;
}

void br_mutex_destroy(br_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_mutex_lock(br_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  AcquireSRWLockExclusive(&mutex->impl);
}

void br_mutex_unlock(br_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  ReleaseSRWLockExclusive(&mutex->impl);
}

bool br_mutex_try_lock(br_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return TryAcquireSRWLockExclusive(&mutex->impl) != 0;
}

br_status br_rw_mutex_init(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  InitializeSRWLock(&mutex->impl);
  return BR_STATUS_OK;
}

void br_rw_mutex_destroy(br_rw_mutex *mutex) {
  BR_UNUSED(mutex);
}

void br_rw_mutex_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  AcquireSRWLockExclusive(&mutex->impl);
}

void br_rw_mutex_unlock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  ReleaseSRWLockExclusive(&mutex->impl);
}

bool br_rw_mutex_try_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return TryAcquireSRWLockExclusive(&mutex->impl) != 0;
}

void br_rw_mutex_shared_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  AcquireSRWLockShared(&mutex->impl);
}

void br_rw_mutex_shared_unlock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  ReleaseSRWLockShared(&mutex->impl);
}

bool br_rw_mutex_try_shared_lock(br_rw_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return TryAcquireSRWLockShared(&mutex->impl) != 0;
}

br_status br_recursive_mutex_init(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  if (InitializeCriticalSectionEx(&mutex->impl, 4000u, 0u) == 0) {
    return BR_STATUS_OUT_OF_MEMORY;
  }
  return BR_STATUS_OK;
}

void br_recursive_mutex_destroy(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  DeleteCriticalSection(&mutex->impl);
}

void br_recursive_mutex_lock(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  EnterCriticalSection(&mutex->impl);
}

void br_recursive_mutex_unlock(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return;
  }
  LeaveCriticalSection(&mutex->impl);
}

bool br_recursive_mutex_try_lock(br_recursive_mutex *mutex) {
  if (mutex == NULL) {
    return false;
  }
  return TryEnterCriticalSection(&mutex->impl) != 0;
}

br_status br_cond_init(br_cond *cond) {
  if (cond == NULL) {
    return BR_STATUS_INVALID_ARGUMENT;
  }
  InitializeConditionVariable(&cond->impl);
  return BR_STATUS_OK;
}

void br_cond_destroy(br_cond *cond) {
  BR_UNUSED(cond);
}

void br_cond_wait(br_cond *cond, br_mutex *mutex) {
  if (cond == NULL || mutex == NULL) {
    return;
  }
  BR_UNUSED(SleepConditionVariableSRW(&cond->impl, &mutex->impl, INFINITE, 0u));
}

void br_cond_signal(br_cond *cond) {
  if (cond == NULL) {
    return;
  }
  WakeConditionVariable(&cond->impl);
}

void br_cond_broadcast(br_cond *cond) {
  if (cond == NULL) {
    return;
  }
  WakeAllConditionVariable(&cond->impl);
}

#else
typedef char br__sync_windows_translation_unit;
#endif
