#include <bedrock/sync/primitives.h>

#include <string.h>

uptr br_current_thread_id(void) {
#if defined(_WIN32)
  return (uptr)GetCurrentThreadId();
#elif defined(__unix__) || defined(__APPLE__)
  pthread_t self;
  uptr id = 0;

  self = pthread_self();
  memcpy(&id, &self, br_min_size(sizeof(id), sizeof(self)));
  return id;
#else
  return 0u;
#endif
}
