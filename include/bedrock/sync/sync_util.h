#ifndef BEDROCK_SYNC_SYNC_UTIL_H
#define BEDROCK_SYNC_SYNC_UTIL_H

#include <bedrock/sync/extended.h>

#define br_lock(lock_ptr)                                                                          \
  _Generic((lock_ptr),                                                                             \
    br_mutex *: br_mutex_lock,                                                                     \
    br_rw_mutex *: br_rw_mutex_lock,                                                               \
    br_recursive_mutex *: br_recursive_mutex_lock,                                                 \
    br_ticket_mutex *: br_ticket_mutex_lock)(lock_ptr)

#define br_unlock(lock_ptr)                                                                        \
  _Generic((lock_ptr),                                                                             \
    br_mutex *: br_mutex_unlock,                                                                   \
    br_rw_mutex *: br_rw_mutex_unlock,                                                             \
    br_recursive_mutex *: br_recursive_mutex_unlock,                                               \
    br_ticket_mutex *: br_ticket_mutex_unlock)(lock_ptr)

#define br_try_lock(lock_ptr)                                                                      \
  _Generic((lock_ptr),                                                                             \
    br_mutex *: br_mutex_try_lock,                                                                 \
    br_rw_mutex *: br_rw_mutex_try_lock,                                                           \
    br_recursive_mutex *: br_recursive_mutex_try_lock)(lock_ptr)

#define br_shared_lock(lock_ptr) br_rw_mutex_shared_lock(lock_ptr)
#define br_shared_unlock(lock_ptr) br_rw_mutex_shared_unlock(lock_ptr)
#define br_try_shared_lock(lock_ptr) br_rw_mutex_try_shared_lock(lock_ptr)

/*
Odin's `guard` helpers rely on `defer`. Bedrock keeps the intent but exposes
them as scoped block macros because C has no equivalent built-in construct.
*/
#define br_guard(lock_ptr)                                                                         \
  for (bool BR_CONCAT(_br_guard_once_, __LINE__) = (br_lock((lock_ptr)), true);                    \
       BR_CONCAT(_br_guard_once_, __LINE__);                                                       \
       br_unlock((lock_ptr)), BR_CONCAT(_br_guard_once_, __LINE__) = false)

#define br_shared_guard(lock_ptr)                                                                  \
  for (bool BR_CONCAT(_br_shared_guard_once_, __LINE__) = (br_shared_lock((lock_ptr)), true);      \
       BR_CONCAT(_br_shared_guard_once_, __LINE__);                                                \
       br_shared_unlock((lock_ptr)), BR_CONCAT(_br_shared_guard_once_, __LINE__) = false)

#endif
