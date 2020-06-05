// (c) 2017-2018 minim.co
// unum helper utils code

#include "unum.h"


// Init the event
void util_event_init(UTIL_EVENT_t *e)
{
    pthread_mutex_init(&e->m, 0);
    pthread_cond_init(&e->c, 0);
    e->is_set = FALSE;
}

// Signal an event (unblocks one or all threads waiting,
// the new calls to wait will return immediately till
// the event is reset)
void util_event_set(UTIL_EVENT_t *e, int all)
{
    pthread_mutex_lock(&e->m);
    e->is_set = TRUE;
    if(all) {
        pthread_cond_broadcast(&e->c);
    } else {
        pthread_cond_signal(&e->c);
    }
    pthread_mutex_unlock(&e->m);
}

// Resets the already signalled event, after the call threads can wait
// on this event till it is signalled again
void util_event_reset(UTIL_EVENT_t *e)
{
    pthread_mutex_lock(&e->m);
    e->is_set = FALSE;
    pthread_mutex_unlock(&e->m);
}

// Wait till the event is signalled or millisec timeout expires (0 - no timeout)
// Returns 0 when the waited event is set, 1 - if timed out, -1 - if error
int util_event_wait(UTIL_EVENT_t *e, unsigned long msec)
{
    int rc, ret = 0;
    pthread_mutex_lock(&e->m);
    while (!e->is_set) {
        if(!msec) {
            rc = pthread_cond_wait(&e->c, &e->m);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += msec / 1000;
            ts.tv_nsec += (msec % 1000) * 1000000;
            ts.tv_sec += ts.tv_nsec / 1000000000;
            ts.tv_nsec = ts.tv_nsec % 1000000000;
            rc = pthread_cond_timedwait(&e->c, &e->m, &ts);
        }
        if(rc == ETIMEDOUT) {
            ret = 1;
            break;
        } else if(rc != 0) {
            ret = -1;
            break;
        }
    }
    pthread_mutex_unlock(&e->m);
    return ret;
}

// Return immediately TRUE (if event is signalled) or FALSE (if not)
int util_event_is_set(UTIL_EVENT_t *e)
{
    return e->is_set;
}
