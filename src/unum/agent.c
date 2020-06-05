// (c) 2017-2019 minim.co
// agent code, init and start all subsystems, threads etc

#include "unum.h"


// If set the agent main thread has to exit
static UTIL_EVENT_t terminate_agent = UTIL_EVENT_INITIALIZER;

// The status code to exit with if the above event is set
static int terminate_status = 0;

// Flag turning on the use of the fallback URL
// (set by commcheck if DNS is not working)
static int use_fallback_url = FALSE;


// Turn on/off the fallback URL use flag.
void agent_use_fallback_url(int on)
{
    use_fallback_url = on;
}

// Turn on/off the fallback URL use flag.
int agent_is_fallback_url_on(void)
{
    return use_fallback_url;
}

// Function to call to tell the agent main thread to exit.
// Note, that the same function should work to exit a single
// threaded process as well.
void agent_exit(int status)
{
    terminate_status = status;
    if(util_is_main_thread()) {
        _exit(terminate_status);
    } else {
        UTIL_EVENT_SET(&terminate_agent);
        sleep(WD_CHECK_TIMEOUT);
    }
    // If it is still running, then something is wrong,
    // try to just kill everything.
    abort();
}

// Agent main init function
int agent_init(int level)
{
    return 0;
}

int agent_main(void)
{
    int level, ii, err;
    INIT_FUNC_t init_fptr;

    // Make sure we are the only instance and create the PID file
    err = unum_handle_start("agent");
    if(err != 0) {
        log("%s: startup failure, terminating.\n", __func__);
        util_restart(UNUM_START_REASON_START_FAIL);
        // should never reach this point
    }

    // Log agent operation mode (it still can change in activate or
    // when platform examines the config during init)
    log("%s: opmode: \"%s\"\n", __func__, unum_config.opmode);

    // Initialize subsystems
    for(level = 1; level <= MAX_INIT_LEVEL; level++)
    {
        log_dbg("%s: Init level %d...\n", __func__, level);
        for(ii = 0; init_list[ii] != NULL; ii++) {
            init_fptr = init_list[ii];
            err = init_fptr(level);
            if(err != 0) {
            	log("%s: Error at init level %d in %s(), terminating.\n",
                    __func__, level, init_str_list[ii]);
                util_restart(UNUM_START_REASON_START_FAIL);
                // should never reach this point
            }
        }
        init_level_completed = level;
    }
    log_dbg("%s: Init done\n", __func__);

    // Watchdog and exit requests check loop
    for(;;) {
        if(UTIL_EVENT_TIMEDWAIT(&terminate_agent, WD_CHECK_TIMEOUT * 1000) == 0)
        {
            _exit(terminate_status);
        }
        util_wd_check_all();
    }

    // In case we ever need to stop it gracefully
    unum_handle_stop();
    return 0;
}
