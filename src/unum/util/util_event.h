// (c) 2017-2018 minim.co
// unum helper utils, event wrappers

#ifndef _UTIL_EVENT_H
#define _UTIL_EVENT_H


// Struct/type for events
typedef struct {
    pthread_mutex_t m;
    pthread_cond_t c;
    int is_set;
} UTIL_EVENT_t;


// Init the event
void util_event_init(UTIL_EVENT_t *e);
// Signal an event (unblocks all who is waiting, all
// new calls to wait will return immediately till
// the event is reset)
void util_event_set(UTIL_EVENT_t *e, int all);
// Resets the already signalled event, after the call threads can wait
// on this event till it is signalled again
void util_event_reset(UTIL_EVENT_t *e);
// Wait till the even is signalled or millisec timeout expires (0 - no timeout)
// Returns 0 when the waited event is set, 1 - if timed out, -1 - if error
int util_event_wait(UTIL_EVENT_t *e, unsigned long msec);
// Return immediately TRUE (if event is signalled) or FALSE (if not)
int util_event_is_set(UTIL_EVENT_t *e);


// Event macros wrapping the functions
#define UTIL_EVENT_INIT(_e)   util_event_init(_e)
#define UTIL_EVENT_SET(_e)    util_event_set((_e), 0)
#define UTIL_EVENT_SETALL(_e) util_event_set((_e), 1)
#define UTIL_EVENT_RESET(_e)  util_event_reset(_e)
#define UTIL_EVENT_WAIT(_e)   util_event_wait((_e), 0)
#define UTIL_EVENT_IS_SET(_e) util_event_is_set(_e)
#define UTIL_EVENT_TIMEDWAIT(_e, _t) util_event_wait((_e), (_t))
#define UTIL_EVENT_INITIALIZER {PTHREAD_MUTEX_INITIALIZER,PTHREAD_COND_INITIALIZER,FALSE}

#endif // _UTIL_EVENT_H
