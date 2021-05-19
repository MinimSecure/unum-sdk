// (c) 2017-2018 minim.co
// unum helper utils, mutex wrappers
// default implementation, platform might require its own

#ifndef _UTIL_MUTEX_H
#define _UTIL_MUTEX_H


// Type for using mutexes internally
typedef pthread_mutex_t UTIL_MUTEX_t;


// The most common case
#if defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)

// Init mutex, dynamic and static init, mutex has to be recursive
#define UTIL_MUTEX_INIT(_m)  \
     { pthread_mutexattr_t _attr; pthread_mutexattr_init(&_attr);     \
       pthread_mutexattr_settype(&_attr, PTHREAD_MUTEX_RECURSIVE_NP); \
       pthread_mutex_init(_m, &_attr); }
#define UTIL_MUTEX_INITIALIZER PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP

// Some platforms define the type and initilizer without '_NP' suffix
#elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER)

// Init mutex, dynamic and static init, mutex has to be recursive
#define UTIL_MUTEX_INIT(_m)  \
     { pthread_mutexattr_t _attr; pthread_mutexattr_init(&_attr);  \
       pthread_mutexattr_settype(&_attr, PTHREAD_MUTEX_RECURSIVE); \
       pthread_mutex_init(_m, &_attr); }
#define UTIL_MUTEX_INITIALIZER PTHREAD_RECURSIVE_MUTEX_INITIALIZER

#else  // Can't find recursive mutex static initializer
#  error No PTHREAD_RECURSIVE_MUTEX_INITIALIZER... defines!
#endif // Done with mutex initializers

// Free dynamically initialized mutex
#define UTIL_MUTEX_DEINIT(_m) (pthread_mutex_destroy((void *)(_m)))

// Take mutex
#define UTIL_MUTEX_TAKE(_m) pthread_mutex_lock(_m)
// Release mutex
#define UTIL_MUTEX_GIVE(_m) pthread_mutex_unlock(_m)

#endif // _UTIL_MUTEX_H
