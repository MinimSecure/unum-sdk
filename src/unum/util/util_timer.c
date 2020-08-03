// (c) 2017-2018 minim.co
// unum timers subsystem code

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Timers queue list head
static UTIL_QI_t head = { &head, &head };

// Timers array
static TIMER_CFG_t timer_a[UTIL_MAX_TIMERS];

// Mutex for protecting timers array, counter and linked list
static UTIL_MUTEX_t timer_m = UTIL_MUTEX_INITIALIZER;

// Event triggering rescan of the timers after changes
static UTIL_EVENT_t timer_c = UTIL_EVENT_INITIALIZER;


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
                              TIMER_FUNC_t f, THRD_PARAM_t *p, int thread)
{
    TIMER_HANDLE_t th = 0;

    if(msecs < 0 || !f || !name) {
        log("%s: invalid parameters\n", __func__);
        return th;
    }

    UTIL_MUTEX_TAKE(&timer_m);
    for(;;)
    {
        // Pick a cell (start at random spot)
        int idx, ii;
        idx = rand() % UTIL_MAX_TIMERS;
        for(ii = 0; ii < UTIL_MAX_TIMERS; ii++) {
            if(!timer_a[idx].f) {
                break;
            }
            ++idx;
            idx %= UTIL_MAX_TIMERS;
        }
        if(timer_a[idx].f) {
            log("%s: error, no free cell for timer <%s>\n",
                __func__, name);
            break;
        }

        // Set up the timer structure
        timer_a[idx].f = f;
        timer_a[idx].msecs = util_time(1000) + msecs;
        if(p) {
            memcpy(&timer_a[idx].param, p, sizeof(TIMER_PARAM_t));
        } else {
            memset(&timer_a[idx].param, 0, sizeof(TIMER_PARAM_t));
        }
        timer_a[idx].name = name;
        timer_a[idx].new_thread = thread;
        if(++(timer_a[idx].id) == 0) {
            ++(timer_a[idx].id); // ID must not be 0
        }

        // Make the timer handle
        th = (idx << 16) | timer_a[idx].id;

        // Add the item to the end of the linked list
        UTIL_Q_ADD(&head, &(timer_a[idx]));

        // Notify the timer thread that the list has changed
        UTIL_EVENT_SETALL(&timer_c);
        break;
    }
    UTIL_MUTEX_GIVE(&timer_m);

    return th;
}

// Cancels not yet fired timer
// Note: always check for the return value as you might try to
//       cancel a just fired timer and fail
// th - timer handle of the timer to cancel
// Returns: 0 - success or an error code
int util_timer_cancel(TIMER_HANDLE_t th)
{
    unsigned int idx, id;
    int ret = -1;

    idx = th >> 16;
    id = th & 0xffff;

    if(idx >= UTIL_MAX_TIMERS) {
        log("%s: inavlid timer handle 0x%x\n", __func__, th);
        return ret;
    }

    UTIL_MUTEX_TAKE(&timer_m);
    for(;;)
    {
        // Make sure the timer is allocated and the ID matches
        if(timer_a[idx].id != id) {
            log("%s: timer's 0x%x ID %d does not match %d\n",
                __func__, th, timer_a[idx].id, id);
            break;
        }
        if(!timer_a[idx].f) {
            log("%s: timer 0x%x is no longer active\n", __func__, th);
            break;
        }

        // Remove the timer from the linked list and mark the entry free
        UTIL_Q_DEL_ITEM(&(timer_a[idx]));
        timer_a[idx].f = NULL;

        // Notify the timer thread that the list has changed
        UTIL_EVENT_SETALL(&timer_c);

        ret = 0;
        break;
    }
    UTIL_MUTEX_GIVE(&timer_m);

    return ret;
}

// Timers thread function, all timer functions run in its context
// unless set up with the option to run them in their own thread
static void timers(THRD_PARAM_t *p)
{
    unsigned long delay;
    unsigned long long cur_t;
    TIMER_CFG_t *ii_item, *run_item;
    TIMER_CFG_t item;

    log("%s: started\n", __func__);

    for(;;)
    {
        delay = 0;
        cur_t = util_time(1000);

        UTIL_MUTEX_TAKE(&timer_m);
        // Walk through the timer queue and pick item we need to run or
        // if nothing found the delay to the first upcoming run.
        run_item = NULL;
        UTIL_Q_FOREACH(&head, ii_item) {
            // Is it time to run the handler?
            if(ii_item->msecs <= cur_t) {
                run_item = ii_item;
                break;
            }
            unsigned long remains = ii_item->msecs - cur_t;
            if(delay == 0 || remains < delay) {
                delay = remains;
            }
        }
        // If run item is found make a copy then remove and mark free
        if(run_item) {
            UTIL_Q_DEL_ITEM(run_item);
            memcpy(&item, run_item, sizeof(item));
            run_item->f = NULL;
        }
        // Clear the event
        UTIL_EVENT_RESET(&timer_c);
        UTIL_MUTEX_GIVE(&timer_m);

        // If nothing to run wait for time or an event and when done
        // waiting go back to the start of the loop to check what to do.
        if(!run_item) {
            UTIL_EVENT_TIMEDWAIT(&timer_c, delay);
            continue;
        }

        // Run the handler and go back to the loop start
        util_wd_set_timeout(TIMERS_INTHREAD_EXE_TIMEOUT);
        if(item.new_thread) {
            if(util_start_thrd(item.name, item.f, &item.param, NULL) != 0)
            {
                log("%s: failed to start thread for <%s>\n",
                    __func__, item.name);
            }
        } else {
            log("%s: launching timer <%s> handler\n", __func__, item.name);
            item.f(&item.param);
        }
        util_wd_set_timeout(0);
    }

    log("%s: done\n", __func__);
}

// Timers subsystem init function
// Returns: 0 - success or an error code
int util_timers_init(void)
{
    // Start the timers thread
    return util_start_thrd("timers", timers, NULL, NULL);
}


#ifdef DEBUG
// Timer test handler 1
static void timer_test1(TIMER_PARAM_t *p)
{
    time_t tt;

    time(&tt);
    printf("%s: called at %.24s, from %ld, param int_val: %d\n",
           __func__, ctime(&tt), syscall(SYS_gettid), p->int_val);
    if(p->int_val != 0) {
        int ret = util_timer_cancel((TIMER_HANDLE_t)p->int_val);
        printf("%s: Cancelling timer2 w/ handle 0x%x, retval: %d\n",
               __func__, p->int_val, ret);
    }
}

// Timer test handler 2
static void timer_test2(TIMER_PARAM_t *p)
{
    time_t tt;

    time(&tt);
    printf("%s: called at %.24s, from %ld, param int_val: %d\n",
           __func__, ctime(&tt), syscall(SYS_gettid), p->int_val);
    if(p->int_val == 4321) {
        printf("%s: Sleeping for 5sec...\n", __func__);
        sleep(5);
    }
}

// Test time and timers functionality
void test_timers(void)
{
    int i, err, ecode;
    TIMER_PARAM_t tp;

    // Init timers subsystem
    if(util_timers_init() != 0) {
        printf("%s: Error, util_timers_init() has failed\n",
               __func__);
        return;
    }

    printf("%s: Starting\n", __func__);

    printf("%s: Clock functions:\n", __func__);
    printf("%s:   util_time(1) => %llu\n", __func__, util_time(1));
    for(i = 0; i < 16; i++) {
        struct timespec t;
        char clock_name[128];
        // Clock IDs up to Linux 3.14.x
        switch(i) {
            case CLOCK_REALTIME:
                snprintf(clock_name, sizeof(clock_name), "CLOCK_REALTIME");
                break;
            case CLOCK_MONOTONIC:
                snprintf(clock_name, sizeof(clock_name), "CLOCK_MONOTONIC");
                break;
            case CLOCK_PROCESS_CPUTIME_ID:
                snprintf(clock_name, sizeof(clock_name), "CLOCK_PROCESS_CPUTIME_ID");
                break;
            case CLOCK_THREAD_CPUTIME_ID:
                snprintf(clock_name, sizeof(clock_name), "CLOCK_THREAD_CPUTIME_ID");
                break;
            case 4: // CLOCK_MONOTONIC_RAW
                snprintf(clock_name, sizeof(clock_name), "CLOCK_MONOTONIC_RAW");
                break;
            case 5: // CLOCK_REALTIME_COARSE
                snprintf(clock_name, sizeof(clock_name), "CLOCK_REALTIME_COARSE");
                break;
            case 6: // CLOCK_MONOTONIC_COARSE
                snprintf(clock_name, sizeof(clock_name), "CLOCK_MONOTONIC_COARSE");
                break;
            case 7: // CLOCK_BOOTTIME
                snprintf(clock_name, sizeof(clock_name), "CLOCK_BOOTTIME");
                break;
            case 8: // CLOCK_REALTIME_ALARM
                snprintf(clock_name, sizeof(clock_name), "CLOCK_REALTIME_ALARM");
                break;
            case 9: // CLOCK_BOOTTIME_ALARM
                snprintf(clock_name, sizeof(clock_name), "CLOCK_BOOTTIME_ALARM");
                break;
            case 10: // CLOCK_SGI_CYCLE
                snprintf(clock_name, sizeof(clock_name), "CLOCK_SGI_CYCLE");
                break;
            case 11: // CLOCK_TAI
                snprintf(clock_name, sizeof(clock_name), "CLOCK_TAI");
                break;
            default:
                snprintf(clock_name, sizeof(clock_name), "%d", i);
        }
        err = clock_gettime(i, &t);
        ecode = errno;
        printf("%s:   clock_gettime(%.25s, ...) => %s",
               __func__, clock_name, ((err == 0) ? "Ok" : "Error"));
        if(err == 0) {
            printf(" %lu:%lu\n", t.tv_sec, t.tv_nsec);
        } else {
            printf(" %s\n", strerror(ecode));
        }
    }


    printf("\n");
    printf("%s: util_system() timing out test:\n", __func__);
    unsigned long long cmd_start = util_time(1);
    char pid_file[] = "/tmp/unum_test.pid";
    char cmd[] = "sleep 5; echo -n PID:; cat /tmp/unum_test.pid; echo; sleep 10";
    printf("%s:   executing util_system(\"%s\", 10, \"%s\")\n",
           __func__, cmd, pid_file);
    err = util_system(cmd, 10, pid_file);
    printf("%s:   completed in %llusec with exit code %d (expected 9)\n",
           __func__, util_time(1) - cmd_start, err);


    printf("\n");
    printf("%s: Timers test:\n", __func__);
    printf("%s: Setup timer1 in 10, timer2 in 15sec, no new thread\n",
           __func__);
    util_timer_set(10000, "timer1", timer_test1, NULL, FALSE);
    tp.int_val = 1234;
    util_timer_set(15000, "timer2", timer_test2, &tp, FALSE);
    printf("%s: Sleeping 20sec...\n", __func__);
    sleep(20);

    printf("%s: Setup timer1 in 11, timer2 in 10sec, 2 in new thread\n",
           __func__);
    util_timer_set(11000, "timer1", timer_test1, NULL, FALSE);
    tp.int_val = 4321;
    util_timer_set(10000, "timer2", timer_test2, &tp, TRUE);
    printf("%s: Sleeping 20sec...\n", __func__);
    sleep(20);

    printf("%s: Setup timer1 in 10, timer2 in 15sec, no new thread\n",
           __func__);
    tp.int_val = (int)util_timer_set(15000, "timer2", timer_test2, NULL, TRUE);
    util_timer_set(10000, "timer1", timer_test1, &tp, FALSE);
    printf("%s: Sleeping 20sec...\n", __func__);
    sleep(20);

    printf("%s: Done\n", __func__);
}
#endif // DEBUG
