// (c) 2017-2020 minim.co
// unum device configuration management code

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// URL to post and get router config, parameters:
// - the URL prefix
// - MAC addr (in xx:xx:xx:xx:xx:xx format)
#define CFG_PATH "/v3/unums/%s/router_configs/raw"


// UID of the last config that has been successfully
// sent to the server (zeroed at bootup).
CONFIG_UID_t last_sent_uid;
// UID of the new/updated config that has been retrieved and
// is in process of being sent to the server.
CONFIG_UID_t new_uid;

// Set if cloud pull_router_config command is received
static UTIL_EVENT_t download_cfg = UTIL_EVENT_INITIALIZER;

// Session number for config tracing
static char session_num_chr = '0' - 1;

// Sequence number for config tracing
static int sequence_num = 0;


// Determines if tracing is "on" or "off" basing on the
// unum_config.cfg_trace and the presence of the tracing folder
// it poins to. Handles swinching between "on" and "off" state
// (basing on the folder presence) while running.
// Sets session_num_chr and resets sequence_num to 0 when
// transitioning from "off" to "on".
// Retunrs: TRUE/FALSE
static int is_cfg_tracing(void)
{
    static int current_status = FALSE;

    int new_status = unum_config.cfg_trace != NULL &&
                     util_path_exists(unum_config.cfg_trace);
    // No changes
    if(!(new_status ^ current_status))
    {
        return new_status;
    }

    // Status has changed
    current_status = new_status;

    // Turning it off - nothing to do
    if(!new_status) {
        log("%s: stopping tracing session: %c\n", __func__, session_num_chr);
        return new_status;
    }

    // Turning it on - start new session and reset sequence number
    char status_file_pname[256];
    snprintf(status_file_pname, sizeof(status_file_pname),
             "%s/status", unum_config.cfg_trace);
    if(util_file_to_buf(status_file_pname, &session_num_chr, 1) <= 0 ||
       ++session_num_chr > '9' || session_num_chr < '0')
    {
        session_num_chr = '0';
    }
    log("%s: starting tracing session: %c\n", __func__, session_num_chr);
    util_buf_to_file(status_file_pname, &session_num_chr, 1, 00666);
    sequence_num = 0;

    return new_status;
}

#ifdef CONFIG_DOWNLOAD_IN_AGENT
// Called by command processing job when pull_router_config command is received
void config_download_req(void)
{
    UTIL_EVENT_SETALL(&download_cfg);
}

// Processing function for loading and applying device configuration.
// The parameter is the command name, returns 0 if successful
int cmd_update_config(char *cmd, char *s_unused, int len_unused)
{
    log("%s: requesting config download...\n", __func__);
    config_download_req();
    return 0;
}
#endif // CONFIG_DOWNLOAD_IN_AGENT


static void config(THRD_PARAM_t *p)
{
    http_rsp *rsp = NULL;
    char url[256];
    char *my_mac = util_device_mac();

    log("%s: started\n", __func__);

    log("%s: waiting for activate to complete\n", __func__);
    wait_for_activate();
    log("%s: done waiting for activate\n", __func__);

    // Check that we have MAC address
    if(!my_mac) {
        log("%s: cannot get device MAC\n", __func__);
        return;
    }

#ifdef DELAY_CONFIG_SEND_ON_STARTUP
    // Wait short random time to avoid all the jobs pending on
    // activate kick in at the same time
    sleep(rand() % CONFIG_PERIOD);
#endif // DELAY_CONFIG_SEND_ON_STARTUP

#ifdef FEATURE_ACTIVATE_SCRIPT
    // Delay config subsystem till intial provisioning of the device
    // from the cloud is complete.
#if !defined(ACTIVATE_FLAG_FILE) || !defined(ACTIVATE_FLAG_FILE_TIMEOUT)
#  error Platform must define ACTIVATE_FLAG_FILE and ACTIVATE_FLAG_FILE_TIMEOUT
#endif // !ACTIVATE_FLAG_FILE || !ACTIVATE_FLAG_FILE_TIMEOUT
    log("%s: waiting for config provisioning up to %dsec\n",
        __func__, ACTIVATE_FLAG_FILE_TIMEOUT);
    int time_count;
    for(time_count = 0;
        time_count < ACTIVATE_FLAG_FILE_TIMEOUT;
        ++time_count, sleep(1))
    {
        struct stat st;
        if(stat(ACTIVATE_FLAG_FILE, &st) == 0) {
            log("%s: config provisioning is complete\n", __func__);
            break;
        }
    }
    if(time_count >= ACTIVATE_FLAG_FILE_TIMEOUT) {
        log("%s: timed out waiting for config provisioning\n", __func__);
    }
#endif // FEATURE_ACTIVATE_SCRIPT

    util_wd_set_timeout(HTTP_REQ_MAX_TIME + CONFIG_PERIOD);

    // Prepare the URL string
    util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API, url, sizeof(url),
                   CFG_PATH, my_mac);

    for(;;) {
        char *cstr = NULL;
        int cstr_len = 0;
        unsigned long delay;
        char trace_file_pname[256];

        for(;;) {

            // Prepare the config data JSON, if config is unchanged
            // or cannnot be retrieved at this time then NULL is returned.
            cstr = platform_cfg_get_if_changed(&last_sent_uid, &new_uid, &cstr_len);
            if(!cstr) {
                break;
            }

            log("%s: config has changed, sending...\n", __func__);

            if(is_cfg_tracing())
            {
                snprintf(trace_file_pname, sizeof(trace_file_pname),
                         "%s/s%c_%02d.bin", unum_config.cfg_trace,
                         session_num_chr, sequence_num);
                sequence_num = (sequence_num + 1) % 100;
                if(util_buf_to_file(trace_file_pname, cstr, cstr_len, 00666) < 0)
                {
                    log("%s: failed to store sent config %s\n",
                        __func__, trace_file_pname);
                } else {
                    log("%s: stored sent config %s\n",
                        __func__, trace_file_pname);
                }
            }

            // Send the config info
            rsp = http_post(url,
                            "Content-Type: application/octet-stream\0"
                            "Accept: application/json\0",
                            cstr, cstr_len);

            // No longer need the JSON string
            platform_cfg_free(cstr);
            cstr = NULL;
            cstr_len = 0;

            if(rsp == NULL || (rsp->code / 100) != 2) {
                log("%s: request error, code %d%s\n",
                    __func__, rsp ? rsp->code : 0, rsp ? "" : "(none)");
                break;
            }

            // Update the UID of the successfuly uploaded config
            memcpy(&last_sent_uid, &new_uid, sizeof(last_sent_uid));

            log("%s: sent successfully\n", __func__);
            break;
        }

        if(cstr) {
            platform_cfg_free(cstr);
            cstr = NULL;
            cstr_len = 0;
        }

        if(rsp) {
            free_rsp(rsp);
            rsp = NULL;
        }

        util_wd_poll();

        // Wait for timeout or config download request
        delay = util_time(1);
        if(UTIL_EVENT_TIMEDWAIT(&download_cfg, CONFIG_PERIOD * 1000) != 0)
        {
            // Timeout, continue to the config changes check loop
            continue;
        }
        delay = util_time(1) - delay;
        delay = (delay > CONFIG_PERIOD) ? 0 : (CONFIG_PERIOD - delay);

#ifdef CONFIG_DOWNLOAD_IN_AGENT
        log("%s: request to download config from cloud\n", __func__);
        UTIL_EVENT_RESET(&download_cfg);

        for(;;) {
            // Retrieve the new config
            rsp = http_get(url, NULL);

            // If failed to download, set the request event to try again later
            if(!rsp || rsp->len <= 0 || (rsp->code / 100) != 2) {
                log("%s: failed to download config, will retry\n", __func__);
                config_download_req();
                break;
            }
            log("%s: downloaded config from cloud\n", __func__);

            if(is_cfg_tracing())
            {
                snprintf(trace_file_pname, sizeof(trace_file_pname),
                         "%s/r%c_%02d.bin", unum_config.cfg_trace,
                         session_num_chr, sequence_num);
                sequence_num = (sequence_num + 1) % 100;
                if(util_buf_to_file(trace_file_pname,
                                    rsp->data, rsp->len, 00666) < 0)
                {
                    log("%s: failed to store received config %s\n",
                        __func__, trace_file_pname);
                } else {
                    log("%s: stored received config %s\n",
                        __func__, trace_file_pname);
                }
            }

            // Try to apply the config
            if(platform_apply_cloud_cfg(rsp->data, rsp->len) != 0) {
                log("%s: unable to apply cloud config\n", __func__);
            }

            // If we tried to apply the config regardless of the result
            // force the agent to resend it without any delay.
            memset(&last_sent_uid, 0, sizeof(last_sent_uid));
            delay = 0;
            
            break;
        }

        if(rsp) {
            free_rsp(rsp);
            rsp = NULL;
        }
#endif // CONFIG_DOWNLOAD_IN_AGENT

        // Continue after sleeping for the remaining wait time
        sleep(delay);
    }

    // Never reaches here
    log("%s: done\n", __func__);
}

// Subsystem init fuction
int config_init(int level)
{
    int ret = 0;
    if(level == INIT_LEVEL_CONFIG) {
        // Do the platform specific init
        if(platform_cfg_init() != 0) {
            return -1;
        }
        // Start the config management job
        ret = util_start_thrd("config", config, NULL, NULL);
    }
    return ret;
}
