// (c) 2021 minim.co
// unum helper utils, stime wrappers
// default implementation, platform might require its own

#ifndef _UTIL_STIME_H
#define _UTIL_STIME_H

#define XSTR(x) STR(x)
#define STR(x) #x

#if (__GLIBC__ > 2) || ((__GLIBC__ == 2) && __GLIBC_MINOR__ >= 31)
int stime(const time_t *t);
#endif

#endif // _UTIL_STIME_H
