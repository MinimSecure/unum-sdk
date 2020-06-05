// (c) 2018 minim.co
// process tracer for unum agent monitor

#ifdef AGENT_TRACE

#include "unum.h"

// Use to disable debug logging even in debug builds if it is too much
#undef log_dbg
#define log_dbg(...) /* Nothing */

// Structure for keeping state of the inferior (process or thread)
typedef struct _TRACER_I_STATE {
#define I_STATE_FREE    0 // The entry is unused
#define I_STATE_STOPPED 1 // The inferior is stopped
#define I_STATE_RUNNING 2 // The inferior is running
#define I_STATE_UNKNOWN 3 // The inferior state is yet unknown
    int state;   // state of the entry (see above)
    int pid;     // PID of the inferior
} TRACER_I_STATE_t;

// Monitored process main thread PID
static int process_main_pid = 0;

// Array of the monitored inferiors
static TRACER_I_STATE_t ifr[MAX_TRACE_INFERIORS];

// Tracing options
static int ptrace_opts = PTRACE_O_TRACECLONE   |
						 PTRACE_O_TRACEFORK    |
						 PTRACE_O_TRACEVFORK   |
						 PTRACE_O_TRACEEXEC;

// Tracing event names
static char *evt_names[] = {
	[PTRACE_EVENT_FORK] =  "EVENT_FORK",
	[PTRACE_EVENT_VFORK] = "EVENT_VFORK",
	[PTRACE_EVENT_VFORK_DONE] = "EVENT_VFORK_DONE",
	[PTRACE_EVENT_EXEC] = "EVENT_EXEC",
	[PTRACE_EVENT_EXIT] = "EVENT_EXIT",
	[PTRACE_EVENT_CLONE] = "EVENT_CLONE"
};


// Get event name
// Returns: static event name string or "EVENT_UNKNOWN"
static char *get_evt_name(int eid)
{
    char *name = "EVENT_UNKNOWN";
    if(eid >= 0 && eid < UTIL_ARRAY_SIZE(evt_names) && evt_names[eid] != NULL) {
        name = evt_names[eid];
    }
    return name;
}

// Find entry in the inferiors array by PID
// find_empty - TRUE to return free entry if PID not found
// Returns: ptr to the found entry or NULL
static TRACER_I_STATE_t *find_ifr(int pid, int find_empty)
{
    int ii;
    TRACER_I_STATE_t *found_ifr = NULL;

    log_dbg("%s: looking for pid: %d, find_empty: %d\n",
            __func__, pid, find_empty);
    for(ii = 0; ii < MAX_TRACE_INFERIORS; ++ii)
    {
        if(ifr[ii].state != I_STATE_FREE && ifr[ii].pid == pid) {
            found_ifr = &ifr[ii];
            break;
        }
        if(ifr[ii].state == I_STATE_FREE && find_empty && !found_ifr) {
            found_ifr = &ifr[ii];
        }
    }
    log_dbg("%s: ifr: %p, state: %d\n", __func__,
            found_ifr, (found_ifr ? found_ifr->state : -1));
    return found_ifr;
}

// Set state for a given entry
// Returns: old state
static int set_ifr_state(TRACER_I_STATE_t *ifr, int state)
{
    int old_state = I_STATE_FREE;
    if(ifr) {
        old_state = ifr->state;
        log_dbg("%s: pid: %d, state %d -> %d\n",
                __func__, ifr->pid, old_state, state);
        ifr->state = state;
    }
    return old_state;
}

// Release entry consumed by a given PID
// Returns: none
static void release_ifr(TRACER_I_STATE_t *ifr)
{
    if(set_ifr_state(ifr, I_STATE_FREE) != I_STATE_FREE) {
        log_dbg("%s: pid: %d, entry released\n", __func__, ifr->pid);
    }
    return;
}

// Add entry for a given PID
// state - initial state of the entry
// no_state_change - do not change state if the entry exists
// Returns: pointer to the new entry or NULL if cannot add
//          if the entry already exists it is reused
static TRACER_I_STATE_t *add_ifr(int pid, int state, int no_state_change)
{
    TRACER_I_STATE_t *ifr = find_ifr(pid, TRUE);
    if(!ifr) {
        log("%s: pid: %d, unable to add entry\n", __func__, pid);
        return ifr;
    }
    int old_state = ifr->state;
    ifr->pid = pid;
    if(old_state == I_STATE_FREE) {
        ifr->state = state;
        log_dbg("%s: pid: %d, new entry, state %d\n",
                __func__, pid, ifr->state);
    } else if(!no_state_change) {
        ifr->state = state;
        log_dbg("%s: pid: %d, reusing existent entry, state %d -> %d\n",
                __func__, pid, old_state, ifr->state);
    } else {
        log_dbg("%s: pid: %d, entry exists, state %d\n",
                __func__, pid, ifr->state);
    }
    log_dbg("%s: pid: %d, entry %p\n", __func__, pid, ifr);
    return ifr;
}

// Get new process PID for ptrace clone, fork, vfork and exec events
// Returns: event subject PID, 0 - if fails
static unsigned long get_evt_pid(int pid)
{
	unsigned long new_pid = 0;
	if(ptrace(PTRACE_GETEVENTMSG, pid, 0, &new_pid) < 0) {
        log("%s: pid: %d, PTRACE_GETEVENTMSG error: %s\n",
            __func__, pid, strerror(errno));
    }
	return new_pid;
}

// Cleanup and prepare for the new tracing session.
// main_pid - PID of the main thread that should be requesting
//            to be traced
// ci - location where the caller wants tracer to put the
//      pointer to structure w/ the crash data collected, reset
//      inits it w/ NULL
// returns: 0 - if successful, negative val othervise
int tracer_reset(int main_pid, TRACER_CRASH_INFO_t **ci)
{
    int err;
    log_dbg("%s: enter, main_pid: %d, ci: %p\n", __func__, main_pid, *ci);

    memset(ifr, 0, sizeof(ifr));
    process_main_pid = main_pid;
    err = 0;
    *ci = NULL;

    log_dbg("%s: done, err: %d, ci: %p\n", __func__, err, *ci);
    return err;
}

// Complete the tracing session by detaching from all inferiors.
// returns: 0 - count of the detached inferiors
int tracer_stop(void)
{
    int ii, count = 0;
    log_dbg("%s: enter\n", __func__);

    // Detach from all the tracked inferiors
    for(ii = 0; ii < MAX_TRACE_INFERIORS; ++ii)
    {
        if(ifr[ii].state != I_STATE_FREE && ifr[ii].pid > 0) {
            if(ptrace(PTRACE_DETACH, ifr[ii].pid, 0, 0) < 0) {
                log_dbg("%s: pid: %d, PTRACE_DETACH error: %s\n",
                        __func__, ifr[ii].pid, strerror(errno));
            }
            ifr[ii].state = I_STATE_FREE;
            ++count;
        }
    }

    log_dbg("%s: done, detached: %d\n", __func__, count);
    return count;
}

// Called at the start of the main thread/process being traced
// to request the tracer (should be its parent) to start monitoring
// returns: 0 - if successful, negative val othervise
int tracee_launch(void)
{
    int err;
    log_dbg("%s: main thread launched\n", __func__);
    for(;;) {
        if(ptrace(PTRACE_TRACEME, 0, 0, 0) < 0) {
            err = errno;
            log("%s: PTRACE_TRACEME error: %s\n", __func__, strerror(err));
            break;
        }
        if(raise(SIGSTOP) != 0) {
            err = errno;
            log("%s: raise(SIGSTOP) error: %s\n", __func__, strerror(err));
            break;
        }
        err = 0;
        break;
    }
    log_dbg("%s: done, err: %d\n", __func__, err);
    return err;
}

// Wait for a given PID to stop, max wait time MAX_TRACE_WAIT_TIMEOUT
static int wait_for_stop(int pid)
{
    int ii, w, status;
    for(ii = 0;
        (w = waitpid(pid, &status, __WALL | WNOHANG)) != pid &&
          (ii < MAX_TRACE_WAIT_TIMEOUT);
        ++ii)
    {
        sleep(1);
    }
    log_dbg("%s: pid: %d res %d after %d seconds wait\n",
            __func__, pid, w, ii);
    return ((w == pid) ? 0 : -1);
}

// Resume executing a stopped process/thread
// signal - signal to pass to the stopped process on resume
// hard_fail - if fails kill the main thread
// Returns: 0 - ok, negative - failed and killed the main
static int resume(TRACER_I_STATE_t *ifr, int signal, int hard_fail)
{
    int pid = ifr->pid;
    int err = ptrace(PTRACE_CONT, pid, 0, signal);
    if(err == 0) {
        log_dbg("%s: pid: %d resumed w/ signal %d\n",
                __func__, pid, signal);
        set_ifr_state(ifr, I_STATE_RUNNING);
    } else {
        log("%s: pid: %d, PTRACE_CONT has failed, %s\n",
            __func__, pid, strerror(errno));
        if(hard_fail) {
            log("%s: terminating main thread\n", __func__);
            kill(process_main_pid, SIGKILL);
        }
    }
    return err;
}

// Handle TRAP signal. Normally it should be due to us hitting
// one of the syscalls spawning new process/thread.
// Returns: 0 - if successful, negative othervise
int tracer_trap(TRACER_I_STATE_t *ifr, int eid)
{
    TRACER_I_STATE_t *new_ifr = NULL;
    int pid = ifr->pid;
    int err = 0;
    int new_pid = get_evt_pid(pid);

    log_dbg("%s: pid: %d, new_pid: %d, event: %s\n",
            __func__, pid, new_pid, get_evt_name(eid));

    // If no new PID, nothing we can do here
    if(new_pid == 0) {
        log("%s: pid: %d, error, TRAP w/ event: %s(%d), new_pid %d\n",
            __func__, pid, get_evt_name(eid), eid, new_pid);
        err = -1;
        return err;
    }

    // Get the new PID entry, could be NULL if we have not seen this PID yet
    new_ifr = find_ifr(new_pid, FALSE);

    switch(eid)
    {
        case PTRACE_EVENT_CLONE:
            log("%s: %d cloned %d\n", __func__, pid, new_pid);
            if(new_ifr == NULL) {
                // Wait till the new thread is stopped
                if(wait_for_stop(new_pid) != 0) {
                    log("%s: pid: %d not stopping, continue anyway\n",
                        __func__, new_pid);
                    break;
                }
                new_ifr = add_ifr(new_pid, I_STATE_STOPPED, TRUE);
                if(!new_ifr) {
                    // Try to resume the thread
                    ptrace(PTRACE_CONT, new_pid, 0, 0);
                    err = -2;
                    break;
                }
            }
            // Resume the new thread (parent is handled by the caller)
            resume(new_ifr, 0, TRUE);
            break;

        case PTRACE_EVENT_FORK:
        case PTRACE_EVENT_VFORK:
        case PTRACE_EVENT_EXEC:
            log("%s: %d %s %d\n", __func__, pid,
                (eid == PTRACE_EVENT_EXEC ? "exec" : "fork"), new_pid);
            if(new_ifr == NULL) {
                // Wait till the new process is stopped
                if(wait_for_stop(new_pid) != 0) {
                    log("%s: pid: %d not stopping, continue anyway\n",
                        __func__, new_pid);
                    break;
                }
            } else {
                release_ifr(new_ifr);
            }
            // We are not interested in tracing the subprocesses for now
            err = ptrace(PTRACE_DETACH, new_pid, 0, 0);
            break;

        default:
            log_dbg("%s: pid: %d unexpected event %d (%s)\n",
                    __func__, pid, eid, get_evt_name(eid));
            err = -4;
            break;
    }

    return err;
}

// Cleanup and prepare for the new tracing session.
// pid - PID of the process or thread triggering the update
// status - status reported by waitpid()
// ci - location where the caller wants tracer to put the
//      pointer w/ crash data  when/if collected
// returns: 0 - if successful, positive - ok, but monitor has to
//          handle it too, negative - to report failure
int tracer_update(int pid, int status, TRACER_CRASH_INFO_t **ci)
{
    int err, val;
    log_dbg("%s: enter, pid: %d, status: 0x%08x\n", __func__, pid, status);
    for(;;)
    {
        err = 0;

        // Get or add (if not seen yet) the process PID entry
        TRACER_I_STATE_t *ifr = add_ifr(pid, I_STATE_UNKNOWN, TRUE);
        if(ifr == NULL) {
            log_dbg("%s: pid: %d, unable to find/add entry\n", __func__, pid);
            err = -1;
            break;
        }

        // If PID has not been seen yet, presumably it is a thread we detected
        // before we saw the CLONE event on its parent. We keep the new thread
        // stopped till the parent gets notified.
        // The exception might be the main thread reporting the first time.
        if(ifr->state == I_STATE_UNKNOWN) {
            if(pid != process_main_pid) {
                log_dbg("%s: new pid: %d, ignoring for now\n", __func__, pid);
                break;
            }
            // If the thread is not stopped, can't do anything
            if(!WIFSTOPPED(status)) {
                log("%s: main thread startup, error, not stopped\n", __func__);
                err = -2;
                break;
            }
            set_ifr_state(ifr, I_STATE_STOPPED);
            log_dbg("%s: main thread startup, thread stopped\n", __func__);
            // Set the tracking options
            if(ptrace(PTRACE_SETOPTIONS, pid, 0, ptrace_opts) < 0) {
                log("%s: PTRACE_SETOPTIONS error: %s\n",
                    __func__, strerror(errno));
            }
            // Let the execution continue, pass the signal through unless
            // this is our startup SIGSTOP
            int stopsig = WSTOPSIG(status);
            if(stopsig == SIGSTOP) {
                stopsig = 0;
            }
            err = resume(ifr, stopsig, TRUE);
            if(err == 0) {
                log("%s: main thread %d has started and is traced\n",
                    __func__, pid);
            }
            break;
        }

        // Handle PID major lifetime events
        if(WIFEXITED(status)) {
            release_ifr(ifr);
            // do_monitor() code has to handle it for the main thread
            if(pid == process_main_pid) {
                err = 1;
                break;
            }
            log("%s: pid: %d, exited\n", __func__, pid);
            break;
        }
        if(WIFSIGNALED(status)) {
            val = WTERMSIG(status);
            release_ifr(ifr);
            // do_monitor() code has to handle it for the main thread
            if(pid == process_main_pid) {
                err = 1;
                break;
            }
            log("%s: pid: %d, terminated by signal %d\n", __func__, pid, val);
            break;
        }
        if(WIFCONTINUED(status)) {
            set_ifr_state(ifr, I_STATE_RUNNING);
            // do_monitor() code has to handle it for the main thread
            if(pid == process_main_pid) {
                err = 1;
                break;
            }
            log("%s: pid: %d, resumed due to SIGCONTINUE\n", __func__, pid);
            break;
        }

        // If none of the above the process should be in STOPPED state
        if(!WIFSTOPPED(status)) {
            log("%s: pid: %d, invalid state, not stopped\n",
                __func__, pid);
            err = -2;
            break;
        }

        // Set the state
        set_ifr_state(ifr, I_STATE_STOPPED);

        // The process is stopped, figure out the cause
        int signal = WSTOPSIG(status);
        int eid = WSTOPEVENT(status);

        // Seeing trap presumably due to our tracing of the syscalls
        if(signal == SIGTRAP) {
            log_dbg("%s: pid: %d, stopped by TRAP\n", __func__, pid);
            if(tracer_trap(ifr, eid) == 0) {
                // Continue the process after stopping
                err = resume(ifr, 0, TRUE);
                break;
            } else {
                log("%s: pid: %d, TRAP handling error, event: %s(%d)\n",
                    __func__, pid, get_evt_name(eid), eid);
            }
        }

        log_dbg("%s: pid: %d stopped, signal %d, event %d\n",
                __func__, pid, signal, eid);

        // Detect the events we are interested in, then pass call the
        // crashinfo handler to collect information about the agent state
        char *name = NULL;
        switch(signal)
        {
            case SIGSEGV: // various access violations
                name = "sigsegv";
                break;
            case SIGFPE:  // floating point op exception (division by 0)
                name = "sigfpe";
                break;
            case SIGILL:  // illegal instruction
                name = "sigill";
                break;
            case SIGBUS:  // bus error
                name = "sigbus";
                break;
            case SIGTRAP: // unexpected/unhandled trap
                name = "sigtrap";
                break;
            case SIGSYS:  // bad argument to routine (SVr4)
                name = "sigsys";
                break;
            case SIGXCPU: // CPU time limit exceeded (4.2BSD)
                name = "sigcpu";
                break;
            case SIGXFSZ: // file size limit exceeded (4.2BSD)
                name = "sigxfsz";
                break;
            case SIGUSR2: // allows to generate crash dump on demand
                name = "debug";
                break;
        }
        if(name != NULL) {
            util_crashinfo_get(process_main_pid, pid, name, ci);
        }

        err = resume(ifr, signal, TRUE);
        if(err != 0) {
            break;
        }
        // Never log anything for the threading library signals
        if(signal > SIGSYS && signal < SIGRTMIN) {
            break;
        }
        // do_monitor() code has to handle the main thread signals
        if(pid == process_main_pid) {
            err = 1;
            break;
        }
        // For the remaining cases log a message here
        log("%s: pid: %d, receiving signal %d\n", __func__, pid, signal);

        break;
    }
    log_dbg("%s: done, err: %d\n", __func__, err);
    return err;
}

#endif // AGENT_TRACE
