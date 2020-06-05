// (c) 2019 minim.co
// Platform specific include file.
// The main purpose is to pull in all the platform specific headers.

#ifndef _PALTFORM_H
#define _PALTFORM_H
#include <libnvram.h>
#include <libshared.h>

#ifdef DNS_USE_ATOMIC_ADD
// Create _pvoid type which is a void * that allows aliasing
typedef void * __attribute__((__may_alias__)) _pvoid;

// From GNU C Library Circa 2002 - atomicity.h
static inline int __attribute__((__used__))
    __atomic_add (volatile _pvoid *mem, int val)
{
  int result;

  __asm__ __volatile__
    ("/* Inline atomic add */\n"
     "1:\n\t"
     ".set      push\n\t"
     ".set      mips2\n\t"
     "ll        %0,%2\n\t"
     "addu      %0,%3,%0\n\t"
     "sc        %0,%1\n\t"
     ".set      pop\n\t"
     "beqz      %0,1b\n\t"
     "/* End atomic add */"
     : "=&r"(result), "=m"(*mem)
     : "m" (*mem), "r"(val)
     : "memory");

  return result;
}

#define DNS_ATOMIC_FETCH_SUB(i) __atomic_add((void*)(i), -1)
#define DNS_ATOMIC_FETCH_ADD(i) __atomic_add((void*)(i), 1)

#else // DNS_USE_ATOMIC_ADD

#define DNS_ATOMIC_FETCH_SUB(i) __sync_fetch_and_sub(i, 1)
#define DNS_ATOMIC_FETCH_ADD(i) __sync_fetch_and_add(i, 1)

#endif // DNS_USE_ATOMIC_ADD

#endif // _PALTFORM_H

