// (c) 2017-2019 minim.co
// unum helper utils code

#include "unum.h"


// Get firmware version string (has to be static string)
char *util_fw_version()
{
#ifdef AGENT_VERSION_IS_FW_VERSION
    return VERSION;
#else  // AGENT_VERSION_IS_FW_VERSION
    return platform_fw_version();
#endif // AGENT_VERSION_IS_FW_VERSION
}

// Terminate agent.
// err - ERROR_SUCCESS or ERROR_FAILURE (any non-zero value is
//       treated as ERROR_FAILURE)
void util_shutdown(int err)
{
    log("Terminating unum %s, error code %d\n", VERSION, err);
    if(err == EXIT_SUCCESS) {
        agent_exit(EXIT_SUCCESS);
    }
    // Any non-zero err value is treated as EXIT_FAILURE since only
    // EXIT_SUCCESS and EXIT_FAILURE indicate to the monitor process
    // that the termination is intended.
    agent_exit(EXIT_FAILURE);
}

// Restart agent (the function does not return to the caller)
// reason - see enum unum_start_reason
void util_restart(int reason)
{
    log("Terminating unum %s, restart requested, reson %d\n", VERSION, reason);
    if(is_process_unmonitored()) {
        // ToDo: handle self restart
        log("The process is not monitored, restart manually\n");
    }
    agent_exit(reason);
}

// Restart command from server
void util_restart_from_server(void)
{
    util_restart(UNUM_START_REASON_SERVER);
}

// Reboot the device
void util_reboot(void)
{
#ifdef REBOOT_CMD
    char *reboot_cmd = REBOOT_CMD;

    log("Reboot using %s...\n", reboot_cmd);
    if(system(reboot_cmd) == 0) {
        sleep(15);
    }
#endif // REBOOT_CMD
    log("Reboot using syscall...\n");
    reboot(LINUX_REBOOT_CMD_RESTART);
    sleep(15);
    log("Failed to reboot, restarting the agent...\n");
    util_restart(UNUM_START_REASON_REBOOT_FAIL);
}

// Factory reset the device (triggers reboot if successful)
// Some platforms (like linux_generic) might just ignore it.
void util_factory_reset(void)
{
#ifdef FACTORY_RESET_CMD
    char *factory_reset_cmd = FACTORY_RESET_CMD;

    log("%s: using %s\n", __func__, factory_reset_cmd);
    system(factory_reset_cmd);
    sleep(5);
#else  // FACTORY_RESET_CMD
    // The platform might provide the function otherwise it will be NULL
    // (it's declared weak)
    if(util_platform_factory_reset != NULL) {
        util_platform_factory_reset();
    }
#endif // FACTORY_RESET_CMD
    log("%s: not supported on the platform\n", __func__);
}

// Get the command output and trim by removing trailing '\n' and '\r'
// characters
// cmd - Command to execute
// output - Command output buffer
// max_out_len - Maximum lenghth or output buffer
int util_get_cmd_output_and_trim(char *cmd, char *output,
                            int max_out_len)
{
    int pstatus;
    int ret = 0;

    if(util_get_cmd_output(cmd,
                           output, max_out_len, &pstatus) > 0)
    {
        if(pstatus == 0) {
            // Chop CR/LF
            int len;
            while((len = strlen(output)) > 0 &&
                  (output[len - 1] == '\r' ||
                   output[len - 1] == '\n'))
            {
                output[len - 1] = 0;
            }
        } else {
            // Some error while getting the output, zero it
            memset(output, 0, max_out_len);
            ret = -1;
        }
    }
    return ret;
}

// Populate auth_info_key. This is an unique identifier 
// that isn't as easy to guess as the MAC
// address. It can provide extra security during initial onboardning
// for the platforms that support it.
// auth_info_key - Auth Info key will be saved into this buffer if it exists
// max_key_len - Max length of auth_info_key buffer
#ifdef AUTH_INFO_GET_CMD
void util_get_auth_info_key(char *auth_info_key, int max_key_len)
{
    char *get_auth_info_key_cmd = AUTH_INFO_GET_CMD;
    if (util_get_cmd_output_and_trim(get_auth_info_key_cmd, auth_info_key,
                                    max_key_len) < 0) {
        log("%s: Failed to get the auth info key\n", __func__);
    }
}
#endif // AUTH_INFO_GET_CMD

// Populate Serial Number. This is an unique identifier 
// that isn't as easy to guess as the MAC
// address. It can provide extra security during initial onboardning
// for the platforms that support it.
// serail_num - Serial number will be saved into this buffer if it exists
// max_sn_len - Max length of serial_num buffer
#ifdef SERIAL_NUM_GET_CMD
void util_get_serial_num(char *serial_num, int max_sn_len)
{
    char *get_serial_num_cmd = SERIAL_NUM_GET_CMD;
    if (util_get_cmd_output_and_trim(get_serial_num_cmd, serial_num,
                                    max_sn_len) < 0) {
        log("%s: Failed to get Serial Number\n", __func__);
    }
}
#endif // SERIAL_NUM_GET_CMD

// Return uptime in specified fractions of the second (rounded to the low)
// Note: it's typically (but not necessarily) the same as uptime
unsigned long long util_time(unsigned int fraction)
{
    struct timespec t;
    unsigned long long l;

    // We might not have the define for CLOCK_MONOTONIC_RAW, but have the kernel
    // that supports it and given that we only compile for Linux the clock ID
    // is going to be the same.
    static int clock_id = 4; // CLOCK_MONOTONIC_RAW

    // If platform does not support it switch to CLOCK_MONOTONIC
    if(clock_gettime(clock_id, &t) < 0) {
        clock_id = CLOCK_MONOTONIC;
        clock_gettime(clock_id, &t);
    }

    l = (unsigned long long)t.tv_sec * fraction;
    l += ((unsigned long)t.tv_nsec) / (1000000000UL / fraction);

    return l;
}

// Sleep the specified number of milliseconds
void util_msleep(unsigned int msecs)
{
    struct timespec t;
    t.tv_sec = msecs / 1000;
    t.tv_nsec = (msecs % 1000) * 1000000;
    nanosleep(&t, NULL);
}

// Create a blank file
// file - Name of the file
// crmode - File Permissions
int touch_file (char *file, mode_t crmode)
{
    return creat(file, crmode);
}

// Save data from memory to a file, returns -1 if fails.
// crmode - permissions for creating new files (see man 2 open)
int util_buf_to_file(char *file, void *buf, int len, mode_t crmode)
{
    int fd = open(file, O_CREAT|O_WRONLY|O_TRUNC, crmode);
    if(fd < 0) {
        log("%s: open(%s) error: %s\n", __func__,
            file, strerror(errno));
        return -1;
    }
    if(write(fd, buf, len) != len) {
        log("%s: write to %s error: %s\n", __func__,
            file, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

// Read data from file to a buffer
// file - Name of the file
// buf - Buffer to save the data to
// buf_size - Maximum size of the buffer
// Returns: negative - error, use errno to get the error code
//          positive - number of bytes read or the file size if
//                     buf_size was not large enough to read all
size_t util_file_to_buf(char *file, char *buf, size_t buf_size)
{
    size_t len = 0;
    int fd = open(file, O_RDONLY);
    if(fd < 0) {
        return -1;
    }
    len = read(fd, buf, buf_size);
    if(len < 0)
    {
        int err = errno;
        close(fd);
        errno = err;
        return -2;
    }
    struct stat st;
    if(fstat(fd, &st) == 0 && len < (size_t)st.st_size) {
        len = st.st_size;
    }
    close(fd);
    return len;
}

// Compare two files.
// Parameters: 
// file1, file2 - pathnames of the files to compare
// must_exist - flag that tells the functon how to treat the non-existent files,
//              if TRUE both files must exist, if FALSE the non-existen files
//              are treated as empty.
// Returns: TRUE - files match, FALSE - differ, negative - error accesing
//          file(s) (-1 - file1, -2 - file2)
int util_cmp_files_match(char *file1, char *file2, int must_exist)
{
    struct stat st1, st2;
    unsigned char buf1[512];
    unsigned char buf2[512];
    long file1_len, file2_len;
    
    file1_len = file2_len = 0;
        
    // open files    
    FILE *fd1 = fopen(file1, "r");
    FILE *fd2 = fopen(file2, "r");    
    
    if(stat(file1, &st1) == 0) {
        file1_len = st1.st_size;
    } else if(must_exist || errno != ENOENT) {
        return -1;
    }

    if(stat(file2, &st2) == 0) {
        file2_len = st2.st_size;
    } else if(must_exist || errno != ENOENT) {
        return -2;
    }

    if(file1_len != file2_len) {
        return FALSE;
    } else if(file1_len == 0) {
        return TRUE;
    }
    
    do{
        int status = 0;    

        if (fd1 != NULL && fd2 != NULL)
        {
            // read files into memory
            int len1 = fread(buf1, sizeof(char), file1_len, fd1);
            int len2 = fread(buf2, sizeof(char), file2_len, fd2);
            
            // finish with null terminator
            buf1[++len1] = '\0';
            buf2[++len2] = '\0';

            // close files
            fclose(fd1);
            fclose(fd2);

            // compare buffers
            status = memcmp(buf1, buf2, 512);

            if(status == 0) {
                return TRUE;
            }
        }

        return FALSE;

    } while(0);

}

// Replace CR/LF w/ LF in a string
void util_fix_crlf(char *str_in)
{
    char *str;
    if(!str_in) {
        return;
    }
    for(str = strstr(str_in, "\r\n");
        str;
        str = strstr(str, "\r\n"))
    {
        memmove(str, str + 1, strlen(str));
    }
    return;
}

// In general it is similar to system() call, but it does not change any
// signal handlers.
// It also allows to specify callback "cb" and time period in sconds
// after which to call that callback. Parameters:
// cmd - the commapd line to pass to shell
// cb_time - positive - specifies time in seconds till callback is called,
//           0 - call unconditionally after fork() (if process is created),
//           negative - the callback calling is disabled
// cb - pointer to the callback function (see below), NULL - no call
// cb_prm - parameter to pass to the callback function
//
// The UTIL_SYSTEM_CB_t callback is passed in the following parameters:
// pid     - PID of the running process
// elapsed - the time in seconds since the command started
// cb_prm  - caller's parameter (void pointer)
// The callback return value can be negative to disable future calling.
// If the value is positive it specifies the number of seconds till the
// next call to the callback (0 still means call in 1 second).
int util_system_wcb(char *cmd, int cb_time, UTIL_SYSTEM_CB_t cb, void *cb_prm)
{
    int ret, pid, status;
    unsigned int time_count = 0;
    int timeout = cb_time;

    if(!cmd) {
        return 1;
    }

    if((pid = fork()) < 0) {
        return -1;
    }
    if(pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }

    // See if we need to make the callack on startup call
    if(cb != NULL && timeout == 0) {
        timeout = cb(pid, time_count, cb_prm);
    }

    for(;;) {
        ret = waitpid(pid, &status, WNOHANG);
        if(ret == pid) {
            return status;
        } else if(ret < 0) {
            return -1;
        }
        sleep(1);
        ++time_count;
        if(cb != NULL && timeout >= 0 && time_count >= timeout) {
            ret = cb(pid, time_count, cb_prm);
            if(ret < 0) {
                timeout = ret;
            } else {
                timeout = time_count + ret;
            }
        }
    }

    return -1;
}

// In general it is similar to system() call, but tries to kill the
// command being waited for if the timeout (in seconds) is reached
// and it does not change any signal handlers.
// If pid_file is not NULL the function tries to store in that file
// the PID of the running process while it is executing.
static int util_system_cb(int pid, unsigned int elapsed, void *cb_ptr)
{
    void **params = cb_ptr;
    unsigned int timeout = *((unsigned int *)(*params));
    char *pid_file = (char *)(*(params + 1));
    FILE *f = NULL;

    // Handle the first call after creating the process
    if(elapsed == 0) {
        if(pid_file && (f = fopen(pid_file, "w")) != NULL)
        {
            fprintf(f, "%d", pid);
            fclose(f);
            f = NULL;
        }
        return (signed int)timeout;
    }

    // Handle the second call (could be only due to the timeout)
    kill(pid, SIGKILL);
    return -1;
}
int util_system(char *cmd, unsigned int timeout, char *pid_file)
{
    void *params[] = { &timeout, pid_file };

    int ret = util_system_wcb(cmd, 0, util_system_cb, (void *)params);

    // Get rid of the PID file
    if(pid_file) {
        unlink(pid_file);
    }

    return ret;
}

// Call cmd in shell and capture its stdout in a buffer.
// Returns: negative if fails to start cmd (see errno for the error code), the
//          length of the captured info (excluding terminating 0) if success.
//          If the returned value is not negative the pstatus (unless NULL)
//          contains command execution status (zero if the command terminated
//          with status 0, negative if error).
// If the buffer is too short the remainig output is ignored.
// The captured output is always zero-terminated.
int util_get_cmd_output(char *cmd, char *buf, int buf_len, int *pstatus)
{
    int len = 0;

    FILE *fp = popen(cmd, "r");
    if(fp == NULL) {
        return -1;
    }

    while(fgets(buf + len, buf_len - len, fp) != NULL)
    {
        len += strlen(buf + len);

        // fgets always reads a max of size - 1
        // So compare length with buf_len - 1.
        // Otherwise, we will end-up in an infintie loop when len = buf_len - 1
        if(len >= (buf_len - 1)) {
            // Run out of the buffer space
            while(fgetc(fp) != EOF);
            break;
        }
    }

    int ret = pclose(fp);
    if(pstatus != NULL) {
        *pstatus = ret;
    }

    return len;
}

// Seed for the hash function (set at init and must stay constant)
static unsigned int hash_seed;

// Hash function
// dptr - ptr to data, must be 4-bytes aligned
// len - length of data
// Returns: 32 bit hash
unsigned int util_hash(void *dptr, int len)
{
    unsigned char *data = (unsigned char *)dptr;
    unsigned int m = 0x5bd1e995;
    int r = 24;
    unsigned int h = hash_seed ^ len;

    while(len >= 4)
    {
        unsigned int k;

        k  = *((unsigned int *)data);

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    switch(len)
    {
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0];
                h *= m;
    };

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

// Clean up multiple tabs & spaces from a string
// str - ptr to the string to clean up
void util_cleanup_str(char *str)
{
    int len = strlen(str) + 1;
    int ii, spaces;

    spaces = 0;
    for(ii = 0; ii < len; ++ii) {
        if(str[ii] == ' ' || str[ii] == '\t') {
            spaces++;
            continue;
        }
        if(spaces <= 1) {
            spaces = 0;
            continue;
        }
        memmove(&str[ii - (spaces - 1)], &str[ii], len - ii);
        len -= (spaces - 1);
        ii -= (spaces - 1);
        spaces = 0;
    }
    return;
}

// Match pattern to a string
// ptr - pointer to the pattern, the pattern can contain following
//       special symbols:
//       '*' - matches 0 or more characters in the target string
//       '?' - matches exactly one character
//       '\' - escape (prefix any special pattern symbol to treat it as is)
// ptr_len - length of the pattern (can be -1 if the pattern is 0-terminated)
// str - pointer to a string to try matcing to the pattern
// str_len - length of the string (can be -1 if the string is 0-terminated)
// Returns: 0 - no match, 1 - match
// Note: the function uses space on stack proportional to the string length
//       (approximately 1 bit per character, i.e. 1KB for a 8K long string)
// Note1: incomplete escape sequence at the end (i.e. "abc\") is ignored
int util_match_str(char *ptr, int ptr_len, char *str, int str_len)
{
    // Set lengths if not provided by the caller
    if(ptr_len < 0) {
        ptr_len = strlen(ptr);
    }
    if(str_len < 0) {
        str_len = strlen(str);
    }
    // The work area has to fit str_len + 1 bits
    int res_len = ((str_len + (sizeof(int) * 8)) / (sizeof(int) * 8));
    int res[res_len];
    memset(res, 0, sizeof(res));

    // Prepare state vars and work areas
    UTIL_BIT_SET(res, 0);
    int esc = FALSE;
    int p_i, s_i = 0;

    // Loop through pattern (1 - the first letter, 0 - empty string)
    // Example: ptr = "b*m", str = boom
    //       b o o m
    //     0 1 2 3 4
    //   0 .
    // b 1   .
    // * 2   . . . .
    // m 3         . <- match result
    for(p_i = 1; p_i <= ptr_len; ++p_i)
    {
        int res_prev = 0;
        char p_c = ptr[p_i - 1];
        if(!esc)
        {
            if(p_c == '\\') { // escape sequence detected
                esc = TRUE;
                continue;
            }
            if(p_c != '*') { // only '*' matches empty string
                res_prev = UTIL_BIT_GET(res, 0);
                UTIL_BIT_CLR(res, 0);
            }
        }
        for(s_i = 1; s_i <= str_len; ++s_i)
        {
            int val;
            char s_c = str[s_i - 1];
            if(!esc && p_c == '*')
            {
                val = UTIL_BIT_GET(res, s_i - 1);
                val |= UTIL_BIT_GET(res, s_i);
            }
            else if(!esc && p_c == '?')
            {
                val = res_prev;
            }
            else
            {
                val = res_prev & (p_c == s_c);
            }
            res_prev = UTIL_BIT_GET(res, s_i);
            if(val) {
                UTIL_BIT_SET(res, s_i);
            } else {
                UTIL_BIT_CLR(res, s_i);
            }
        }
        // we get here only after processing the escaped char, clear flag
        esc = FALSE;
    }
    
    return UTIL_BIT_GET(res, str_len);
}

// Configure the agent to the specified operation mode
// opmode - the operation mode string pointer
// Returns: 0 - success, negative - error setting the opmode
// Note: this function is called from command line procesing routines
//       before any init is done (i.e. even the log msgs would go to stdout)
int util_set_opmode(char *opmode)
{
    int skip_generic_check = FALSE;
    int new_flags = 0;
    char *new_mode = NULL;

// QCA mesh devices (only support QCA mesh AP/GW modes)
#if defined(FEATURE_MESH_QCA)
    if(strcmp(opmode, UNUM_OPMS_MESH_QCA_GW) == 0) {
        new_flags = UNUM_OPM_GW | UNUM_OPM_MESH_QCA;
        new_mode = UNUM_OPMS_MESH_QCA_GW;
    } else if(strcmp(opmode, UNUM_OPMS_MESH_QCA_AP) == 0) {
        new_flags = UNUM_OPM_AP | UNUM_OPM_MESH_QCA;
        new_mode = UNUM_OPMS_MESH_QCA_AP;
    } else  {
        // Unrecognized mode, keeping the flags unchanged
        // Mesh cannot be disabled for FEATURE_MESH_QCA
        return -1;
    }
    skip_generic_check = TRUE;
#endif // FEATURE_MESH_QCA

// MTK EasyMesh devices (only support, currently different HW kinds for
// router and extenders, each supports only its own mode of operation)
#if defined(FEATURE_MTK_EASYMESH)
# if !defined(FEATURE_LAN_ONLY)
    if(strcmp(opmode, UNUM_OPMS_MTK_EM_GW) == 0) {
        new_flags = UNUM_OPM_GW | UNUM_OPM_MTK_EM;
        new_mode = UNUM_OPMS_MTK_EM_GW;
    } else
# endif // !FEATURE_LAN_ONLY
# if defined(FEATURE_AP_MODE) || defined(FEATURE_LAN_ONLY)
    if(strcmp(opmode, UNUM_OPMS_MTK_EM_AP) == 0) {
        new_flags = UNUM_OPM_AP | UNUM_OPM_MTK_EM;
        new_mode = UNUM_OPMS_MTK_EM_AP;
    } else
# endif // defined(FEATURE_AP_MODE) || defined(FEATURE_LAN_ONLY)
    {
        // Unrecognized mode, keeping the flags unchanged
        // Mesh cannot be disabled for FEATURE_MTK_EASYMESH
        return -1;
    }
    skip_generic_check = TRUE;
#endif // FEATURE_MTK_EASYMESH

// Managed device (only supports its own mode of operation)
#if defined(FEATURE_MANAGED_DEVICE)
    if(strcmp(opmode, UNUM_OPMS_MD) == 0) {
        new_flags = UNUM_OPM_MD;
        new_mode = UNUM_OPMS_MD;
    } else  {
        // Unrecognized mode, keeping the flags unchanged
        // Mesh cannot be disabled for FEATURE_MANAGED_DEVICE
        return -1;
    }
    skip_generic_check = TRUE;
#endif // FEATURE_MANAGED_DEVICE

// 802.11s mesh, can be turned on and off
#if defined(FEATURE_MESH_11S)
# if !defined(FEATURE_LAN_ONLY)
    if(strcmp(opmode, UNUM_OPMS_MESH_11S_GW) == 0) {
        new_flags = UNUM_OPM_GW | UNUM_OPM_MESH_11S;
        new_mode = UNUM_OPMS_MESH_11S_GW;
        skip_generic_check = TRUE;
    } else
# endif // !FEATURE_LAN_ONLY
    if(strcmp(opmode, UNUM_OPMS_MESH_11S_AP) == 0) {
        new_flags = UNUM_OPM_AP | UNUM_OPM_MESH_11S;
        new_mode = UNUM_OPMS_MESH_11S_AP;
        skip_generic_check = TRUE;
    }
#endif // FEATURE_MESH_11S

    // Check for generic modes of operation.
    // First see if we've already found the mode name match and 
    // no longer need to check for the generic modes.
    if(skip_generic_check) {
       // Nothing to do here, new_flags, new_mode should be set already
    } else
#if !defined(FEATURE_LAN_ONLY)
    if(strcmp(opmode, UNUM_OPMS_GW) == 0) {
        new_flags = UNUM_OPM_GW;
        new_mode = UNUM_OPMS_GW;
    } else
#endif // !FEATURE_LAN_ONLY
#if defined(FEATURE_AP_MODE) || defined(FEATURE_LAN_ONLY)
    if(strcmp(opmode, UNUM_OPMS_AP) == 0) {
        new_flags = UNUM_OPM_AP;
        new_mode = UNUM_OPMS_AP;
    } else
#endif // FEATURE_AP_MODE || FEATURE_LAN_ONLY
    {
        // Unrecognized mode, keeping the flags unchanged
        return -1;
    }

    // If opmode change is requested after the setup stage of the init is done
    // let the platform handle the opmode change (it might need to do something
    // special or limit possible changes)
    if(init_level_completed >= INIT_LEVEL_SETUP  &&
       strcmp(unum_config.opmode, new_mode) != 0 &&
       platform_change_opmode != NULL            &&
       platform_change_opmode(unum_config.opmode_flags, new_flags) != 0)
    {
        return -2;
    }

    // All checks passed, change opmode
    unum_config.opmode = new_mode;
    unum_config.opmode_flags = new_flags;
    return 0;
}

// Subsystem init function
int util_init(int level)
{
    int ret = 0;

    // If platform implemented its own init routine, call it
    if(util_platform_init != NULL) {
        ret |= util_platform_init(level);
    }
    // Initialize random number generator
    if(level == INIT_LEVEL_RAND) {
        unsigned int seed = 0;
        unsigned int a, b, c, d;
        char *mac;
        if((mac = util_device_mac()) != NULL) {
            sscanf(mac, "%*02x:%*02x:%02x:%02x:%02x:%02x", &a, &b, &c, &d);
            seed = a | (b << 8) | (c << 16) | (d << 24);
        }
        seed += (unsigned int)util_time(1000000000);
        srand(seed);
        // Initialize the seed for the hash function
        hash_seed = rand();
    }
    // Init default server list
    if(level == INIT_LEVEL_SETUP) {
        ret |= util_get_servers(TRUE);
    }
    // Get ready to work with threads. This has to be run in
    // the process main thread before util jobs APIs are used.
    if(level == INIT_LEVEL_THREADS) {
        util_init_thrd_key();
        ret |= util_set_main_thrd();
    }
    // Init timers subsystem (Note: requires threads)
    if(level == INIT_LEVEL_TIMERS) {
        ret |= util_timers_init();
    }

    return ret;
}
