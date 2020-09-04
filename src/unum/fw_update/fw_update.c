// (c) 2017-2019 minim.co
// unum firmware update code

#include "unum.h"

// URL to pull in the active firmware version, parameters:
// - URL prefix
// - MAC addr (in xx:xx:xx:xx:xx:xx format)
// - device product name string
// The suffix define allows LEDE builds to request sysupgrade
// image by adding "?image=sysupgrade"
#define FIRMWARE_RELEASE_INFO_PATH \
    "/release_server/releases/%s/active/%s_firmware" \
    FIRMWARE_RELEASE_INFO_URL_SUFFIX

// RAM FS flag to indicate whether firmware upgrade is forced
#define FORCE_FW_UPDATE_FLAG_FILE "/tmp/unum_force_fw_update.cmd"

// How often to check for forced upgrade while waiting for the
// upgrade check period (in seconds).
#define FORCE_FW_UPDATE_CHECK_PERIOD 10

// Called from command processor thread
// Set the "Force Firmware Upgrade" flag 
void cmd_force_fw_update(void)
{
    int fd = creat(FORCE_FW_UPDATE_FLAG_FILE, 0666);

    if (fd == -1) {
        log("%s: Error while cretating force firmware upgrade flag file %s",
                __func__, strerror(errno));
         return;
    }
    close(fd);
}

// Is the "firmware upgrade" command received from server?
static int is_force_fw_update_set(void)
{
    if(access(FORCE_FW_UPDATE_FLAG_FILE, F_OK) != -1) {
        return TRUE;
    } else {
        return FALSE;
    }
}

// Clear the "Force Firmware Upgrade" flag
static void force_fw_update_clear(void)
{
    unlink(FORCE_FW_UPDATE_FLAG_FILE);
}

static void fw_update(void)
{
    unsigned long no_rsp_t = 0;
    unsigned int delay;
    char *cur_fw_ver = util_fw_version();
    char *sptr;
    int development = FALSE;
    http_rsp *rsp = NULL;
    char *my_mac = util_device_mac();
    char url[256];

    log("%s: started\n", __func__);

    // Check that we have MAC address
    if(!my_mac) {
        log("%s: cannot get device MAC\n", __func__);
        return;
    }

    // Random delay at startup to avoid all APs poll server
    // at the same time after large area power outage.
    delay = rand() % FW_UPDATE_CHECK_PERIOD;
    log("%s: delaying updater startup for %d sec\n", __func__, delay);
    sleep(delay);

    sptr = strrchr((const char *)cur_fw_ver, '.');
    if(sptr != NULL && strcmp(sptr, ".0") == 0) {
        development = TRUE;
    }

    // Get TXT Record from DNS to determine which servers to connect to
    for(;;) {
        util_wd_set_timeout(unum_config.dns_timeout * util_num_oob_dns() * 2);
        if(util_get_servers(FALSE) == 0) {
            util_wd_set_timeout(0);
            break;
        }
        util_wd_set_timeout(0);
        log("%s: Cannot retrieve server endpoints\n", __func__);
        // Wait before retrying
        sleep(FORCE_FW_UPDATE_CHECK_PERIOD);
        continue;
    }

    // Build URL to Send
    util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_RELEASES, url,
                   sizeof(url), FIRMWARE_RELEASE_INFO_PATH, my_mac,
                   DEVICE_PRODUCT_NAME);

    // Get s3.amazonaws.com address from public DNS to use in case the
    // normal name resolution fails.
    char *fw_storage_host = "s3.amazonaws.com";
    char fw_storage_addr[INET_ADDRSTRLEN];
    int have_fw_storage_addr = FALSE;

    log("%s: getting address for %s\n", __func__, fw_storage_host);
    util_wd_set_timeout(unum_config.dns_timeout * util_num_oob_dns() * 2);
    if(util_nodns_get_ipv4(fw_storage_host, fw_storage_addr) != 0) {
        log("%s: error, proceeding without backup address for %s\n",
            __func__, fw_storage_host);
    } else {
        log("%s: using backup address %s for %s\n",
            __func__, fw_storage_addr, fw_storage_host);
        char str[256];
        snprintf(str, sizeof(str), "%s:443:%s",
                 fw_storage_host, fw_storage_addr);
        if(util_add_nodns_entry(str) < 0) {
            log("%s: error adding entry to DNS fallback list\n", __func__);
        } else {
            have_fw_storage_addr = TRUE;
        }
    }
    util_wd_set_timeout(0);

    log("%s: starting firmware updater, check period %d\n", __func__,
        unum_config.fw_update_check_period);

    // Forever loop for firmware upgrade.
    // Set delay to 0 as we enter the loop the first time, after that
    // reset it to unum_config.fw_update_check_period on every cycle.
    int force_upgrade = FALSE;
    int force_upgrade_try = 0;
    for(delay = 0;; delay = unum_config.fw_update_check_period)
    {
        // Count forced upgrade retries
        if(force_upgrade) {
            ++force_upgrade_try;
        } else {
            force_upgrade_try = 0;
        }

        // Check if it is time to stop trying to do the forced fw upgrades
        if(force_upgrade_try > FORCE_FW_UPDATE_RETRIES) {
            force_fw_update_clear();
            force_upgrade_try = 0;
        } else if(force_upgrade_try > 0) { // retrying forced upgrade
            delay = delay - FORCE_FW_UPDATE_CHECK_PERIOD;
            sleep(FORCE_FW_UPDATE_CHECK_PERIOD);
        }

        // Wait till we have to check for upgrade or till it is forced
        while(!(force_upgrade = is_force_fw_update_set()) && delay > 0) {
            delay = delay - FORCE_FW_UPDATE_CHECK_PERIOD;
            sleep(FORCE_FW_UPDATE_CHECK_PERIOD);
        }

        // We are here means that either the upgrade is forced or it is
        // just time to check for the upgrade availability.
        // Before proceeding we have to make sure the existing firmware is
        // not of the development version (unless the upgrade is forced).
        if(development && !force_upgrade) {
            log("%s: skipping firmware check for dev version %s\n",
                __func__, cur_fw_ver);
            // Start over the wait
            continue;
        }

        // Check for upgrade
        for(;;)
        {
            char *download_url;
            char *new_fw_ver;
            int err;

            log("%s: Attempting%s upgrade now\n",
                __func__, (force_upgrade ? " forced" : ""));

            // Retrieve the active firmware info for the device
            rsp = http_get(url, NULL);
            if(rsp == NULL)
            {
                // If no response for over UPDATER_OFFLINE_RESTART time period
                // try to restart updater process
                if(no_rsp_t == 0) {
                    no_rsp_t = util_time(1);
                } else if(util_time(1) - no_rsp_t > UPDATER_OFFLINE_RESTART) {
                    log("%s: no response for more than %d sec, restarting...\n",
                        __func__, UPDATER_OFFLINE_RESTART);
                    // In updater mode the agent runs under monitor and
                    // util_restart() works
                    util_restart(UNUM_START_REASON_FW_CONN_FAIL);
                }
                break;
            }
            no_rsp_t = 0;

            if((rsp->code / 100) != 2) {
                log("%s: failed to retrieve active firmware info for %s %s\n",
                    __func__, DEVICE_PRODUCT_NAME, my_mac);
                break;
            }

            // Just in case, try to fix line endings. The response data
            // guaranteed to be null-terminated.
            if(rsp->len > 0) {
                util_fix_crlf(rsp->data);
            }

            // The firmware info is 2 lines, the first is the version
            // prefixed with the letter 'v', the second is the download URL.
            sptr = strchr(rsp->data, '\n');
            if(!sptr) {
                log("%s: firmware info is not split into lines\n", __func__);
                break;
            }
            *sptr++ = 0;
            download_url = sptr;
            if(*(rsp->data) == 'v') {
                new_fw_ver = rsp->data + 1;
            } else {
                new_fw_ver = rsp->data;
            }
            // Check the new version
            if(strcmp("UNKNOWN", new_fw_ver) == 0) {
                log("%s: firmware info is not yet availble for the device\n",
                    __func__);
                break;
            }
            // Check the download URL
            if(strlen(download_url) < 8 ||
               toupper(download_url[0]) != 'H' || toupper(download_url[1]) != 'T' ||
               toupper(download_url[2]) != 'T' || toupper(download_url[3]) != 'P')
            {
                log("%s: bad firmware URL format or not HTTP\n", __func__);
                break;
            }
            // chop the newline at the end ot the URL (if present)
            sptr = strchr(download_url, '\n');
            if(sptr) {
                *sptr = 0;
            }
            // Compare the current and the new firmware versions
            if(strcmp(new_fw_ver, cur_fw_ver) == 0) {
                // Firmware versions match, nothing to do
                break;
            }

            // Download the new firmware
            log("%s: downloading new firmware %s from <%s>\n",
                __func__, new_fw_ver, download_url);
            if(http_get_file(download_url, NULL, UPGRADE_FILE_NAME) != 0) {
                log("%s: failed to download the new firmware\n", __func__);
                // If we do not have backup address for the firmware storage
                // host and there is an ongoing DNS outage, restart to try
                // retrieving it again.
                if(!have_fw_storage_addr && conncheck_no_dns())
                {
                    log("%s: no IP for %s during DNS outage, restarting...\n",
                        __func__);
                    // In updater mode the agent runs under monitor and
                    // util_restart() works
                    util_restart(UNUM_START_REASON_FW_CONN_FAIL);
                }
                break;
            }

            log("%s: downloaded firmware to %s\n", __func__, UPGRADE_FILE_NAME);
#ifdef UPGRADE_CMD
            err = system(UPGRADE_CMD);
#else  // UPGRADE_CMD
#  error The device requires custom upgrade procedure!
#endif // UPGRADE_CMD
            log("%s: upgrade cmd <%s> returned (%d)\n",
                __func__, UPGRADE_CMD, err);

#ifdef UPGRADE_GRACE_PERIOD
            // Wait for the platform's grace period (if any)
            log("%s: update grace period is %dsec, waiting...\n",
                __func__, UPGRADE_GRACE_PERIOD);
            sleep(UPGRADE_GRACE_PERIOD);
#endif // UPGRADE_GRACE_PERIOD

            // If we have not rebooted still, remove the file
            unlink(UPGRADE_FILE_NAME);

            break;
        }

        if(rsp) {
            free_rsp(rsp);
            rsp = NULL;
        }
    }

    log("%s: done\n", __func__);
}

// Firmware update process main function
int fw_update_main(void)
{
    int level, ii, err;
    INIT_FUNC_t init_fptr;

    // Redirect all output to fw updater log and disable all other logs
    // besides stdout.
    // Note: fw_update log is not using mutex since we run in single thread
    unsigned long mask = ~(
      (1 << LOG_DST_UPDATE) |
      (1 << LOG_DST_STDOUT)
    );
    set_disabled_log_dst_mask(mask);
    set_proc_log_dst(LOG_DST_UPDATE);

    // Make sure we are the only instance and create the PID file
    err = unum_handle_start("updater");
    if(err != 0) {
        log("%s: startup failure, terminating.\n", __func__);
        return -1;
    }

    // Go through the init levels up to MAX_INIT_LEVEL_FW_UPDATE
    for(level = 1; level <= MAX_INIT_LEVEL_FW_UPDATE; level++)
    {
        log_dbg("%s: Init level %d...\n", __func__, level);
        for(ii = 0; init_list[ii] != NULL; ii++) {
            init_fptr = init_list[ii];
            err = init_fptr(level);
            if(err != 0) {
                log("%s: Error at init level %d in %s(), terminating.\n",
                    __func__, level, init_str_list[ii]);
                util_restart(UNUM_START_REASON_FW_START_FAIL);
                // should never reach this point
            }
        }
    }
    log_dbg("%s: Init done\n", __func__);

    // Start the firmware update loop
    fw_update();

    // In case we ever need to stop it gracefully
    unum_handle_stop();
    return 0;
}
