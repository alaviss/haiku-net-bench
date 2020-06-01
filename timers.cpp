#ifdef __HAIKU__
# include <OS.h>
#else
# include <stdlib.h>
# include <time.h>
#endif

#include "common.h"
#include "timers.h"

int64_t getMonoTime() {
#ifdef __HAIKU__
  return (int64_t)system_time_nsecs();
#else
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
    abort();

  return (int64_t)ts.tv_sec * (int64_t)SEC + ts.tv_nsec;
#endif
}
