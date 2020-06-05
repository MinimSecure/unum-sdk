// (c) 2017-2018 minim.co
// jobs handling code

#include "unum.h"


// Array of agent threads
static UTIL_THRD_t threads[MAX_THRD_COUNT];

// Pointer to the main thread entry in the threads[] array
static UTIL_THRD_t *main_thrd_ptr = NULL;

// Mutex for protecting threads[]
static UTIL_MUTEX_t thrd_m = UTIL_MUTEX_INITIALIZER;

// Our thread data key
static pthread_key_t thrd_key;

// Initialize our thread key (call at init)
void util_init_thrd_key(void)
{
    pthread_key_create(&thrd_key, NULL);
}

// Thread startup wrapper
static void *thread_start_wrapper(void *arg)
{
    UTIL_THRD_t *thrd_ptr = (UTIL_THRD_t *)arg;

    log("%s: starting thread %s\n", __func__, thrd_ptr->name);
    pthread_setspecific(thrd_key, thrd_ptr);

    UTIL_MUTEX_TAKE(&thrd_m);
    thrd_ptr->tid = syscall(SYS_gettid);
    thrd_ptr->flags |= THRD_FLAG_STARTED;
    UTIL_MUTEX_GIVE(&thrd_m);

    thrd_ptr->func(&(thrd_ptr->param));

    UTIL_MUTEX_TAKE(&thrd_m);
    thrd_ptr->flags &= ~(THRD_FLAG_BUSY | THRD_FLAG_STARTED);
    UTIL_MUTEX_GIVE(&thrd_m);

    pthread_setspecific(thrd_key, NULL);
    log("%s: thread %s terminated\n", __func__, thrd_ptr->name);

    return NULL;
}

// Start a thread, returns 0 if successful or negative number if fails
// Params:
// name - thread name
// f - thread function
// p - thread function parameters structure (can be NULL)
// ctl - thread startup control structure (can be NULL to start w/ defaults)
int util_start_thrd(const char *name, THRD_FUNC_t f,
                    THRD_PARAM_t *p, THRD_CTL_t *ctl)
{
    int ii, ret = 0;
    UTIL_THRD_t *thrd_ptr;
    pthread_attr_t attr;
    pthread_attr_t *p_attr = NULL;

    ret = pthread_attr_init(&attr);
    if(ret != 0) {
        log("%s: error %d in pthread_attr_init() for <%s>, ignoring\n",
            __func__, ret, name);
        // Try to continue anyway
    } else {
        // The platform code might change the thread stack size limit
#ifdef JOB_MAX_STACK_SIZE
        pthread_attr_setstacksize(&attr, (size_t)JOB_MAX_STACK_SIZE);
#endif // JOB_MAX_STACK_SIZE

        // The thread must be either joined or detached to free its resources
        // unless this attribute is set (then it is created detached)
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        p_attr = &attr;
    }

    UTIL_MUTEX_TAKE(&thrd_m);
    for(;;)
    {
        // Find available slot (0 - main thread slot)
        thrd_ptr = threads;
        if((thrd_ptr->flags & THRD_FLAG_BUSY) == 0) {
            log("%s: error <%s>, subsystem is not yet initialized\n",
                __func__, name);
            ret = -1;
            break;
        }
        for(ii = 1; ii < MAX_THRD_COUNT; ii++) {
            if((threads[ii].flags & THRD_FLAG_BUSY) == 0) {
                thrd_ptr = &(threads[ii]);
                break;
            }
        }
        if(thrd_ptr == threads) {
            log("%s: error <%s>, no slot available\n", __func__, name);
            ret = -2;
            break;
        }
        // Fill in thread info
        memset(thrd_ptr, 0, sizeof(*thrd_ptr));
        snprintf(thrd_ptr->name, MAX_THRD_NAME_LEN, "%s", name);
        thrd_ptr->func = f;
        thrd_ptr->flags |= THRD_FLAG_BUSY;
        if(p) {
            thrd_ptr->param = *p;
        }
        if(ctl) {
            thrd_ptr->wd_timeout = ctl->wd_timeout;
        }
        thrd_ptr->wd_uptime = util_time(1);
        // Start the thread
        ret = pthread_create(&(thrd_ptr->thread), p_attr,
                             thread_start_wrapper, thrd_ptr);
        if(ret != 0) {
            log("%s: error %d in pthread_create() for <%s>\n",
                __func__, ret, name);
            break;
        }
        break;
    }
    UTIL_MUTEX_GIVE(&thrd_m);

    if(p_attr != NULL) {
        pthread_attr_destroy(p_attr);
    }

    return ret;
}

// Returns TRUE if run in the main (or only) thread, FALSE otherwise
int util_is_main_thread(void)
{
    // If TID matches or we have not initialized threads
    if(!main_thrd_ptr || main_thrd_ptr->tid == syscall(SYS_gettid)) {
        return TRUE;
    }
    return FALSE;
}

// Set main thread info (should be called from the main
// thread itself during init). Returns 0 if successful;
int util_set_main_thrd()
{
    int ret = 0;
    UTIL_THRD_t *thrd_ptr;

    UTIL_MUTEX_TAKE(&thrd_m);
    for(;;)
    {
        // The thread ptr should not yet be set and all flags should be cleared
        thrd_ptr = (UTIL_THRD_t *)pthread_getspecific(thrd_key);
        if(thrd_ptr != NULL) {
            log("%s: error, pointer for key %p is not NULL (%p)\n",
                __func__, thrd_key, thrd_ptr);
            ret = -1;
            break;
        }
        thrd_ptr = threads;
        if((thrd_ptr->flags & THRD_FLAG_BUSY) != 0) {
            log("%s: error, thread slot 0 already taken\n", __func__);
            ret = -2;
            break;
        }
        pthread_setspecific(thrd_key, thrd_ptr);
        memset(thrd_ptr, 0, sizeof(*thrd_ptr));
        thrd_ptr->tid = syscall(SYS_gettid);
        thrd_ptr->flags |= THRD_FLAG_STARTED;
        thrd_ptr->flags |= THRD_FLAG_BUSY;
        snprintf(thrd_ptr->name, MAX_THRD_NAME_LEN, "main");
        main_thrd_ptr = thrd_ptr;
        break;
    }
    UTIL_MUTEX_GIVE(&thrd_m);

    return ret;
}

// Set the calling thread watchdog timeout (in seconds, 0 to disable)
// It is checked every 10sec (i.e. shorter timeouts are not detected)
// Returns 0 if successful.
int util_wd_set_timeout(unsigned int timeout)
{
    UTIL_THRD_t *thrd_ptr = (UTIL_THRD_t *)pthread_getspecific(thrd_key);
    if(thrd_ptr < threads || thrd_ptr >= &(threads[MAX_THRD_COUNT])) {
        log("%s: error, invalid thread info pointer %p\n",
            __func__, thrd_ptr);
        return -1;
    }
    log_dbg("%s: setting watchdog timeout to %d sec for %s\n",
            __func__, timeout, thrd_ptr->name);
    __sync_synchronize();
    thrd_ptr->wd_uptime = util_time(1);
    __sync_synchronize();
    thrd_ptr->wd_timeout = timeout;
    return 0;
}

// Update the calling thread wd, returns 0 if successful
int util_wd_poll(void)
{
    UTIL_THRD_t *thrd_ptr = (UTIL_THRD_t *)pthread_getspecific(thrd_key);
    if(thrd_ptr < threads || thrd_ptr >= &(threads[MAX_THRD_COUNT])) {
        log("%s: error, invalid thread info pointer %p\n",
            __func__, thrd_ptr);
        return -1;
    }
    __sync_synchronize();
    thrd_ptr->wd_uptime = util_time(1);
    return 0;
}

// Check the specified thread watchdog for expiration
// Returns 0 if the thread is ok, or the number of the seconds it is late for.
// Returns negative number if the thread is not started or not found.
static int util_wd_check(UTIL_THRD_t *thrd_ptr)
{
    int late, ret = 0;
    unsigned long wd_timeout;
    unsigned long wd_uptime;
    unsigned long cur_uptime;

    UTIL_MUTEX_TAKE(&thrd_m);
    for(;;)
    {
        if(thrd_ptr < threads || thrd_ptr >= &(threads[MAX_THRD_COUNT])) {
            log("%s: error, invalid thread info pointer %p\n",
                __func__, thrd_ptr);
            ret = -1;
            break;
        }
        // check if the thread is running
        if((thrd_ptr->flags & THRD_FLAG_STARTED) == 0) {
            ret = -2;
            break;
        }
        wd_timeout = thrd_ptr->wd_timeout;
        wd_uptime = thrd_ptr->wd_uptime;
        cur_uptime = util_time(1);
        if(wd_timeout == 0) {
            break; // wd is not enabled
        }
        late = (cur_uptime - wd_uptime) - wd_timeout;
        if(late > 0) {
            log("%s: thread %s (%d) is %d sec late polling wd, timeout %lu\n",
                __func__, thrd_ptr->name, thrd_ptr->tid, late, wd_timeout);
            ret = late;
            break;
        }

        break;
    }
    UTIL_MUTEX_GIVE(&thrd_m);

    return ret;
}

// Checks all the threads for watchdog timeout
// Restarts the agent if fails.
void util_wd_check_all(void)
{
    int ii;

    // Starting from 1 since 0 is the main thread running the wd check
    for(ii = 1; ii < MAX_THRD_COUNT; ii++) {
        if(util_wd_check(&(threads[ii])) > 0) {
            log("%s: restarting due to wd timeout\n", __func__);
            util_restart(UNUM_START_REASON_WD_TIMEOUT);
            // Should never reach this point
        }
    }
}
