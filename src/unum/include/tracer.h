// (c) 2018-2019 minim.co
// Main unum include file that pulls in everything else

#ifndef _TRACER_H
#define _TRACER_H

// Max number of the processes and/or threads the tracer can monitor
#define MAX_TRACE_INFERIORS 64

// Max time to wait for process or thread to get into expected state
// before giving up and trying to recover (in seconds)
#define MAX_TRACE_WAIT_TIMEOUT 30

// Extract event id from witpid() status
#define WSTOPEVENT(_s) (((_s) & 0x00ff0000) >> 16)

// Crash info structure, pre-allocated on startup, freed by agent process
// after it is forked and reported the data to the server
typedef struct {
    // crash type name (exception class)
    char type[16];
    // crash location (pointer or better)
    char location[48];
    // crash message (readable description of the exception)
    char msg[256];
    // crash dump file upload url
    char dumpurl[192];
    // Length of the crash_data (if text w/ terminating 0)
    int data_len;
    // crash info data (binary or text w/ regs, modules loaded, stack...)
    char data[];
} TRACER_CRASH_INFO_t;

#ifdef AGENT_TRACE

// Cleanup and prepare for the new tracing session.
// main_pid - PID of the main thread that should be requesting
//            to be traced
// ci - location where the caller wants tracer to put the
//      pointer w/ crash data collected, reset inits it w/ NULL
// returns: 0 - if successful, negative val othervise
int tracer_reset(int main_pid, TRACER_CRASH_INFO_t **ci);

// Called at the start of the main thread/process being traced
// to request the tracer (should be its parent) to start monitoring
// returns: 0 - if successful, negative val othervise
int tracee_launch(void);

// Cleanup and prepare for the new tracing session.
// pid - PID of the thread triggering the update
// status - waitpid(-1, &status, __WCLONE) status
// ci - location where the caller wants tracer to put the
//      pointer w/ crash data  when/if collected
// returns: 0 - if successful, negative val othervise
int tracer_update(int pid, int status, TRACER_CRASH_INFO_t **ci);

// Complete the tracing session by detaching from all inferiors.
// returns: 0 - count of the detached inferiors
int tracer_stop(void);

#else  // AGENT_TRACE

// Stub all the functions
static __inline__
int tracer_reset(int a, TRACER_CRASH_INFO_t **b) { return 0; };
static __inline__
int tracee_launch(void) { return 0; };
static __inline__
int tracer_update(int a, int b, TRACER_CRASH_INFO_t **c) { return 1; };
static __inline__
int tracer_stop(void) { return 0; };

#endif // AGENT_TRACE

#endif // _TRACER_H
