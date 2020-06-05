// (c) 2017-2018 minim.co
// unum timers subsystem include file

#ifndef _UTIL_TIMER_H
#define _UTIL_TIMER_H


// MAX number of active timers
#define UTIL_MAX_TIMERS 64

// Watchdog timeout to make sure in-thread timer handlers do not
// hang the timer thread (the handlers should finish within a
// second or set up to run in their own thread, here it is just a
// catch for a grossly misbehaving code).
#define TIMERS_INTHREAD_EXE_TIMEOUT 20


// Timer function parameters structure (the same as thread,
// the timers might request to be run in their own thread)
typedef THRD_PARAM_t TIMER_PARAM_t;

// Timer handle (top 2 bytes timer index in the array,
// bottom the timer ID)
typedef unsigned long TIMER_HANDLE_t;

// Timer handler function type (the same as thread function type)
// Note: the timer functions are normally fast and are called in
//       the same thread. If any blocking or time consuming operation is
//       executed the timer should be set up to spawd its own thread
//       (see util_timer_set())
typedef THRD_FUNC_t TIMER_FUNC_t;

// Timer configuration item
typedef struct _TIMER_CFG {
    UTIL_Q_HEADER();          // declares fields used by queue macros
    unsigned long long msecs; // uptime in msec when to fire
    TIMER_FUNC_t f;           // timer handler function (NULL if unused)
    TIMER_PARAM_t param;      // param for the timer functon call
    const char *name;         // timer name (NULL if unused)
    unsigned short id;        // timer ID
    short new_thread;         // TRUE - run the handler in a new thread
} TIMER_CFG_t;

// Sets a timer to fire after specified number of milliseconds.
// The timer is automaticaly cancelled before the handler is called.
// If you want it to continue firing make the handler re-set it at the end.
// If the handler function is slow/might block use thread == TRUE.
// Do not do it 'just to be safe' as the # of threads we can run
// simultaneously is limited.
// msec - milliseconds till the timer should fire
// name - pointer to a constant string naming the timer
// f - function to call when the timer fires
// p - pointer to parameters structure to store and pass to the timer
//     function (by a pointer) when it is called (can be NULL)
// thread - TRUE if it has to run in its own thread
// Returns: timer handle if OK or 0 if fails
TIMER_HANDLE_t util_timer_set(unsigned int msecs, const char *name,
                              TIMER_FUNC_t f, THRD_PARAM_t *p, int thread);

// Cancels not yet fired timer
// Note: always check for the return value as you might try to
//       cancel a just fired timer and fail
// th - timer handle of the timer to cancel
// Returns: 0 - success or an error code
int util_timer_cancel(TIMER_HANDLE_t th);

// Timers subsystem init function
// Returns: 0 - success or an error code
int util_timers_init(void);

#ifdef DEBUG
// Subsystem test function
void test_timers(void);
#endif // DEBUG

#endif // _UTIL_TIMER_H
