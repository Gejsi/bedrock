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
Bedrock intentionally does not provide a scoped guard macro. A for-loop macro
cannot release a lock when control leaves its body through break, return, or
goto. Use br_lock/br_unlock (or the typed functions) explicitly and release the
lock on every control-flow path.
*/

#endif
