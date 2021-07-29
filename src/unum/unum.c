// (c) 2017-2020 minim.co
// main entry point, option parsing, prepare and start the agent process

#include "unum.h"

// Array of init function pointers (the last enty is NULL)
INIT_FUNC_t init_list[] = { INITLIST, NULL };
// Array of init function names (matches the above)
char *init_str_list[] = { INITSTRLIST, NULL };

// The init level that we have executed all the init routines for
int init_level_completed = 0;

// Unum configuration context global structure
UNUM_CONFIG_t unum_config = {
    .telemetry_period          = TELEMETRY_PERIOD,
    .tpcap_time_slice          = TPCAP_TIME_SLICE,
    .tpcap_nice                = INT_MIN, // this value indicates no change
    .wireless_scan_period      = WIRELESS_SCAN_PERIOD,
    .wireless_telemetry_period = WIRELESS_TELEMETRY_PERIOD,
    .support_long_period       = SUPPORT_LONG_PERIOD,
    .fw_update_check_period    = FW_UPDATE_CHECK_PERIOD,
    .sysinfo_period            = SYSINFO_TELEMETRY_PERIOD,
    .ipt_period                = IPT_TELEMETRY_PERIOD,
    .config_path               = UNUM_CONFIG_PATH,
    .logs_dir                  = LOG_PATH_PREFIX,
    .dns_timeout               = DNS_TIMEOUT,
#if defined(FEATURE_MANAGED_DEVICE)
    .opmode                    = UNUM_OPMS_MD,
    .opmode_flags              = UNUM_OPM_MD,
#elif defined(FEATURE_LAN_ONLY)
    .opmode                    = UNUM_OPMS_AP,
    .opmode_flags              = UNUM_OPM_AP,
#else  // Has both LAN & WAN and not a managed device
    .opmode                    = UNUM_OPMS_GW,
    .opmode_flags              = UNUM_OPM_AUTO | UNUM_OPM_GW,
#endif // 
};

// long options table, we are using the option long name to encode
// the option type and the option usage scope - 2 bytes
// after the terminating zero, the option type:
// 'n' - no argument option
// 'c' - char string option
// 'i' - takes an integer number
// 'm' - run mode change option
// the option scope:
// 'o' - the option can be used in the command line only
// 'c' - config option that can be in both command line and config file
// 'a' - config option like above that can also be sent from cloud at activate
// Note: capital letter IDs are used for long-only options
static struct option long_options[] =
{
    {"help\0no",           no_argument,       NULL, 'h'},
    {"version\0no",        no_argument,       NULL, 'v'},
    {"product-id\0no",     no_argument,       NULL, 'z'},
    {"cfg-file\0co",       required_argument, NULL, 'f'},
    {"daemonize\0nc",      no_argument,       NULL, 'd'},
    {"unmonitored\0nc",    no_argument,       NULL, 'u'},
    {"skip-netcheck\0nc",  no_argument,       NULL, 'n'},
#ifdef DEBUG
    {"no-provision\0nc",   no_argument,       NULL, 'p'},
    {"set-url-pfx\0cc",    required_argument, NULL, 's'},
#endif // DEBUG
#ifdef SUPPORT_RUN_MODE
    {"support-period\0ic", required_argument, NULL, 'A'},
#endif // SUPPORT_RUN_MODE
#ifdef FW_UPDATER_RUN_MODE
    {"fwupd-period\0ic",   required_argument, NULL, 'B'},
#endif // FW_UPDATER_RUN_MODE
    {"pid-prefix\0cc",     required_argument, NULL, 'i'},
    {"trusted-ca\0cc",     required_argument, NULL, 'c'},
    {"mode\0mc",           required_argument, NULL, 'm'},
    {"opmode\0ca",         required_argument, NULL, 'o'},
#ifndef FEATURE_LAN_ONLY
    {"wan-if\0cc",         required_argument, NULL, 'w'},
#endif // FEATURE_LAN_ONLY
    {"lan-if\0ca",         required_argument, NULL, 'l'},
    {"rtr-t-period\0ia",   required_argument, NULL, 'C'},
    {"tpcap-period\0ia",   required_argument, NULL, 'D'},
    {"wscan-period\0ia",   required_argument, NULL, 'E'},
    {"rad-t-period\0ia",   required_argument, NULL, 'F'},
    {"ipt-period\0ia",     required_argument, NULL, 'G'},
    {"sysinfo-period\0ia", required_argument, NULL, 'H'},
    {"tpcap-nice\0ia",     required_argument, NULL, 'I'},
    {"dns-timeout\0ia",    required_argument, NULL, 'J'},
    {"cfg-trace\0ca",      required_argument, NULL, 'K'},
#ifdef UNUM_LOG_ALLOW_RELOCATION
    {"log-dir\0cc",        required_argument, NULL, 'L'},
#endif // UNUM_LOG_ALLOW_RELOCATION
#ifdef FEATURE_GZIP_REQUESTS
    {"gzip-requests\0ia",  required_argument, NULL, 'M'},
#endif //FEATURE_GZIP_REQUESTS
    {0, 0, 0, 0}
};
// The short options string for the above
static char *optstring = "hvzf:dunps:i:c:m:o:w:l:";
    
// Global variables for storing command line args
int arg_count;
char **arg_vector;

// File descriptor and PID file name used by the unum_handle_start() and
// unum_handle_stop()
static int proc_pid_file_fd = -1;
static char proc_pid_file[256] = "";

// Startup info (passed by the monitor to the inferior at startup to provide
// the reason for the start/re-start and in case of the restart due to
// a failure the associated error information)
UNUM_START_REASON_t process_start_reason;

// Helper to open file with close on exec flag
static int open_cloexec(char *name, int flags, int mode)
{
#ifdef O_CLOEXEC
   int fd = open(name, flags | O_CLOEXEC, mode);
#else  // !O_CLOEXEC
   int fd = open(proc_pid_file, flags, mode);
   if(fd != -1) {
     fcntl(fd, F_SETFD, FD_CLOEXEC);
   }
#endif // O_CLOEXEC
   return fd;
}

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

    if(suffix == NULL || *suffix == 0)
    {
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
    for(ii = 0; ii < AGENT_STARTUP_RETRIES; ++ii)
    {
        // add delay if not the first attempt
        if(ii > 0) {
            sleep(AGENT_STARTUP_RETRY_DELAY);
        }
        int fd = open_cloexec(proc_pid_file, O_CREAT | O_EXCL | O_RDWR, 0660);
        if(fd < 0)
        {
            // try to open for read/write if can't create the file
            if((fd = open_cloexec(proc_pid_file, O_RDWR, 0)) < 0) {
                log("%s: can't %s PID file <%s>: %s\n",
                    __func__, "open", proc_pid_file, strerror(errno));
                ret = -1;
                continue;
            }
        }
        // try to lock, that will fail if some other process created and locked
        // it or sneaked in and locked the file we created, the lock holds only
        // while the descriptor or its clones (duped or forked) are open.
        if(flock(fd, LOCK_EX | LOCK_NB) < 0)
        {
            log("%s: can't %s PID file <%s>: %s\n",
                __func__, "lock", proc_pid_file, strerror(errno));
            ret = 1;
            close(fd);
            continue;
        }
        // Successfully locked the file, if it was not created by us whoever did
        // it is already dead. Write our PID to the file.
        if(ftruncate(fd, 0) < 0)
        {
            log("%s: can't %s PID file <%s>: %s\n",
                __func__, "truncate", proc_pid_file, strerror(errno));
            close(fd);
            ret = -1;
            continue;
        }
        char pid_str[16];
        int len = sprintf(pid_str, "%d", getpid());
        if(write(fd, pid_str, strlen(pid_str)) != len)
        {
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
    int res = chdir("/");
    if(res != 0) {
        log("%s: chdir() failed: %s\n", __func__, strerror(res));
    }

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

    // Close fd as 2 & 1 point there now
    close(fd);

    // Ignore SIGINT, SIGQUIT and SIGPIPE
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);

    return;
}

// Prints program usage (if supplied options are invalid) and/or
// help (for -h option)
static void print_usage(int argc, char *argv[])
{
    printf("Usage: %s [OPTIONS...]\n", argv[0]);
    printf("This is Unum device management and monitoring agent.\n");
    printf("All the time intervals are in seconds.\n");
    printf("Options:\n");
    printf(" -h, --help                  - show usage\n");
    printf(" -v, --version               - show version\n");
    printf(" -z, --product-id            - show the device product name\n");
    printf(" -f, --cfg-file <PATHNAME>   - config (JSON) file pathname\n");
    printf("                               default: %s\n", UNUM_CONFIG_PATH);
    printf("                               format: {\"option\":<value>,...}\n");
    printf("                               use 0/1 for on/off options where\n");
    printf("                               no value is expected\n");
    printf(" -n, --skip-net-check        - skip network connectivity check\n");
    printf(" -d, --daemonize             - daemonize\n");
    printf(" -u, --unmonitored           - no monitoring process\n");
    printf(" -i, --pid-prefix            - PID file pathname prefix\n");
#ifdef DEBUG
    printf(" -s, --set-url-pfx           - custom 'http(s)://<host>' URL\n");
    printf("                                prefix\n");
#endif //DEBUG
    printf(" -m, --mode                  - set custom run mode\n");
#ifdef FW_UPDATER_RUN_MODE
    printf("                               u[pdater]: firmware updater\n");
#endif // FW_UPDATER_RUN_MODE
#ifdef SUPPORT_RUN_MODE
    printf("                               s[upport]: support portal agent\n");
#endif // SUPPORT_RUN_MODE
#ifdef DEBUG
    printf("                               t[1|2|3...]: run test 1,2,3...\n");
    printf(" -p, --no-provision          - skip provisioning (no client\n");
    printf("                               cert in the requests)\n");
#endif // DEBUG
    printf(" -o, --opmode <mode_name>    - set unum agent operation mode\n");
#if !defined(FEATURE_MESH_QCA) && !defined(FEATURE_MTK_EASYMESH)
# if !defined(FEATURE_LAN_ONLY)
    printf("                               gw: gateway (default)\n");
# endif // !FEATURE_LAN_ONLY
# if defined(FEATURE_AP_MODE) || defined(FEATURE_LAN_ONLY)
    printf("                               ap: wireless AP\n");
# endif // FEATURE_AP_MODE || FEATURE_LAN_ONLY
#endif // !FEATURE_MTK_EASYMESH && !FEATURE_MESH_QCA
#if defined(FEATURE_MANAGED_DEVICE)
    printf("                               md: managed device\n");
#endif // FEATURE_MANAGED_DEVICE
#if defined(FEATURE_MESH_11S)
# if !defined(FEATURE_LAN_ONLY)
    printf("                               mesh_11s_gw: gw w/ 802.11s mesh\n");
# endif // !FEATURE_LAN_ONLY
# if defined(FEATURE_AP_MODE) || defined(FEATURE_LAN_ONLY)
    printf("                               mesh_11s_ap: ap w/ 802.11s mesh\n");
# endif // FEATURE_AP_MODE || FEATURE_LAN_ONLY
#endif // FEATURE_MESH_11S
#if defined(FEATURE_MESH_QCA)
    printf("                               mesh_qca_gw: Qualcomm mesh gw\n");
    printf("                               mesh_qca_ap: Qualcomm mesh ap\n");
#endif // FEATURE_MESH_QCA
#if defined(FEATURE_MTK_EASYMESH)
# if !defined(FEATURE_LAN_ONLY)
    printf("                               mesh_em_gw: Mediatek EasyMesh gw\n");
# endif // !FEATURE_LAN_ONLY
# if defined(FEATURE_AP_MODE) || defined(FEATURE_LAN_ONLY)
    printf("                                mesh_em_ap: Mediatek EasyMesh ap\n");
# endif // defined(FEATURE_AP_MODE) || defined(FEATURE_LAN_ONLY)
#endif // FEATURE_MTK_EASYMESH
    printf(" -c, --trusted-ca <PATHNAME> - trusted CAs file pathname\n");
    printf(" -l, --lan-if <IFNAME_LIST>  - custom LAN interface name(s)\n");
    printf("                               separated by comma or space,\n");
    printf("                               list the main LAN ifname first,\n");
    printf("                               example: -l \"eth0 eth1 ...\"\n");
#ifndef FEATURE_LAN_ONLY
    printf(" -w, --wan-if <IFNAME>       - custom WAN interface name\n");
#endif // FEATURE_LAN_ONLY
    printf(" --rtr-t-period <5-120>      - router telemetry reporting\n");
    printf("                               interval\n");
    printf(" --tpcap-period <15-120>     - devices telemetry reporting\n");
    printf("                               interval\n");
    printf(" --tpcap-nice <-%d-%d>       - devices telemetry niceness\n",
                                           NZERO, NZERO - 1);
    printf(" --wscan-period <60-...>     - wireless scan interval\n");
    printf("                               0: disable the scan\n");
    printf(" --rad-t-period <15-120>     - radio and client telemetry\n");
    printf("                               reporting interval\n");
    printf(" --ipt-period <0-...>        - IP tables reporting interval\n");
    printf("                               0: disable reporting\n");
    printf(" --sysinfo-period <0-...>    - sysinfo reporting interval\n");
    printf("                               0: disable reporting\n");
    printf(" --dns-timeout <1-...>       - timeout in seconds for dns request\n");
#ifdef FEATURE_GZIP_REQUESTS
    printf(" --gzip-requests <0-...>      - message compression threshold\n");
    printf("                                0: no compression (default)\n");
#endif //FEATURE_GZIP_REQUESTS
#ifdef FW_UPDATER_RUN_MODE
    printf(" --fwupd-period <60-...>     - firmware upgrade check time\n");
    printf("                               for \"-m updater\"\n");
#endif //FW_UPDATER_MODE
#ifdef SUPPORT_RUN_MODE
    printf(" --support-period <%d-...>   - support connection attempt\n",
           SUPPORT_SHORT_PERIOD);
    printf("                               interval for \"-m support\"\n");
    printf("                               0: disable periodic retries\n");
#endif //SUPPORT_RUN_MODE
#ifdef UNUM_LOG_ALLOW_RELOCATION
    printf(" --log-dir <pathname>        - log files directory\n");
    printf("                               default: %s\n", LOG_PATH_PREFIX);
#endif // UNUM_LOG_ALLOW_RELOCATION
    printf(" --cfg-trace <pathname>      - capture configs sent to/from\n");
    printf("                               cloud (if pathname exists)\n");
    printf("                               default: none\n");
}

// Takes a list of interface names from the command line, separates them using
// a delimiter, performs error checking and stores approved names in the data 
// structure. Invalid names are ignored and an error is returned.
// The count of valid names is stored in count_ptr (if it is not NULL).
static int validate_ifnames(char* optarg, int *count_ptr)
{
    char lan_ifnames_buf[IFNAMSIZ * TPCAP_IF_MAX];
    char *token;
    int count = 0;
    int status = 0;
        
    // copy parameter into buffer
    strncpy(lan_ifnames_buf, optarg, sizeof(lan_ifnames_buf));
    lan_ifnames_buf[sizeof(lan_ifnames_buf) - 1] = 0;

    // walk through tokens
    for(token = strtok(lan_ifnames_buf, ", "); token != NULL;
        token = strtok(NULL, ", "))
    {
        if(strlen(token) < IFNAMSIZ && count < TPCAP_IF_MAX)
        {
            strncpy(unum_config.lan_ifname[count], token, IFNAMSIZ);
            unum_config.lan_ifname[count][IFNAMSIZ - 1] = 0;
            ++count;
        }
        else
        {
            status = -1;
        }
    }

    if(count_ptr) {
        *count_ptr = count;
    }

    return status;
}

// Function which will configure the agent from options that require 
// no arguments
static int do_config(char opt_long)
{
    int status = 0;

    switch(opt_long) 
    {
        case 'h':
            print_usage(arg_count, arg_vector);
            exit(EXIT_SUCCESS);
        case 'z':
            printf("%s\n", DEVICE_PRODUCT_NAME);
            exit(EXIT_SUCCESS);
        case 'v':
            printf("%s\n", VERSION);
            exit(EXIT_SUCCESS);
        case 'd':
            unum_config.daemon = TRUE;
            break;
        case 'n':
            unum_config.no_conncheck = TRUE;
            break;
#ifdef DEBUG
        case 'p':
            unum_config.no_provision = TRUE;
            break;
#endif // DEBUG   
        case 'u':
            unum_config.unmonitored = TRUE;
            break;            
        default:
            status = -1;
            break;
    }
  
    return status;
}

// Function which will configure the agent from options that require 
// int arguments
static int do_config_int(char opt_long, int optarg)
{
    int status = 0;

    // switched based off val returned by getopt
    switch(opt_long) 
    {
#ifdef SUPPORT_RUN_MODE
        case 'A':
            if(!unum_config.mode || *(unum_config.mode) != 's') {
                status = -1;
            } else if(optarg == 0) {
                unum_config.support_long_period = 0;
            } else if(optarg >= SUPPORT_SHORT_PERIOD) {
                unum_config.support_long_period = optarg;
            } else {
                status = -2;
            }
            break;
#endif // SUPPORT_RUN_MODE
#ifdef FW_UPDATER_RUN_MODE
        case 'B':
            if(!unum_config.mode || *(unum_config.mode) != 'u') {
                status = -3;
            } else if(optarg < 60) {
                status = -4;
            } else {
                unum_config.fw_update_check_period = optarg;
            }
            break;
#endif //FW_UPDATER_RUN_MODE
        case 'C':
            if(optarg < 5 || optarg > 120) {
                status = -5;
            } else {
                unum_config.telemetry_period = optarg;
            }
            break;
        case 'D':
            if(optarg < 15 || optarg > 120) {
                status = -6;
            } else {
                unum_config.tpcap_time_slice = optarg;
            }
            break;
        case 'E':
            if(optarg == 0) {
                unum_config.wireless_scan_period = optarg;
            } else if(optarg < 60) {
                status = -7;
            } else {
                unum_config.wireless_scan_period = optarg;
            }
            break;
        case 'F':
            if(optarg < 15 || optarg > 120) {
                status = -8;
            } else {
                unum_config.wireless_telemetry_period = optarg;
            }
            break;
        case 'I':
            if(optarg < -NZERO || optarg > NZERO - 1) {
                status = -9;
            } else {
                unum_config.tpcap_nice = optarg;
            }
            break;
        case 'G':
            if(optarg < 0) {
                status = -10;
            } else {
                unum_config.ipt_period = optarg;
            }
            break;
        case 'H':
            if(optarg < 0) {
                status = -11;
            } else {
                unum_config.sysinfo_period = optarg;
            }
            break;
        case 'J':
            if(optarg < 1) {
                status = -12;
            } else {
                unum_config.dns_timeout = optarg;
            }
            break;
#ifdef FEATURE_GZIP_REQUESTS
         case 'M':
            if(optarg < 0) {
                status = -13;
            } else {
                unum_config.gzip_requests = optarg;
            }
            break;
#endif //FEATURE_GZIP_REQUESTS
        default:
            status = -14;
            break;
    }
	
    return status;
}

// Function which will configure the agent from options that require 
// char arguments.
static int do_config_char(char opt_long, char *optarg)
{
    int status = 0;
    
    switch(opt_long)
    {
        case 'f':
            // config file - special case, just ignore here;
            break;
        case 'c':
            unum_config.ca_file = optarg;
            break;
#ifdef DEBUG
        case 's':
            unum_config.url_prefix = optarg;
            break;
#endif // DEBUG
        case 'i':
            unum_config.pid_fprefix = optarg;
            break;
        case 'l':
            if(validate_ifnames(optarg, &(unum_config.lan_ifcount)) != 0) {
                status = -1;
            }
            break;
        case 'w':
            if(strlen(optarg) > IFNAMSIZ) {
                status = -2;
                break;
            }
            strncpy(unum_config.wan_ifname, optarg, IFNAMSIZ);
            unum_config.wan_ifname[sizeof(unum_config.wan_ifname) - 1] = 0;
            unum_config.wan_ifcount = 1;
            break;                
        case 'L':
            if(!util_path_exists(optarg)) {
                status = -3;
            }
            unum_config.logs_dir = optarg;
            break;
        case 'K':
            // No check for error here to allow the option to be there
            // and the behavior controlled by creating/deleteing the folder.
            unum_config.cfg_trace = optarg;
            break;
        case 'o':
            status = util_set_opmode(optarg);
            if(status < 0) {
                // Give status offset to avoid overlap with other errors here
                status -= 10;
                break;
            }
            break;
        default:
            status = -4;
            break;
    }

    return status;
}

// Function that will store in config the requested run mode
static int do_config_mode(char *optarg)
{
    int status = 0;

    switch(*optarg)
    {
        default:
            status = -1;
            // drop down (in case nothing is defined)
#ifdef FW_UPDATER_RUN_MODE
        case 'u':
#endif // FW_UPDATER_RUN_MODE
#ifdef SUPPORT_RUN_MODE
        case 's':
#endif // SUPPORT_RUN_MODE
#ifdef DEBUG
        case 't':
#endif // DEBUG
            unum_config.mode = optarg;
            break;
    }
	
    return status;
}

// The function parses config JSON from the file or from the server activate
// response. The JSON can only contain the key-value pairs with the keys same 
// as the command line long options.
// at_activate - TRUE if passing null-terminated JSON string received from
//               server's activate response, FALSE if passing config file name
// file_or_data - filename or JSON data (if at_activate is TRUE)
// Note: the log() macro prints to stdout until the log subsystem
//       is initialized
int parse_config(int at_activate, char *file_or_data)
{   
    json_t *root, *json_val;
    json_error_t jerr;
    void *iter;
    const char *key;
    char *str_val;
    int int_val, opt_index;
    int status = 0;

    if(at_activate) {
        root = json_loads(file_or_data, JSON_DECODE_ANY, &jerr);
    } else {
        // load file into json, if file does not exist it's fine, nothing to do
        FILE *f = fopen(file_or_data, "r");
        if(!f) {
            return 0;
        }
        root = json_loadf(f, JSON_DECODE_ANY, &jerr);
        fclose(f);
    }
    if(!root) {
        log("%s: error in %s at l:%d c:%d parsing: '%s'\n",
            __func__, (at_activate ? "activate response" : file_or_data),
            jerr.line, jerr.column, jerr.text);
        return -1;
    }
    
    for(iter = json_object_iter(root); iter != NULL; 
        iter = json_object_iter_next(root, iter))
    {
        key = json_object_iter_key(iter);
        json_val = json_object_iter_value(iter);
	
        for(opt_index = 0;
            long_options[opt_index].name != NULL; ++opt_index)
        {
            if(strcmp(key, long_options[opt_index].name) == 0) {
                break;
            }
        }
        if(long_options[opt_index].name == NULL) {
            log("%s: unrecognized key: %s\n", __func__, key);
            continue;
        }

        // Get the option type and scope
        char opt_long = long_options[opt_index].val;
        const char *option_name = long_options[opt_index].name;
        char option_type = option_name[strlen(option_name) + 1];
        char option_scope = option_name[strlen(option_name) + 2];
        
        // Check if option is allowed during activate processing
        if(at_activate && option_scope != 'a') {
            log("%s: \"%s\" not allowed at activate\n", __func__, option_name);
            continue;
        }
        // Check if option is allowed during config processing
        if(!at_activate && option_scope != 'c' && option_scope != 'a') {
            log("%s: \"%s\" is for command-line only\n", __func__, option_name);
            continue;
        }

        // Switch depending on the type of the option
        switch(option_type)
        {
            // on/off commands which require true/false or 1/0
            // when found in the config file
            case 'n':
                // convert json to int
                if(json_is_boolean(json_val)) {
                    int_val = json_is_true(json_val);
                } else if(json_is_integer(json_val)) {
                    int_val = json_integer_value(json_val);
                } else {
                    log("%s: invalid value type for: %s\n", __func__, key);
                    status = -2;
                    break;
                }
                if(do_config(opt_long) != 0) {
                    log("%s: invalid value: %s for: %s\n",
                        __func__, (int_val ? "TRUE" : "FALSE"), key);
                    status = -3;
                    break;
                }
                if(at_activate) {
                    log("%s: turned %s %s at activate\n",
                        __func__, key, (int_val ? "on" : "off"));
                }
                break;
	
            // commands which require an int arg
            case 'i':
                // convert json to int
                if(!json_is_integer(json_val)) {
                    log("%s: invalid value type for: %s\n", __func__, key);
                    status = -4;
                    break;
                }
                int_val = json_integer_value(json_val);
                // config with value
                if(do_config_int(opt_long, int_val) != 0) {
                    log("%s: invalid value: %d for: %s\n",
                        __func__, int_val, key);
                    status = -5;
                    break;
                }
                if(at_activate) {
                    log("%s: set %s to %d at activate\n",
                        __func__, key, int_val);
                }
                break;

            case 'c':
                // convert json to char
                if(!json_is_string(json_val) ||
                   (str_val = (char *)json_string_value(json_val)) == NULL)
                {
                    log("%s: invalid value type for: %s\n", __func__, key);
                    status = -6;
                    break;
                }
                // config with the value
                if(do_config_char(opt_long, str_val) != 0)
                {
                    log("%s: invalid value: %s for: %s\n",
                        __func__, str_val, key);
                    status = -7;
                    break;
                }
                // Bump up refcount for the string value since it might be
                // stored as pointer (typical since we reuse cmdline handlers).
                // The value stays allocated for the lifetime of the process.
                json_incref(json_val);
                if(at_activate) {
                    log("%s: set %s to %s at activate\n",
                        __func__, key, str_val);
                }
                break;

            default:
                status = -8;
                log("%s: key: %s, not allowed in config\n",
                    __func__, key);
                break;
        }
    }

    // Free up unreferenced JSON resources
    json_decref(root);

    return status;
}

// Parse the command line options
// pick_cfg_file_name - if TRUE only pick the cfg file name from the options
static int parse_cmdline(int pick_cfg_file_name)
{
    int opt_long;
    int opt_index;
    int status = 0;

    // Reset getopt_long()
    optind = 0;

    // Parse options and fill in gobal configuration structure
    while((opt_long = getopt_long(arg_count, arg_vector, optstring,
                                  long_options, &opt_index)) > 0)
    {
        // If parsing error
        if(opt_long == ':' || opt_long == '?')
        {
            status = -1;
            break;
        }

        // The config file name is a special case, process it here or skip.
        if(opt_long == 'f')
        {
            if(pick_cfg_file_name)
            {
                struct stat st;
                unum_config.config_path = optarg;
                if(stat(unum_config.config_path, &st) != 0 && errno == ENOENT)
                {
                    printf("Warning, file does not exist: %s\n", optarg);
                }
            }
        }
        // If only looking for the cfg file name no parsing of the rest
        if(pick_cfg_file_name) {
            continue;
        }

        // Get the option name, type and scope
        for(opt_index = 0;
            long_options[opt_index].name != NULL &&
            long_options[opt_index].val != opt_long;
            ++opt_index);
        if(long_options[opt_index].name == NULL) {
            status = -1;
            break;
        }
        const char *option_name = long_options[opt_index].name;
        char option_type = option_name[strlen(option_name) + 1];
        char option_scope = option_name[strlen(option_name) + 2];
        
        // Any scope works here
        if(option_scope != 'o' && option_scope != 'c' && option_scope != 'a') {
            log("%s: invalid scope for option \"%s\"\n", __func__, option_name);
            continue;
        }

        // Switch depending on the type of the option
        switch(option_type)
        {
            // options which do not require any argument
            case 'n':
                if(do_config(opt_long) != 0) {
                    log("%s: invalid option \"%s\"\n", __func__, option_name);
                    status = -3;
                }
                break;

            // options which require an int arg
            case 'i':
                if(do_config_int(opt_long, atoi(optarg)) != 0) {
                    log("%s: invalid option \"%s\" value \"%s\"\n",
                        __func__, option_name, optarg);
                    status = -4;
                }
                break;

            // options which require a char* arg
            case 'c':
                if(do_config_char(opt_long, optarg) != 0) {
                    log("%s: invalid option \"%s\" value \"%s\"\n",
                        __func__, option_name, optarg);
                    status = -5;
                }
                break;

            // mode option
            case 'm':
                if(do_config_mode(optarg) != 0) {
                    log("%s: invalid run mode \"%s\"\n",
                        __func__, optarg);
                    status = -6;
                }
                break;

            default:
                log("%s: invalid option \"%s\"\n", __func__, option_name);
                status = -2;
                break;
        }

        // If error is detected stop parsing and retun it
        if(status != 0) {
            break;
        }
    }

    return status;
}

// Main entry point
int main(int argc, char *argv[])
{
    // store argc argv in the global variables
    arg_count = argc;
    arg_vector = argv;

    // Override all logging to go to stdout during initial startup stages
    set_proc_log_dst(LOG_DST_STDOUT);

    // Search command line for the config file option
    if(parse_cmdline(TRUE) != 0) {
        print_usage(arg_count, arg_vector);
        exit(EXIT_FAILURE);
    }

    // Load from the config file
    parse_config(FALSE, unum_config.config_path);

    // Update from the command line options
    if(parse_cmdline(FALSE) != 0) {
        print_usage(arg_count, arg_vector);
        exit(EXIT_FAILURE);
    }

    // Daemonize if requested
    if(unum_config.daemon) {
        do_daemon();
    }

    // Check and run code for the requested run mode
    if(unum_config.mode)
    {
        switch(*(unum_config.mode))
        {
#ifdef FW_UPDATER_RUN_MODE
            case 'u':
                // Until sure that updater never fails allow to monitor it.
                process_monitor(LOG_DST_UPDATE_MONITOR, "updater_monitor");
                return fw_update_main();
#endif // FW_UPDATER_RUN_MODE
#ifdef SUPPORT_RUN_MODE
            case 's':
                return support_main();
#endif // SUPPORT_RUN_MODE
#ifdef DEBUG
            case 't':
                // Crash handling test requires monitoring
                if(atoi(unum_config.mode + 1) == U_TEST_CRASH) {
                    unum_config.unmonitored = FALSE;
                    process_monitor(LOG_DST_CONSOLE, "test_monitor");
                }
                return run_tests(unum_config.mode + 1);
#endif // DEBUG
            default:
                // should never get here
                break;
        }
    }

    // Call the agent process monitor. If enabled it forks here and
    // the parent process monitors the child. The child continues
    // to do the real work.
    process_monitor(LOG_DST_MONITOR, "monitor");

    // The below code runs in the main agent thread.
    // The process-wide log redirection can now be stopped.
    clear_proc_log_dst();

    // Disable access to the logs the agent should not be touching
    unsigned long mask = (
#ifdef FW_UPDATER_RUN_MODE
      (1 << LOG_DST_UPDATE)         |
      (1 << LOG_DST_UPDATE_MONITOR) |
#endif // FW_UPDATER_RUN_MODE
#ifdef SUPPORT_RUN_MODE
      (1 << LOG_DST_SUPPORT)        |
#endif // SUPPORT_RUN_MODE
      0
    );
    set_disabled_log_dst_mask(mask);

    // The agent process is daemonized and monitored, 
    // now pass the control to the agent code
    return agent_main();
}
