// (c) 2018 minim.co
// process monitor code

#include "unum.h"


// Use to disable debug logging even in debug builds if it is too much
#undef log_dbg
#define log_dbg(...) /* Nothing */

// Variable for storing the monitor process PID
// (set before monitored children are forked)
static pid_t monitor_pid = 0;

// Upload crash dump data from a buffer to S3 bucket
// url_tpl - template for the upload URL (%s - LAN MAC)
// buf,len - pointer to the buffer and the buffer length
// The function runs in its own process.
static int do_dump_upload(char *url_tpl, char *buf, int len)
{
    int level, ii, err, delay;
    INIT_FUNC_t init_fptr;

    // Upload process has no log of its own, use only debug logging here.
    // The process-wide log destination is forced to console here, disabling
    // it for non-dev builds to avoid the S3 auth header allowing uploads
    // from popping up on the console.
    unsigned long mask = ~(
#ifdef DEVELOPER_BUILD
      (1 << LOG_DST_CONSOLE) |
#endif // DEVELOPER_BUILD
      (1 << LOG_DST_STDOUT)
    );
    set_disabled_log_dst_mask(mask);

    // Go through the init levels up to to INIT_LEVEL_HTTP
    for(level = 1; level <= INIT_LEVEL_HTTP; level++)
    {
        for(ii = 0; init_list[ii] != NULL; ii++) {
            init_fptr = init_list[ii];
            err = init_fptr(level);
            if(err != 0) {
                log_dbg("%s: Error at init level %d in %s(), terminating.\n",
                        __func__, level, init_str_list[ii]);
                return -1;
            }
        }
    }

    // Make the HTTPs file upload request
    log_dbg("%s: uploading %d bytes to S3\n", __func__, len);
    delay = PROCESS_MONITOR_DUMP_UPLOAD_RETRY_TIME;
    http_rsp *rsp = NULL;
    err = -1;
    for(;rsp == NULL;) {
        char *my_mac = util_device_mac();
        char url[256];

        for(;;) {
            // Check that we have MAC address
            if(!my_mac) {
                log_dbg("%s: cannot get device MAC\n", __func__);
                break;
            }

            // Build the URL from the supplied template
            snprintf(url, sizeof(url), url_tpl, my_mac);
            rsp = http_put(url, "x-amz-acl: bucket-owner-full-control\0",
                           buf, len);
            if(rsp == NULL || (rsp->code / 100) != 2) {
                log("%s: upload has failed, code %d%s\n",
                    __func__, (rsp ? rsp->code : 0), (rsp ? "" : "(none)"));
                err = -2;
                break;
            }

            log_dbg("%s: success uploading %d bytes to %s\n",
                    __func__, len, url);
            err = 0;
            break;
        }

        if(rsp == NULL) {
            log_dbg("%s: next try in %d sec\n", __func__, delay);
            sleep(delay);
        }
    }

    if(rsp != NULL) {
        free_rsp(rsp);
        rsp = NULL;
    }

    // If here then the upload was successful
    log_dbg("%s: done\n", __func__);
    return err;
}

// Try to upload crash dump before attempting to start the inferior process
// again. The uploader runs in a new process, but we wait (up to timeout)
// till it is complete. The uploader must not log to the log file and can
// only inform us about the status using its exit code.
static void upload_dump(void)
{
    int w, status, val;
    unsigned long ts;
    TRACER_CRASH_INFO_t *ci = process_start_reason.ci;

    // Not crashed
    if(process_start_reason.code != UNUM_START_REASON_CRASH)
    {
        return;
    }
    // Crashed, but no crash info
    if(ci == NULL) {
        log_dbg("%s: crash info missing\n", __func__);
        return;
    }

    for(;;)
    {
        // No crash dump
        if(ci->data_len < 4) {
            log_dbg("%s: crash data is not available\n", __func__);
            break;
        }

        // Calculate the sum of the data in the dump (will use as a unique ID)
        unsigned int dumpsum = 0;
        unsigned int *ptr;
        for(ptr = (void *)ci->data;
            ((char *)ptr - ci->data) + sizeof(unsigned int) < ci->data_len;
            ptr++)
        {
            int rem = ci->data_len - ((void *)ptr - (void *)ci->data);
            if(rem >= sizeof(*ptr)) {
                dumpsum += *ptr;
            } else {
                int val = 0;
                memcpy(&val, ptr, rem);
                dumpsum += val;
            }
        }

        // Prepare the S3 URL template. Doing it so early complicates
        // things, we can't get MAC or use hash function till utils code is
        // initialized. The main portion of the name though is prepared here
        // and shared w/ the uploader process. It then does the final step
        // (%s is the MAC address).
        snprintf(ci->dumpurl, sizeof(ci->dumpurl),
                 PROCESS_MONITOR_DUMP_URL "/"
                 DEVICE_PRODUCT_NAME "/" VERSION "/%%s/%08x.txt",
                 dumpsum);

        // Fork the uploader to avoid adding to the risk of failure
        int pid = fork();
        // Child
        if(pid == 0) {
            int code = do_dump_upload(ci->dumpurl, ci->data, ci->data_len);
            _exit(code);
            // Done
        }
        // Parent waits till the child is done or times out
        val = -1;
        ts = util_time(1);
        for(;;)
        {
            // If we are out of time kill the uploader
            if(util_time(1) - ts > PROCESS_MONITOR_DUMP_UPLOAD_TIMEOUT) {
                log("%s: crash dump uploader has timed out, killing %d\n",
                    __func__, pid);
                kill(pid, SIGKILL);
            }
            // Give it some time...
            sleep(1);
            // Check the process status
            w = waitpid(pid, &status, WNOHANG);
            if(w < 0) {
                log("%s: waitpid() failed: %s\n", __func__, strerror(errno));
                val = -2;
                break;
            }
            // If nothing happened yet
            if(w != pid) {
                continue;
            }
            // The process has exited
            if(WIFEXITED(status)) {
                val = WEXITSTATUS(status);
                log("%s: crash dump uploader completed, status %d\n",
                    __func__, val);
                break;
            } else if(WIFSIGNALED(status)) {
                val = WTERMSIG(status);
                log("%s: crash dump uploader was killed by signal %d\n",
                    __func__, val);
                val = -3;
                break;
            }
            log("%s: crash dump uploader terminted, reason unknown 0x%x\n",
                __func__, status);
            val = -4;
            break;
        }

        // If upload has failed clear the url template in the crash info
        // structure (it tells activate that there is no dump).
        if(val != 0) {
            ci->dumpurl[0] = 0;
        }
        break;
    }

    // Reallocate the crash data buffer to save memory (we do not need
    // the dump data that take the most of it beyound this point).
    TRACER_CRASH_INFO_t *newci = UTIL_REALLOC(ci, sizeof(*ci));
    if(newci != NULL && newci != ci) {
        process_start_reason.ci = newci;
    }
    return;
}

// Forks when parent process starts monitoring the child process.
// The parent process control never leaves the function. The child
// returns and proceeds to run the remaining code.
// log_dst - allows to set monitor log destination (i.e. if using
//           the monitor for multiple different processes)
// pid_suffix - suffix for the PID file of the monitoring process
void process_monitor(int log_dst, char *pid_suffix)
{
    pid_t pid, w;
    int status, val;

    // Do nothing if monitoring is turned off
    if(unum_config.unmonitored) {
        return;
    }

    // since we are running with monitoring process direct all logging from
    // this process to the monitor log.
    set_proc_log_dst(log_dst);

    // Make sure we are the only instance and create the PID file
    status = unum_handle_start(pid_suffix);
    if(status != 0) {
        log("%s: startup failure, terminating.\n", __func__);
        return;
    }

    // Set monitor process pid var (this will be passed in to the child
    // and eventually used to detect if the child is being monitored)
    monitor_pid = getpid();

    pid = -1;
    for(;;)
    {
        // Fork off the parent process
        pid = fork();

        // An error occurred
        if(pid < 0) {
            log("%s: fork()%s failed: %s\n", __func__, "", strerror(errno));
            // Continue without monitoring
            unum_handle_stop();
            break;
        }

        // The child returns and continues execution
        if(pid == 0) {
            // The child is the process group leader, we can kill the
            // whole group when need to terminate and restart
            if(setpgid(0, 0) < 0) {
                log("%s: setpgid() failed: %s\n", __func__, strerror(errno));
            }
            // We need to start tracking the child separately, passing NULL
            // cleans up after the parent's unum_handle_start() call
            unum_handle_start(NULL);
            break;
        }

        // Cleanup and prepare for the new session
        tracer_reset(pid, &process_start_reason.ci);

        for(;;)
        {
            // Intercept events for all the child processes and threads
            w = waitpid(-1, &status, __WALL | WUNTRACED);
            if(w == -1) {
                log("%s: waitpid() failed: %s\n", __func__, strerror(errno));
                // Nothing to monitor, something is not right here, just exit
                unum_handle_stop();
                exit(EXIT_FAILURE);
            }

            // Inform the tracer of a monitored child thread state change
            val = tracer_update(w, status, &process_start_reason.ci);
            if(val < 0) {
                log("%s: tracer error %d updating pid:%d w/ status:0x%08x\n",
                    __func__, val, w, status);
            } else if(val == 0) {
                // The tracer did all the handling, ignore the event
                continue;
            }

            // React to the main thread state changes, all the rest - ignore
            if(w != pid) {
                continue;
            }

            if(WIFEXITED(status)) {
                val = WEXITSTATUS(status);
                log("%s: process %d exited, status %d\n", __func__, pid, val);
                if(val == EXIT_REBOOT) {
                    log("%s: rebooting the device...\n", __func__);
                    util_reboot();
                    // should never reach this point
                } else if(val == EXIT_SUCCESS || val == EXIT_FAILURE) {
                    log("%s: agent requested termination, status %d\n",
                        __func__, val);
                    exit(val);
                    // should never reach this point
                } else {
                    // Agent requested restart
                    process_start_reason.code = val;
                    log("%s: restarting the agent with reason code = %d\n",
                        __func__, val);
                }
                break;
            }
            else if(WIFSIGNALED(status)) {
                val = WTERMSIG(status);
                process_start_reason.code = UNUM_START_REASON_CRASH;
                if(val == 9) {
                    process_start_reason.code = UNUM_START_REASON_KILL;
                }
                log("%s: the agent was killed by signal %d, restarting...\n",
                    __func__, val);
                break;
            }
            else if(WIFSTOPPED(status)) {
                val = WSTOPSIG(status);
                log("%s: the agent was stopped by signal %d\n", __func__, val);
            }
#ifdef WIFCONTINUED
            else if(WIFCONTINUED(status)) {
                log("%s: the agent was resumed\n", __func__);
            }
#endif // WIFCONTINUED
        }

        // The wait loop has terminated, meaning that monitored process
        // main thread has exited, but we might have stale threads still
        // hanging out.

        // Detach tracer from still tracked threads
        tracer_stop();

        // Perform wait-kill-wait cycle to get rid of all the
        // processes in the group being terminated
        unsigned long cur_time = util_time(1);
        unsigned long kill_time = cur_time + MONITOR_TRMINATE_KILL_TIME;
        unsigned long stop_time = kill_time + MONITOR_TRMINATE_KILL_WAIT_TIME;
        log_dbg("%s: waiting for group %d to terminate\n", __func__, pid);
        for(;cur_time <= stop_time; cur_time = util_time(1))
        {
            w = waitpid(-pid, &status, __WALL | WNOHANG);
            // All done, can procede
            if(w < 0) {
                // Got an error other than "no more processes"
                if(errno != ECHILD) {
                    log("%s: wait for pgrp %d error: %s\n",
                        __func__, pid, strerror(errno));
                    break;
                }
                // Done waiting, all are terminated
                log_dbg("%s: done, no more processes in pgrp %d\n",
                        __func__, pid);
                break;
            }
            // Something happened to one of the processes in the group
            if(w > 0) {
                log_dbg("%s: pid %d status 0x%08x\n", __func__, w, status);
                continue;
            }
            // If not yet reached the kill time or already done it just wait
            if(kill_time == 0 || cur_time < kill_time) {
                sleep(1);
                continue;
            }
            // Try to kill the group if reached the kill time
            log("%s: attempting to kill pgrp %d\n", __func__, pid);
            kill(-pid, SIGKILL);
            kill_time = 0;
        }
        log_dbg("%s: waiting for group %d completed\n", __func__, pid);

        // Continuing here will restart the agent
    }

    // If pid is 0 then we successfully forked and about to start running the
    // agent code.
    if(pid == 0) {
        val = tracee_launch();
        if(val != 0) {
            log("%s: error %d starting agent process tracer\n", __func__, val);
        }
        upload_dump();
    }

    return;
}

// The function is intended to be called by the agent process that might be run
// under the supervision of the process monitor. It returns TRUE if the
// supervision is NOT detected, FALSE otherwise. It returns a negative value in
// case of an error. It is able to detect if the monitor process was killed and
// replaced with a debugger session.
int is_process_unmonitored(void)
{
    char str[128];
    if(monitor_pid == 0) {
        return TRUE; 
    }
    FILE *f = fopen("/proc/self/status", "r");
    if(NULL == f) {
        return -1;
    }
    int tracer_pid = 0;
    while(fgets(str, sizeof(str), f) != NULL) {
        if(sscanf(str, "TracerPid: %d", &tracer_pid) == 1) {
            break;
        }
    }
    fclose(f);
    // Note: return TRUE if NOT monitored
    return (tracer_pid != monitor_pid);
}
