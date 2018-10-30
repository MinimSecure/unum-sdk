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

// unum device activate code

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// URL to send the activation info to, parameters:
// - the URL prefix
// - MAC addr of the router (in xx:xx:xx:xx:xx:xx format)
#define ACTIVATE_URL "%s/v3/unums/%s/activate"
#define ACTIVATE_URL_HOST "https://api.minim.co"

// Set to TRUE once we have activated the Unum
static UTIL_EVENT_t activate_complete = UTIL_EVENT_INITIALIZER;

// This function blocks the caller till device is activated.
void wait_for_activate(void)
{
    // Wait for the event to be set otherwise
    UTIL_EVENT_WAIT(&activate_complete);
}

// This function returns TRUE if the agent is activated, FALSE otherwise.
int is_agent_activated(void)
{
    return UTIL_EVENT_IS_SET(&activate_complete);
}

// Set the activate event (for tests to bypass the activate step)
void set_activate_event(void)
{
    UTIL_EVENT_SETALL(&activate_complete);
}

// Get uptime to report in activate
static int uptime_for_activate(char *key)
{
    return util_time(1);
}

// Collects router activation data and allocates JSON string
// with the activate request payload.
static char *router_activate_json(char *my_mac)
{
    char *hardware_type = DEVICE_PRODUCT_NAME;
    char *fw_ver = util_fw_version();
    char *agent_ver = VERSION;
    char *start_reason = NULL;

    char crash_url[256];
    char *crash_url_ptr = NULL;
    char *crash_type_ptr = NULL;
    char *crash_location_ptr = NULL;
    char *crash_msg_ptr = NULL;
    JSON_KEYVAL_TPL_t *crashinfo_tpl_ptr = NULL;
    int add_crash_info = FALSE;

    if(process_start_reason.code == UNUM_START_REASON_UNKNOWN) {
        start_reason = "normal_start";
    } else if(process_start_reason.code == UNUM_START_REASON_RESTART) {
        start_reason = "self_restart";
    } else if(process_start_reason.code == UNUM_START_REASON_CRASH) {
        start_reason = "exception";
        if(process_start_reason.ci != NULL) {
            TRACER_CRASH_INFO_t *ci = process_start_reason.ci;
            add_crash_info = TRUE;
            crash_type_ptr = ci->type;
            crash_location_ptr = ci->location;
            crash_msg_ptr = ci->msg;
            // We have to complete building the URL by supplying the device MAC
            if(ci->dumpurl[0] != 0) {
                snprintf(crash_url, sizeof(crash_url), ci->dumpurl, my_mac);
                crash_url_ptr = crash_url;
            }
        }
    } else {
        start_reason = NULL;
    }

    JSON_OBJ_TPL_t tpl_tbl_crash_info = {
      { "type",       {.type = JSON_VAL_STR, { .s = crash_type_ptr }}},
      { "location",   {.type = JSON_VAL_STR, { .s = crash_location_ptr }}},
      { "description",{.type = JSON_VAL_STR, { .s = crash_msg_ptr }}},
      { "report_url", {.type = JSON_VAL_STR, { .s = crash_url_ptr }}},
      { NULL }
    };

    if(add_crash_info) {
        crashinfo_tpl_ptr = tpl_tbl_crash_info;
    }

    JSON_OBJ_TPL_t tpl = {
      { "hardware_type",   {.type = JSON_VAL_STR, {.s = hardware_type}}},
      { "uptime",          {.type = JSON_VAL_FINT,{.fi = uptime_for_activate}}},
      { "firmware_version",{.type = JSON_VAL_STR, {.s = fw_ver}}},
      { "unum_agent_version",{.type = JSON_VAL_STR, {.s = agent_ver}}},
      { "start_reason",    {.type = JSON_VAL_STR, {.s = start_reason}}},
      { "crash_info",      {.type = JSON_VAL_OBJ, {.o = crashinfo_tpl_ptr}}},
#ifdef DEVELOPER_BUILD
      { "developer_build", {.type = JSON_VAL_INT, {.i = 1}}},
#endif // DEVELOPER_BUILD
      { NULL }
    };

    return util_tpl_to_json_str(tpl);
}

static void activate(THRD_PARAM_t *p)
{
    unsigned long no_rsp_t = 0;
    unsigned int delay = 0;
    int err;
    http_rsp *rsp = NULL;

    log("%s: started\n", __func__);

    log("%s: waiting for conncheck to complete\n", __func__);
    wait_for_conncheck();
    log("%s: done waiting for conncheck\n", __func__);

    log("%s: waiting for provision to complete\n", __func__);
    wait_for_provision();
    log("%s: done waiting for provision\n", __func__);

#if ACTIVATE_PERIOD_START > 0
    // Random delay at startup to avoid all APs poll server
    // at the same time after large area power outage.
    delay = rand() % ACTIVATE_PERIOD_START;
    log("%s: delaying first attempt to %d sec\n", __func__, delay);
    sleep(delay);
#endif // ACTIVATE_PERIOD_START > 0

    util_wd_set_timeout(HTTP_REQ_MAX_TIME + ACTIVATE_MAX_PERIOD);

    for(;;) {
        err = -1;
        for(;;) {
            char *my_mac = util_device_mac();
            char *jstr = NULL;
            char url[256];

            for(;;) {
                // Check that we have MAC address
                if(!my_mac) {
                    log("%s: cannot get device MAC\n", __func__);
                    break;
                }

                jstr = router_activate_json(my_mac);
                if(!jstr) {
                    log("%s: JSON encode failed\n", __func__);
                    break;
                }

                // Activate us with the server
                snprintf(url, sizeof(url), ACTIVATE_URL,
                         (unum_config.url_prefix ?
                           unum_config.url_prefix : ACTIVATE_URL_HOST),
                         my_mac);

                rsp = http_put(url,
                               "Content-Type: application/json\0"
                               "Accept: application/json\0",
                               jstr, strlen(jstr));

                // No longer need the JSON string
                util_free_json_str(jstr);
                jstr = NULL;

                if(rsp == NULL)
                {
                    log("%s: no response\n", __func__);
                    // If no response for over CONNCHECK_OFFLINE_RESTART sec
                    // try to restart the conncheck (this restarts the agent)
                    if(no_rsp_t == 0) {
                        no_rsp_t = util_time(1);
                    }
                    else if(util_time(1) - no_rsp_t > CONNCHECK_OFFLINE_RESTART)
                    {
                        log("%s: restarting conncheck...\n", __func__);
                        restart_conncheck();
                    }
                    break;
                }
                no_rsp_t = 0;

                if((rsp->code / 100) != 2)
                {
                    log("%s: activation denied, code %d\n",
                        __func__, rsp->code);
                    // If the response code is 400 trigger re-provisioning.
                    if(rsp && rsp->code == 400) {
                        log("%s: restarting device provisioning...\n",
                            __func__);
                        restart_provision();
                    }
                    break;
                }

                // Success
                err = 0;
				
                // if length of response is 0, ignore
                if(rsp->len == 0) {
                    // nothing is sent, ignore silently
                    break;
                }

                // Update config (rsp->data is guaranteed to be NULL-terminated)
                parse_config(TRUE, rsp->data);
                break;
            }

            if(rsp) {
                free_rsp(rsp);
                rsp = NULL;
            }

            if(jstr) {
                UTIL_FREE(jstr);
                jstr = NULL;
            }

            util_wd_poll();

            if(!err) {
                log("%s: successfully activated %s\n",
                    __func__, my_mac);
                UTIL_EVENT_SETALL(&activate_complete);
                break;
            }

            // Calculate delay before next activation attempt
            if(delay < ACTIVATE_MAX_PERIOD) {
                delay += rand() % ACTIVATE_PERIOD_INC;
            }
            if(delay > ACTIVATE_MAX_PERIOD) {
                delay = ACTIVATE_MAX_PERIOD;
            }
            log("%s: error, retrying in %d sec\n", __func__, delay);
            sleep(delay);
        }

        if(err == 0) {
            break;
        }
    }

    log("%s: done\n", __func__);
}

// Subsystem init fuction
int activate_init(int level)
{
    int ret = 0;
    if(level == INIT_LEVEL_ACTIVATE) {
        // Start the activate reporting job
        ret = util_start_thrd("activate", activate, NULL, NULL);
    }
    return ret;
}
