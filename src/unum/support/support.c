// (c) 2017-2018 minim.co
// unum firmware update code

#include "unum.h"

// This callback keeps checking for the SUPPORT_CONNECT_FILE_NAME while
// the session is running. If SUPPORT_CONNECT_FILE_NAME is detected it
// kills the current (probably stalled) sesion so the new one can be
// started.
// It also watches for the session total time exceeding
// SUPPORT_CONNECT_TIME_MAX and kills it as well if it happens.
static int support_cmd_cb(int pid, unsigned int elapsed, void *params)
{
    char *pid_file = (char *)params;
    FILE *f = NULL;
    
    // Handle the first call after creating the process
    if(elapsed == 0) {
        // Make it have its own process group
        setpgid(pid, 0);
        // Save PID
        if(pid_file && (f = fopen(pid_file, "w")) != NULL)
        {
            fprintf(f, "%d", pid);
            fclose(f);
            f = NULL;
        }
        // Call us again in SUPPORT_SHORT_PERIOD seconds
        return SUPPORT_SHORT_PERIOD;
    }

    // If session max time is reached, kill
    if(elapsed >= SUPPORT_CONNECT_TIME_MAX) {
        log("%s: session expired, killing PID %d\n", __func__, pid);
        kill(-pid, SIGKILL);
        return -1;
    }

    // If another request to start session is detected
    if(util_path_exists(SUPPORT_CONNECT_FILE_NAME)) {
        log("%s: new session rquest, killing PID %d\n", __func__, pid);
        kill(-pid, SIGKILL);
        return -1;
    }

    // Call us again in SUPPORT_SHORT_PERIOD seconds
    return SUPPORT_SHORT_PERIOD;
}

// Man loop for support process
static void support(void)
{
    char *cmd = NULL;
    unsigned int delay = 0;
    unsigned int short_delay_count = 0;
    char pid_file[256] = "";
    char *pid_fprefix = unum_config.pid_fprefix;

    if(!pid_fprefix) {
        pid_fprefix = AGENT_PID_FILE_PREFIX;
    }
    snprintf(pid_file, sizeof(pid_file), "%s-%s.pid",
             pid_fprefix, SUPPORT_PID_FILE_SFX);

    log("%s: started\n", __func__);

    // Clean up stale PID file for the SSH session
    unlink(pid_file);

    delay = SUPPORT_STARTUP_DELAY_FIXED;

#if SUPPORT_STARTUP_DELAY_RANDOM > 0
    // Random delay at startup to avoid all devices trying to connect
    // at the same time after large area power outage.
    delay += rand() % SUPPORT_STARTUP_DELAY_RANDOM;
#endif // SUPPORT_STARTUP_DELAY_RANDOM > 0

#if (SUPPORT_STARTUP_DELAY_FIXED + SUPPORT_STARTUP_DELAY_RANDOM) > 0
    // After the startup delay try to connect a few times with a short delay.
    short_delay_count = SUPPORT_SHORT_PERIOD_TRIES;
#endif // (SUPPORT_STARTUP_DELAY_FIXED + SUPPORT_STARTUP_DELAY_RANDOM) > 0

    log("%s: initial delay %d sec, short retries %d\n",
        __func__, delay, short_delay_count);

    for(;;) {
        int ii, oo;
        unsigned long con_time;
        char *my_mac = util_device_mac();
        char username[16]; // macXXXXXXXXXXXX
        char lan_ip[INET_ADDRSTRLEN];
        int cmd_max_len = SUPPORT_CONNECT_CMD_MAX_LEN;

        int do_not_try = (unum_config.support_long_period == 0) &&
                         (short_delay_count <= 0);

        // Wait for the pre-calculated delay, but keep checking for
        // the connection request file to terminate the loop earlier.
        for(ii = 0; do_not_try || ii < delay; ii++) {
            if(util_path_exists(SUPPORT_CONNECT_FILE_NAME)) {
                log("%s: connect request detected, terminating wait\n",
                    __func__);
                break;
            }
            sleep(1);
        }

        // If detected conect request set the count and clear the file
        // Note: we might skip the delay loop, so have to do it outside
        if(util_path_exists(SUPPORT_CONNECT_FILE_NAME)) {
            short_delay_count = SUPPORT_SHORT_PERIOD_TRIES;
            unlink(SUPPORT_CONNECT_FILE_NAME);
        }

        // Decrement short delay count
        if(short_delay_count > 0) {
            --short_delay_count;
        }

        // Make a connection attempt
        con_time = util_time(1);
        for(;;) {
            // Allocate memory for the suport command (it could be very long)
            // unless already allocated. The memory is never freed.
            if(!cmd) {
                cmd = malloc(cmd_max_len);
                if(!cmd) {
                    log("%s: cannot allocate memory\n", __func__);
                    break;
                }
            }

            // Check that we have MAC address
            if(!my_mac) {
                log("%s: cannot get device MAC\n", __func__);
                break;
            }

            // Build username
            memset(username, 0, sizeof(username));
            strcpy(username, "mac");
            ii = 0, oo = 3;
            while(oo < sizeof(username) && my_mac[ii]) {
                if(my_mac[ii] == ':') {
                    ++ii;
                    continue;
                }
                username[oo++] = my_mac[ii++];
            }

            // Get the main LAN interface IP
            if(util_get_ipv4(GET_MAIN_LAN_NET_DEV(), lan_ip) != 0) {
                strcpy(lan_ip, "127.0.0.1");
            }

            log("%s: attempting to make a connection\n", __func__);

            // By default use hostname, but fall back to IP if DNS
            // failure is detected by the agent.
            char *host = SUPPORT_PORTAL_HOSTNAME;
            if(conncheck_no_dns()) {
                host = SUPPORT_PORTAL_FB_IP;
            }

            // Prepare and execute the command
            snprintf(cmd, cmd_max_len, SUPPORT_CONNECT_CMD,
                     SUPPORT_CONNECT_CMD_PARAMS);

            log("%s: command\n%s\n", __func__, cmd);

            util_system_wcb(cmd, 0, support_cmd_cb, (void *)pid_file);
            unlink(pid_file);
            break;
        }
        con_time = util_time(1) - con_time;

        // Log the connection time (only if longer than the short delay,
        // otherwise the info is not useful).
        if(con_time > SUPPORT_SHORT_PERIOD) {
            log("%s: conection took %d sec\n", __func__, con_time);
        }

        // Use short delay if the fast tries counter is above 0
        if(short_delay_count > 0) {
            delay = SUPPORT_SHORT_PERIOD;
        }
        else if(unum_config.support_long_period > 0) {
            delay = unum_config.support_long_period;
        }
        else {
            delay = 0;
            continue;
        }

        // Adjust for the connection time
        if(delay > con_time) {
            delay -= con_time;
        } else {
            delay = 0;
        }

        log("%s: next try in %d sec, short retries left %d\n",
            __func__, delay, short_delay_count);
    }

    log("%s: done\n", __func__);
}

// Firmware update process main function
// Note: currently we only need logging and rand init for support, but
//       MAX_INIT_LEVEL_SUPPORT is set to go up to HTTP for the future use.
int support_main(void)
{
    int level, ii, err;
    INIT_FUNC_t init_fptr;

    // Redirect all output to fw updater log and disable all other logs
    // besides stdout.
    // Note: fw_update log is not using mutex since we run in single thread
    unsigned long mask = ~(
      (1 << LOG_DST_SUPPORT) |
      (1 << LOG_DST_STDOUT)
    );
    set_disabled_log_dst_mask(mask);
    set_proc_log_dst(LOG_DST_SUPPORT);

    // Make sure we are the only instance and create the PID file
    err = unum_handle_start("support");
    if(err != 0) {
        log("%s: startup failure, terminating.\n", __func__);
        return -1;
    }

    // Go through the init levels up to MAX_INIT_LEVEL_FW_UPDATE
    for(level = 1; level <= MAX_INIT_LEVEL_SUPPORT; level++)
    {
        log_dbg("%s: Init level %d...\n", __func__, level);
        for(ii = 0; init_list[ii] != NULL; ii++) {
            init_fptr = init_list[ii];
            err = init_fptr(level);
            if(err != 0) {
            	log("%s: Error at init level %d in %s(), terminating.\n",
                    __func__, level, init_str_list[ii]);
                util_restart(UNUM_START_REASON_SUPPORT_FAIL);
                // should never reach this point
            }
        }
    }
    log_dbg("%s: Init done\n", __func__);

    // Start the support portal agent loop
    support();

    // In case we ever need to stop it gracefully
    unum_handle_stop();
    return 0;
}

#ifdef SUPPORT_RUN_MODE
// Create the support flag file
void create_support_flag_file(void)
{
#ifdef ENABLE_DROPBEAR_CMD
    if(util_path_exists(ENABLE_DROPBEAR_CMD)) {
        util_system(ENABLE_DROPBEAR_CMD, ENABLE_DROPBEAR_CMD_TIMEOUT, NULL);
    }
#endif // ENABLE_DROPBEAR_CMD
    if(touch_file(SUPPORT_CONNECT_FILE_NAME, 0660) < 0)
    {
        log("%s: Error while creating support filename: %s\n",
                            __func__, SUPPORT_CONNECT_FILE_NAME);
        return;
    }
}
#endif // SUPPORT_RUN_MODE
