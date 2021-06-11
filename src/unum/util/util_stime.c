// (c) 2019-2020 minim.co

#include "unum.h"
#include "util_stime.h"

#if (__GLIBC__ > 2) || ((__GLIBC__ == 2) && __GLIBC_MINOR__ >= 31)
int stime(const time_t *time) {
    struct timespec ts = {.tv_sec = *time, .tv_nsec = 0};
    return clock_settime(CLOCK_REALTIME, &ts);
}
#endif
