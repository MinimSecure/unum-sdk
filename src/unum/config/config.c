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
#define CFG_URL "%s/v3/unums/%s/router_configs"
#define CFG_URL_HOST "https://api.minim.co"

// UID of the last config that has been successfully
// sent to the server (zeroed at bootup).
CONFIG_UID_t last_sent_uid;
// UID of the new/updated config that has been retrieved and
// is in process of being sent to the server.
CONFIG_UID_t new_uid;

// Set if cloud pull_router_config command is received
static UTIL_EVENT_t download_cfg = UTIL_EVENT_INITIALIZER;


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

// Prepare the config data wraped in JSON, if config is unchanged
// or cannnot be retrieved at this time then NULL is returned.
static char *config_json()
{
    char *config_str;
    char *json_str;

    config_str = platform_cfg_get_if_changed(&last_sent_uid, &new_uid, NULL);

    // Can't get it or no changes
    if(!config_str) {
        return NULL;
    }

    JSON_OBJ_TPL_t tpl = {
      { "raw_config",  { .type = JSON_VAL_STR, { .s = config_str }}},
      { NULL }
    };

    json_str = util_tpl_to_json_str(tpl);

    if(config_str) {
        platform_cfg_free(config_str);
        config_str = NULL;
    }

    return json_str;
}

static void config(THRD_PARAM_t *p)
{
    http_rsp *rsp = NULL;

    log("%s: started\n", __func__);

    log("%s: waiting for activate to complete\n", __func__);
    wait_for_activate();
    log("%s: done waiting for activate\n", __func__);

#ifdef DELAY_CONFIG_SEND_ON_STARTUP
    // Wait short random time to avoid all the jobs pending on
    // activate kick in at the same time
    sleep(rand() % CONFIG_PERIOD);
#endif // DELAY_CONFIG_SEND_ON_STARTUP

    util_wd_set_timeout(HTTP_REQ_MAX_TIME + CONFIG_PERIOD);

    for(;;) {
        char *jstr = NULL;
        char *my_mac = util_device_mac();
        unsigned long delay;
        char url[256];

        for(;;) {

            // Check that we have MAC address
            if(!my_mac) {
                log("%s: cannot get device MAC\n", __func__);
                break;
            }

            // Prepare the URL string
            snprintf(url, sizeof(url), CFG_URL,
                     (unum_config.url_prefix ?
                       unum_config.url_prefix : CFG_URL_HOST),
                     my_mac);

            // Prepare the config data JSON, if config is unchanged
            // or cannnot be retrieved at this time then NULL is returned.
            jstr = config_json(&last_sent_uid, &new_uid);
            if(!jstr) {
                break;
            }

            log("%s: config has changed, sending...\n", __func__);

            // Send the config info
            rsp = http_post(url,
                            "Content-Type: application/json\0"
                            "Accept: application/json\0",
                            jstr, strlen(jstr));

            // No longer need the JSON string
            util_free_json_str(jstr);
            jstr = NULL;

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

        if(jstr) {
            util_free_json_str(jstr);
            jstr = NULL;
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
            if(!rsp || rsp->len <= 0) {
                log("%s: failed to download config, will retry\n", __func__);
                config_download_req();
                break;
            }
            log("%s: downloaded config from cloud\n", __func__);

            // If apply fails give up and force AP to re-send current config
            if(platform_apply_cloud_cfg(rsp->data, rsp->len) != 0) {
                log("%s: unable to apply cloud config\n", __func__);
                memset(&last_sent_uid, 0, sizeof(last_sent_uid));
                delay = 0;
                break;
            }

            log("%s: cloud config has been applied successfully\n", __func__);
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
