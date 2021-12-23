#ifndef TABBED_UTILS_H
#define TABBED_UTILS_H

#include <time.h>

static inline int64_t get_posix_time() {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
    fprintf(stderr, "[error-tabbed] get_posix_time : can't get time, "
                    "clock_gettime(CLOCK_MONOTONIC) == -1\n");
    return -1;
  } else {
    return (long)ts.tv_sec * 1000L + (ts.tv_nsec / 1000000);
  }
}

static inline void sleep_seconds(int seconds) {
  struct timespec t = {.tv_sec = seconds, .tv_nsec = 0};
  nanosleep(&t, NULL);
}


static inline void sleep_ms(int ms) {
  struct timespec t = {
    .tv_sec = ms / 1000,
    .tv_nsec = (ms % 1000) * 1000000L
  };
  nanosleep(&t, NULL);
}

#endif // TABBED_UTILS_H
