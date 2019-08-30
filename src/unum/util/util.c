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

// Terminate agent. Use EXIT_SUCCESS, EXIT_FAILURE or
// add more custom exit codes (see EXIT_RESTART, EXIT_REBOOT).
void util_shutdown(int err_code)
{
    log("Terminating unum %s, exit code %d\n", VERSION, err_code);
    agent_exit(err_code);
}

// Restart agent (the function does not return to the caller)
void util_restart(void)
{
    log("Terminating unum %s, restart requested\n", VERSION);
    if(unum_config.unmonitored || unum_config.mode != NULL) {
        log("The process is not monitored, restart manually\n");
    }
    agent_exit(EXIT_RESTART);
}

// Reboot the device
void util_reboot(void)
{
#ifdef REBOOT_CMD
    int err;
    struct stat st;
    char *reboot_cmd = REBOOT_CMD;

    err = stat(reboot_cmd, &st);
    if(err == 0) {
        log("Reboot using %s...\n", reboot_cmd);
        system(reboot_cmd);
        sleep(5);
    }
#endif // REBOOT_CMD
    log("Reboot using syscall...\n");
    reboot(LINUX_REBOOT_CMD_RESTART);
    sleep(5);
    log("Failed to reboot, restarting the agent...\n");
    util_restart();
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

// Populate auth_info_key Auth Info Key
// For Gl-inet B1300 it is serial number
// For all other devices, auth_info_key is not updated
#ifdef AUTH_INFO_GET_CMD
void util_get_auth_info_key(char *auth_info_key, int max_key_len)
{
    char *get_auth_info_key_cmd = AUTH_INFO_GET_CMD;
    int pstatus;

    if(util_get_cmd_output(get_auth_info_key_cmd, auth_info_key,
                               max_key_len, &pstatus) > 0) {
        if(pstatus == -1) {
            // Some error while getting the auth info key
            // Zero the key
            memset(auth_info_key, 0, max_key_len);
            log("%s: Failed to get the auth info key\n", __func__);
        }
        return;
    }
}
#endif // AUTH_INFO_GET_CMD

// Return uptime in specified fractions of the second (rounded to the low)
unsigned long long util_time(unsigned int fraction)
{
    struct timespec t;
    unsigned long long l;

    clock_gettime(CLOCK_MONOTONIC, &t);
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

// Save data from memory to a file, returns -1 if fails.
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
        // open files    
        FILE *fd1 = fopen(file1, "r");
        FILE *fd2 = fopen(file2, "r");    
        int status = 0;    

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

// In general it is similar to system() call, but tries to kill the
// command being waited for if the timeout (in seconds) is reached
// and does not change any signal handlers.
// If pid_file is not NULL the function tries to store there the
// PID of the running process while it is executing.
int util_system(char *cmd, unsigned int timeout, char *pid_file)
{
    int ret, pid, status;
    unsigned int time_count = 0;
    FILE *f = NULL;

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

    if(pid_file && (f = fopen(pid_file, "w")) != NULL) {
        fprintf(f, "%d", pid);
        fclose(f);
        f = NULL;
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
        if(time_count >= timeout) {
            kill(pid, SIGKILL);
        }
    }

    if(pid_file) {
        unlink(pid_file);
    }

    return -1;
}

// Call an executable in shell and capture its stdout in a buffer.
// Returns: negative if fails to start (see errno for the error code) or
//          the length of the captured info (excluding terminating 0),
//          if the returned value is not negative the status (if not NULL)
//          is filled in with the exit status of the command or
//          -1 if unable to get it.
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
        if(len >= buf_len) {
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

// Configure the agent to the specified operation mode
// opmode - the operation mode string pointer
// Returns: 0 - success, negative - error setting the opmode
// Note: this function is called from command line procesing routines
//       before any init is done (i.e. even the log msgs would go to stdout)
int util_set_opmode(char *opmode)
{
    int new_flags = 0;
    char *new_mode = NULL;

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
#if defined(FEATURE_MANGED_DEVICE)
    if(strcmp(opmode, UNUM_OPMS_MD) == 0) {
        new_flags = UNUM_OPM_MD;
        new_mode = UNUM_OPMS_MD;
    } else
#endif // FEATURE_MANGED_DEVICE
#if defined(FEATURE_MESH_11S)
# if !defined(FEATURE_LAN_ONLY)
    if(strcmp(opmode, UNUM_OPMS_MESH_11S_GW) == 0) {
        new_flags = UNUM_OPM_GW | UNUM_OPM_MESH_11S;
        new_mode = UNUM_OPMS_MESH_11S_GW;
    } else
# endif // !FEATURE_LAN_ONLY
    if(strcmp(opmode, UNUM_OPMS_MESH_11S_AP) == 0) {
        new_flags = UNUM_OPM_AP | UNUM_OPM_MESH_11S;
        new_mode = UNUM_OPMS_MESH_11S_AP;
    } else
#endif // FEATURE_MESH_11S
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
