#ifndef BEDROCK_SYNC_PRIMITIVES_H
#define BEDROCK_SYNC_PRIMITIVES_H

#include <bedrock/base.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#include <pthread.h>
#endif

BR_EXTERN_C_BEGIN

typedef u64 br_thread_id;

#define BR_THREAD_ID_INVALID ((br_thread_id)0)

/*
Bedrock keeps Odin's sync surface close, but the primitive locks use explicit
init/destroy in C instead of Odin's "zero value is ready" contract. That is an
intentional API difference: the native OS primitives Bedrock targets are more
reliable to use through explicit construction than through hidden lazy init.
*/

typedef struct br_mutex {
#if defined(_WIN32)
  SRWLOCK impl;
#elif defined(__unix__) || defined(__APPLE__)
  pthread_mutex_t impl;
#else
  i32 impl;
#endif
} br_mutex;

typedef struct br_rw_mutex {
#if defined(_WIN32)
  SRWLOCK impl;
#elif defined(__unix__) || defined(__APPLE__)
  pthread_rwlock_t impl;
#else
  i32 impl;
#endif
} br_rw_mutex;

typedef struct br_recursive_mutex {
#if defined(_WIN32)
  CRITICAL_SECTION impl;
#elif defined(__unix__) || defined(__APPLE__)
  pthread_mutex_t impl;
#else
  i32 impl;
#endif
} br_recursive_mutex;

typedef struct br_cond {
#if defined(_WIN32)
  CONDITION_VARIABLE impl;
#elif defined(__unix__) || defined(__APPLE__)
  pthread_cond_t impl;
#else
  i32 impl;
#endif
} br_cond;

#if defined(_WIN32)
#define BR_MUTEX_INIT {SRWLOCK_INIT}
#define BR_RW_MUTEX_INIT {SRWLOCK_INIT}
#define BR_COND_INIT {CONDITION_VARIABLE_INIT}
#elif defined(__unix__) || defined(__APPLE__)
#define BR_MUTEX_INIT {PTHREAD_MUTEX_INITIALIZER}
#define BR_RW_MUTEX_INIT {PTHREAD_RWLOCK_INITIALIZER}
#define BR_COND_INIT {PTHREAD_COND_INITIALIZER}
#else
#define BR_MUTEX_INIT {0}
#define BR_RW_MUTEX_INIT {0}
#define BR_COND_INIT {0}
#endif

br_thread_id br_current_thread_id(void);

br_status br_mutex_init(br_mutex *mutex);
void br_mutex_destroy(br_mutex *mutex);
void br_mutex_lock(br_mutex *mutex);
void br_mutex_unlock(br_mutex *mutex);
bool br_mutex_try_lock(br_mutex *mutex);

br_status br_rw_mutex_init(br_rw_mutex *mutex);
void br_rw_mutex_destroy(br_rw_mutex *mutex);
void br_rw_mutex_lock(br_rw_mutex *mutex);
void br_rw_mutex_unlock(br_rw_mutex *mutex);
bool br_rw_mutex_try_lock(br_rw_mutex *mutex);
void br_rw_mutex_shared_lock(br_rw_mutex *mutex);
void br_rw_mutex_shared_unlock(br_rw_mutex *mutex);
bool br_rw_mutex_try_shared_lock(br_rw_mutex *mutex);

br_status br_recursive_mutex_init(br_recursive_mutex *mutex);
void br_recursive_mutex_destroy(br_recursive_mutex *mutex);
void br_recursive_mutex_lock(br_recursive_mutex *mutex);
void br_recursive_mutex_unlock(br_recursive_mutex *mutex);
bool br_recursive_mutex_try_lock(br_recursive_mutex *mutex);

br_status br_cond_init(br_cond *cond);
void br_cond_destroy(br_cond *cond);
void br_cond_wait(br_cond *cond, br_mutex *mutex);
void br_cond_signal(br_cond *cond);
void br_cond_broadcast(br_cond *cond);

BR_EXTERN_C_END

#endif
