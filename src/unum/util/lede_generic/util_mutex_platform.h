// (c) 2017-2018 minim.co
// unum helper utils, mutex wrappers


#ifndef _UTIL_MUTEX_PLATFORM_H
#define _UTIL_MUTEX_PLATFORM_H


// Type for internal mutex, the mess is needed since some LEDE
// platforms do not provide static initializer for recursive mutex
// (fortunately still provide the support recursive mutexes)
typedef struct _UTIL_MUTEX {
    pthread_mutex_t m; // normal pthreads mutex
    pthread_mutex_t g; // internal mutex for protecting init
    int init; // init flag (needed to init on the first use)
} UTIL_MUTEX_t;

#if defined(PTHREAD_MUTEX_RECURSIVE_NP)
#  define UTIL_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#elif defined(PTHREAD_MUTEX_RECURSIVE)
#  define UTIL_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE
#else
#error The platform does not support PTHREAD_MUTEX_RECURSIVE...!
#endif // PTHREAD_MUTEX_RECURSIVE or PTHREAD_MUTEX_RECURSIVE_NP

// Init mutex, dynamic and static init, mutex has to be recursive
#define UTIL_MUTEX_INIT(_m)  \
    { pthread_mutexattr_t _attr; pthread_mutexattr_init(&_attr); \
      pthread_mutexattr_settype(&_attr, UTIL_MUTEX_RECURSIVE);   \
      (_m)->init = TRUE; pthread_mutex_init(&((_m)->g), NULL);   \
      pthread_mutex_init((void *)(_m), &_attr); }

// Mutex initializer (set up the helper mutex and init only)
#define UTIL_MUTEX_INITIALIZER \
    { .g = PTHREAD_MUTEX_INITIALIZER, .init = FALSE }

// Free dynamically initialized mutex
#define UTIL_MUTEX_DEINIT(_m) \
    { pthread_mutex_destroy(&((_m)->g));   \
      pthread_mutex_destroy((void *)(_m)); }


// Inline for taking mutex
static __inline__ int util_mutex_take(UTIL_MUTEX_t *m) {
    pthread_mutex_lock(&m->g);
    if(!m->init) {
        pthread_mutexattr_t _attr;
        pthread_mutexattr_init(&_attr);
        pthread_mutexattr_settype(&_attr, UTIL_MUTEX_RECURSIVE);
        pthread_mutex_init(&m->m, &_attr);
        m->init = TRUE;
    }
    pthread_mutex_unlock(&m->g);
    return pthread_mutex_lock(&m->m);
}

// Inline for giving mutex
static __inline__ int util_mutex_give(UTIL_MUTEX_t *m) {
    // Just error if not initialized (can only happen if it has never been
    // taken since we do not allow to create in taken state)
    if(!m->init) {
        return EDEADLK;
    }
    return pthread_mutex_unlock(&m->m);
}

// Take mutex
#define UTIL_MUTEX_TAKE(_m) util_mutex_take(_m)
// Release mutex
#define UTIL_MUTEX_GIVE(_m) util_mutex_give(_m)


#endif // _UTIL_MUTEX_PLATFORM_H
