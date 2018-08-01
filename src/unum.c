// Copyright 2018 Minim Inc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// main entry point, option parsing, prepare and start the agent process

#include "unum.h"

// Array of init function pointers (the last enty is NULL)
INIT_FUNC_t init_list[] = { INITLIST, NULL };
// Array of init function names (matches the above)
char *init_str_list[] = { INITSTRLIST, NULL };

// Unum configuration contex global structure
UNUM_CONFIG_t unum_config = {
    .telemetry_period          = TELEMETRY_PERIOD,
    .tpcap_time_slice          = TPCAP_TIME_SLICE,
    .wireless_scan_period      = WIRELESS_SCAN_PERIOD,
    .wireless_telemetry_period = WIRELESS_TELEMETRY_PERIOD,
    .support_long_period       = SUPPORT_LONG_PERIOD,
    .fw_update_check_period    = FW_UPDATE_CHECK_PERIOD,
};

// File descriptor and PID file name used by the unum_handle_start() and
// unum_handle_stop()
static int proc_pid_file_fd = -1;
static char proc_pid_file[256] = "";

// Startup info (passed by the monitor to the inferior at startup to provide
// the reason for the start/re-start and in case of the restart due to
// a failure the associated error information)
UNUM_START_REASON_t process_start_reason;

// Check that the process is not already running and create its PID
// file w/ specified suffix.
// suffix  - PID file suffix ("monitor", "agent", or the op mode name from -m),
//           full pid file name /var/run/unum-<SUFFIX>.pid.
//           The call can be made w/ suffix == NULL. This cleans up state
//           inherited from parent that already called unum_handle_start()
// Returns: 0 - ok, negative - file operation error,
//          positive - already running (the file is locked)
// Note:  It is not thread safe (run it in the main thread of the process only)
// Note1: This function should normally be called for stratup and paired w/
//        unum_handle_stop() call before the exit.
// Note2: If the process is forked and the child might continue running without
//        the parent it should call unum_handle_stop(NULL) to clean up the
//        inherited from the parent state.
int unum_handle_start(char *suffix)
{
    int ii, ret;
    char *pid_fprefix = unum_config.pid_fprefix;

    if(suffix == NULL || *suffix == 0) {
        if(proc_pid_file_fd < 0) {
            log("%s: unable to reset PID file info, not yet initialized\n",
                __func__);
            return -1;
        }
        close(proc_pid_file_fd);
        proc_pid_file_fd = -1;
        *proc_pid_file = 0;
        return 0;
    }
    if(!pid_fprefix) {
        pid_fprefix = AGENT_PID_FILE_PREFIX;
    }
    snprintf(proc_pid_file, sizeof(proc_pid_file),
             "%s-%s.pid", pid_fprefix, suffix);
    ret = -1;
    for(ii = 0; ii < AGENT_STARTUP_RETRIES; ++ii) {
        // add delay if not the first attempt
        if(ii > 0) {
            sleep(AGENT_STARTUP_RETRY_DELAY);
        }
        int fd = open(proc_pid_file, O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC, 0660);
        if(fd < 0) {
            // try to open for read/write if can't create the file
            if((fd = open(proc_pid_file, O_RDWR|O_CLOEXEC)) < 0) {
                log("%s: can't %s PID file <%s>: %s\n",
                    __func__, "open", proc_pid_file, strerror(errno));
                ret = -1;
                continue;
            }
        }
        // try to lock, that will fail if some other process created and locked
        // it or sneaked in and locked the file we created, the lock holds only
        // while the descriptor or its clones (duped or forked) are open.
        if(flock(fd, LOCK_EX | LOCK_NB) < 0) {
            log("%s: can't %s PID file <%s>: %s\n",
                __func__, "lock", proc_pid_file, strerror(errno));
            ret = 1;
            close(fd);
            continue;
        }
        // Successfully locked the file, if it was not created by us whoever did
        // it is already dead. Write our PID to the file.
        if(ftruncate(fd, 0) < 0) {
            log("%s: can't %s PID file <%s>: %s\n",
                __func__, "truncate", proc_pid_file, strerror(errno));
            close(fd);
            ret = -1;
            continue;
        }
        char pid_str[16];
        int len = sprintf(pid_str, "%d", getpid());
        if(write(fd, pid_str, strlen(pid_str)) != len) {
            log("%s: can't %s PID file <%s>: %s\n",
                __func__, "write to", proc_pid_file, strerror(errno));
            close(fd);
            ret = -1;
            continue;
        }
        fsync(fd);
        proc_pid_file_fd = fd;
        ret = 0;
        break;
    }

    return ret;
}

// Clean up the process PID file (call right before exiting).
// Returns: none
// Note: It is not thread safe (run it in the main thread of the process only)
void unum_handle_stop()
{
    // Not started or already stopped
    if(proc_pid_file_fd < 0) {
        return;
    }
    // Delete the file without closing or unlocking the descriptor (it will
    // be closed by OS when the process terminates)
    unlink(proc_pid_file);
}

// Daemonize the process
static void do_daemon(void)
{
    struct sigaction sa;
    pid_t pid;
    int fd;

    // Fork off the parent process
    pid = fork();

    // An error occurred
    if(pid < 0) {
        log("%s: fork()%s failed: %s\n", __func__, " 1", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Let the parent terminate
    if(pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // The child process becomes the new session leader
    if(setsid() < 0) {
        log("%s: setsid() failed: %s\n", __func__, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Fork off for the second time to stop being the session leader
    // (this prevents the daemon process from acquiring the control
    // terminal when opening console for the log output)
    pid = fork();

    // An error occurred
    if(pid < 0) {
        log("%s: fork()%s failed: %s\n", __func__, " 2", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Let the parent terminate (2nd time)
    if(pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Change the working directory to the root
    chdir("/");

    // Close all open file descriptors
    for(fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--)
    {
        if(fd == 1 || fd == 2) {
            continue;
        }
        close(fd);
    }

    // Direct all logging to the console since we are losing
    // stdout and stderr
    set_proc_log_dst(LOG_DST_CONSOLE);

    // Redirect stdout and stderr
    fd = open(LOG_STDOUT, O_CREAT | O_WRONLY, 0666);
    if(fd < 0) {
        log("%s: open(%s) has failed: %s\n",
            __func__, LOG_STDOUT, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(dup2(fd, 2) < 0) {
        log("%s: dup2() for stderr failed: %s\n", __func__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(dup2(fd, 1) < 0) {
        log("%s: dup2() for stdout failed: %s\n", __func__, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Ignore SIGINT and SIGQUIT
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    return;
}

// Prints program usage (if supplied options are invalid) and/or
// help (for -h option)
static void print_usage(int argc, char *argv[])
{
    printf("Usage: %s [OPTIONS...]\n", argv[0]);
    printf("This is Unum device management and monitoring agent.\n");
    printf("Options:\n");
    printf(" -h, --help        - show usage\n");
    printf(" -v, --version     - show version\n");
    printf(" -n, --skip-check  - skip network connectivity check\n");
    printf(" -d, --daemonize   - daemonize\n");
    printf(" -u, --unmonitored - no monitoring process\n");
    printf(" -i, --pid-prefix  - PID file pathname prefix\n");
#ifdef DEBUG
    printf(" -s, --set-url-pfx - specify custom 'http(s)://<host>' URL part\n");
#endif //DEBUG
    printf(" -m, --mode        - start in special mode of opertion\n");
#ifdef FW_UPDATER_OPMODE
    printf("          u[pdater]: firmware updater mode\n");
#endif // FW_UPDATER_OPMODE
#ifdef SUPPORT_OPMODE
    printf("          s[upport]: support portal agent mode\n");
#endif // SUPPORT_OPMODE
#ifdef DEBUG
    printf("          t[1|2|3...]: run test 1, 2, 3...\n");
    printf(" -p, --no-provision       - skip provisioning");
    printf(" (no client cert in requests)\n");
#endif // DEBUG
    printf(" -c, --trusted-ca         - trusted CAs file pathname\n");
    printf(" --rtr-t-period <05-120>  - router telemetry report period\n");
    printf(" --tpcap-period <15-120>  - traffic counter and");
    printf(" packet capturing interval\n");
    printf(" --wscan-period <60-...>  - wireless neighborhood scan period\n");
    printf("                            0 will disable the scan\n");
    printf(" --rad-t-period <15-120>");
    printf("  - set radio and client telemetry period\n");
#ifdef FW_UPDATER_OPMODE
    printf(" --fwupd-period <60-...>  - firmware upgrade check time ");
    printf("for \"-m updater\"\n");
#endif //FW_UPDATER_MODE
#ifdef SUPPORT_OPMODE
    printf(" --suprt-period <%d-...>  - support connect attempt",
           SUPPORT_SHORT_PERIOD);
    printf(" period for \"-m support\"\n");
    printf("                            0 will disable it\n");

#endif //SUPPORT_OPMODE
}

// Main entry point
int main(int argc, char *argv[])
{
    // Override all logging to go to stdout during initial startup stages
    set_proc_log_dst(LOG_DST_STDOUT);

    int opt_long;
    int cmd_ind = 1;     // int for parsing command line arguments
    int status = FALSE;  // flag that indicates incorrect output
    int input;

    // long options table
    static struct option long_options[] =
    {
        {"help",         no_argument,       NULL, 'h'},
        {"version",      no_argument,       NULL, 'v'},
        {"daemon",       no_argument,       NULL, 'd'},
        {"unmonitored",  no_argument,       NULL, 'u'},
        {"skip-check",   no_argument,       NULL, 'n'},
#ifdef DEBUG
        {"no-provision", no_argument,       NULL, 'p'},
        {"set-url-pfx",  required_argument, NULL, 's'},
#endif // DEBUG
#ifdef SUPPORT_OPMODE
        {"suprt-period", required_argument, NULL, 'l'},
#endif // SUPPORT_OPMODE
#ifdef FW_UPDATER_OPMODE
        {"fwupd-period", required_argument, NULL, 'e'},
#endif // FW_UPDATER_OPMODE
        {"pid-prefix",   required_argument, NULL, 'i'},
        {"trusted-ca",   required_argument, NULL, 'c'},
        {"mode",         required_argument, NULL, 'm'},
        {"rtr-t-period", required_argument, NULL, 't'},
        {"tpcap-period", required_argument, NULL, 'r'},
        {"wscan-period", required_argument, NULL, 'w'},
        {"rad-t-period", required_argument, NULL, 'o'},
        {0, 0, 0, 0}
    };

    // Parse options and fill in gobal configuration structure
    while((opt_long = getopt_long(argc, argv,
                                  "hvdups:l:e:i:c:m:t:r:n:o:",
                                  long_options, NULL)) != -1)
    {
        // increment command line argument
        cmd_ind++;

        switch (opt_long) {
        case 'v':
            printf("%s\n", VERSION);
            if(cmd_ind == argc) {
                exit(EXIT_SUCCESS);
            }
            status = TRUE;
            break;
        case 'h':
            if(cmd_ind == argc) {
                print_usage(argc, argv);
                exit(EXIT_SUCCESS);
            }
            status = TRUE;
            break;
        case 'd':
            unum_config.daemon = TRUE;
            break;
        case 'u':
            unum_config.unmonitored = TRUE;
            break;
        case 'c':
            unum_config.ca_file = optarg;
            break;
#ifdef DEBUG
        case 'p':
            unum_config.no_provision = TRUE;
            break;
        case 's':
            unum_config.url_prefix = optarg;
            break;
#endif // DEBUG
#ifdef SUPPORT_OPMODE
        case 'l':
            input = atoi(optarg);
            if(input == 0) {
                unum_config.support_long_period = 0;
            }
            else if(input >= SUPPORT_SHORT_PERIOD) {
                unum_config.support_long_period = input;
            }
            else {
                status = TRUE;
            }
            break;
#endif // SUPPORT_OPMODE
#ifdef FW_UPDATER_OPMODE
        case 'e':
            input = atoi(optarg);
            if(input < 60) {
                status = TRUE;
            }
            else {
                unum_config.fw_update_check_period = input;
            }
            break;
#endif //FW_UPDATER_OPMODE
        case 'm':
            switch (*optarg) {
                case 'u':
                case 's':
#ifdef DEBUG
                case 't':
#endif // DEBUG
                    break;
                default:
                    print_usage(argc, argv);
                    exit(EXIT_FAILURE);
            }
            unum_config.mode = optarg;
            break;
        case 'n':
            unum_config.no_conncheck = TRUE;
            break;
        case 'i':
            unum_config.pid_fprefix = optarg;
            break;
        case 't':
            input = atoi(optarg);
            // check for invalid range
            if(input < 5 || input > 120) {
                status = TRUE;
            }
            else {
                unum_config.telemetry_period = input;
            }
            break;
        case 'r':
            input = atoi(optarg);
            // check for invalid range
            if(input < 15 || input > 120) {
                status = TRUE;
            }
            else {
                unum_config.tpcap_time_slice = input;
            }
            break;
        case 'w':
            input = atoi(optarg);
            if(input == 0) {
                unum_config.wireless_scan_period = input;
            }
            else if(input < 60) {
                status = TRUE;
            }
            else {
                unum_config.wireless_scan_period = input;
            }
            break;
        case 'o':
            input = atoi(optarg);
            // check for invalid
            if(input < 15 || input > 120) {
                status = TRUE;
            }
            else {
                unum_config.wireless_telemetry_period = input;
            }
            break;
        default:
            status = TRUE;
            break;
        }
        //if error was detected, print usage and exit with failure code
        if(status) {
            print_usage(argc, argv);
            exit(EXIT_FAILURE);
        }
    }
    if (optind != argc) {
        print_usage(argc, argv);
        exit(EXIT_FAILURE);
    }

    // Daemonize if requested
    if(unum_config.daemon) {
        do_daemon();
    }

    // Check and run code for the special modes of operation
    if(unum_config.mode) {
        switch (*(unum_config.mode)) {
#ifdef FW_UPDATER_OPMODE
        case 'u':
            // Until sure that updater never fails allow to monitor it
            process_monitor(LOG_DST_UPDATE_MONITOR, "updater_monitor");
            return fw_update_main();
#endif // FW_UPDATER_OPMODE
#ifdef SUPPORT_OPMODE
        case 's':
            return support_main();
#endif // SUPPORT_OPMODE
#ifdef DEBUG
        case 't':
            // Crash handling test requires monitoring
            if(atoi(unum_config.mode + 1) == U_TEST_CRASH) {
                unum_config.unmonitored = FALSE;
                process_monitor(LOG_DST_CONSOLE, "test_monitor");
            }
            return run_tests(unum_config.mode + 1);
#endif // DEBUG
        }
    }

    // Call the agent process monitor. If enabled it forks here and
    // the parent process monitors the child. The child continues
    // to do the real work.
    process_monitor(LOG_DST_MONITOR, "monitor");

    // The below code runs in the main agent thread, the process-wide log
    // redirection can now be stopped.
    clear_proc_log_dst();

    // Disable access to the logs the agent should not be touching
    unsigned long mask = (
#ifdef FW_UPDATER_OPMODE
      (1 << LOG_DST_UPDATE)         |
      (1 << LOG_DST_UPDATE_MONITOR) |
#endif // FW_UPDATER_OPMODE
#ifdef SUPPORT_OPMODE
      (1 << LOG_DST_SUPPORT)        |
#endif // SUPPORT_OPMODE
      0
    );
    set_disabled_log_dst_mask(mask);

    // The agent process is daemonized and monitored, now pass the control
    // to the agent code
    return agent_main();
}

