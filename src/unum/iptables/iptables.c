// (c) 2017-2021 minim.co
// unum device iptables telemetry code

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// URL to send the iptables to, parameters:
// - the URL prefix
// - MAC addr (in xx:xx:xx:xx:xx:xx format)
#define TELEMETRY_PATH "/v3/unums/%s/firewall"

static int ipt_seq_num = 0;

// Collects iptables data 
// allocates JSON string with the iptables rules.
static char *iptables_json()
{
    char *ipt_filter = NULL;
    char *ipt_nat = NULL;

    // Collect iptables data
    ipt_collect_data();

    // Get rules as strings
    ipt_filter = ipt_get_rules("filter");
    ipt_nat = ipt_get_rules("nat");

    if (ipt_filter == NULL || ipt_nat == NULL) {
        log("%s: Error while getting iptables rules\n", __func__);
        // Just log the error, but move on
    }

    JSON_OBJ_TPL_t tpl_tbl_ipt_obj = {
      {"filter",     {.type = JSON_VAL_STR, {.s = ipt_filter }}},
      {"nat",        {.type = JSON_VAL_STR, {.s = ipt_nat }}},
      {"seq_num", {.type = JSON_VAL_PINT,   {.pi = &ipt_seq_num}}},
      { NULL }
    };

    return util_tpl_to_json_str(tpl_tbl_ipt_obj);
}

static void iptables_telemetry(THRD_PARAM_t *p)
{
    http_rsp *rsp = NULL;
    char *my_mac = util_device_mac();
    char url[256];

    log("%s: started\n", __func__);

    log("%s: waiting for activate to complete\n", __func__);
    wait_for_activate();
    log("%s: done waiting for activate\n", __func__);

    util_wd_set_timeout(HTTP_REQ_MAX_TIME + unum_config.ipt_period);

    // Prepare the URL string
    util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API, url, sizeof(url),
                   TELEMETRY_PATH, my_mac);

    for(;;) {
        char *jstr = NULL;

        for(;;) {
            // Prepare the iptables JSON
            jstr = iptables_json();
            if(!jstr) {
                log("%s: JSON encode failed\n", __func__);
                break;
            }
#ifdef DEBUG
            if(get_test_num() == U_TEST_IPTABLES) {
                printf("%s: JSON for <%s>:\n%s\n", __func__, url, jstr);
                break;
            } else // send request (function call below) only if not a test
#endif // DEBUG

            // Send the iptables info
            // Not checking the response
            rsp = http_post_no_retry(url,
                                     "Content-Type: application/json\0"
                                     "Accept: application/json\0",
                                     jstr, strlen(jstr));

            if(rsp == NULL || (rsp->code / 100) != 2) {
                log("%s: request error, code %d%s\n",
                    __func__, rsp ? rsp->code : 0, rsp ? "" : "(none)");
                break;
            }

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
        sleep(unum_config.ipt_period);
    }

    // Never reaches here
    log("%s: done\n", __func__);
}

// Subsystem init fuction
int iptables_init(int level)
{
    int ret = 0;
    if(level == INIT_LEVEL_TELEMETRY) {
        // Start the iptables reporting job
        ret = util_start_thrd("iptables", iptables_telemetry, NULL, NULL);
    }
    return ret;
}
#ifdef DEBUG
// Test iptables telemetry
void test_iptables(void)
{
    set_activate_event();
    iptables_telemetry(NULL);
}
#endif // DEBUG
