// (c) 2017-2018 minim.co
// unum helper utils include file

#ifndef _JOBS_H
#define _JOBS_H

// Max thread name lengt
#define MAX_THRD_NAME_LEN 64

// Max number of simultaneously running agent
// API threads we allow
#define MAX_THRD_COUNT 16

// Watchdog check time in seconds
#define WD_CHECK_TIMEOUT 10

// UTIL_THRD_t flags
#define THRD_FLAG_BUSY    0x00000001 // set if entry is taken
#define THRD_FLAG_STARTED 0x00000002 // the thread has started

// Thread function parameters structure
typedef struct {
  union {
    char *cptr_val;
    void *vptr_val;
    int  int_val;
  };
} THRD_PARAM_t;

// Thread function type
typedef void (*THRD_FUNC_t)(THRD_PARAM_t *p);

// Thread startup control structure
typedef struct {
    unsigned int wd_timeout; // Watchdog timeout in seconds (0 - off)
} THRD_CTL_t;

// Thread info type
typedef struct {
    int flags;    // thread flags
    int tid;      // Linux thread id
    pthread_t thread; // thread handle
    char name[MAX_THRD_NAME_LEN]; // thread name
    THRD_FUNC_t func;  // job function pointer
    THRD_PARAM_t param; // job function parameter
    unsigned long volatile wd_uptime; // Uptime of the last watchdog poll
    unsigned int volatile wd_timeout; // Watchdog timeout in seconds (0 - off)
} UTIL_THRD_t;


// Start a thread, returns 0 if successful or negative number if fails
// Params:
// name - thread name
// f - thread function
// p - thread function parameters structure (can be NULL)
// ctl - thread startup control structure (can be NULL to start w/ defaults)
int util_start_thrd(const char *name, THRD_FUNC_t f,
                    THRD_PARAM_t *p, THRD_CTL_t *ctl);

// Initialize our thread key (call during init);
void util_init_thrd_key(void);

// Returns TRUE if run in the main (or only) thread, FALSE otherwise
int util_is_main_thread(void);

// Set main thread info (should be called from the main
// thread itself at init). Returns 0 if successful;
int util_set_main_thrd(void);

// Set the calling thread watchdog timeout (in seconds, 0 to disable)
// It is checked every 10sec (i.e. shorter timeouts are not detected)
// Returns 0 if successful.
int util_wd_set_timeout(unsigned int timeout);

// Update the calling thread wd, returns 0 if successful
int util_wd_poll(void);

// Checks all the threads for watchdog timeout
// Restarts the agent if fails.
void util_wd_check_all(void);

#endif // _JOBS_H
