// (c) 2017-2019 minim.co
// unum device telemetry code

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// URL to send the router telemetry to, parameters:
// - the URL prefix
// - MAC addr (in xx:xx:xx:xx:xx:xx format)
#define TELEMETRY_PATH "/v3/unums/%s/telemetry"

// Structure for caching telemetry data
typedef struct {
  char lan_ip[INET_ADDRSTRLEN];
  char wan_ip[INET_ADDRSTRLEN];
  char wan_mac[MAC_ADDRSTRLEN];
  int mem_info_update_num;      // mem info number (used to track which is sent)
  int cpu_info_update_num;      // CPU info number (used to track which is sent)
  int first_ipt_diff_count;     // Counter included w/ initial diff chunks
  int ipt_seq_num;              // Monotonic counter included w/ all diffs
  int ipt_diff_complete;        // Flag indicating we are reporting that
                                // sending the diff chunk set is complete
  int ipt_diff_plus_offset;     // Offset for diff "add" to report
  int ipt_diff_minus_offset;    // Offset for diff "remove" to report
} TELEMETRY_DATA_t;

// Telemetry data we've successfully sent
static TELEMETRY_DATA_t last_sent;

// Telemetry data we are trying to send
static TELEMETRY_DATA_t new_data;

// Telemetry sequence #. Start counting after the first telemetry
// gets through to the server.
static unsigned long telemetry_seq_num = 0;


// Collect sysinfo telemetry
static void update_sysinfo_telemetry(void)
{
    // Update the memory usage info
    update_meminfo();

    // Update the CPU info
    update_cpuinfo();

    return;
}

// Collects router telemetry data, caches it in new_data and
// allocates JSON string with the telemetry request payload.
static char *router_telemetry_json()
{
    char *lan_ip = NULL;
    char *wan_ip = NULL;
    char *wan_mac = NULL;
    JSON_VAL_PFINT_t cpu_pfint_ptr = NULL;
    JSON_VAL_PFINT_t mem_pfint_ptr = NULL;
    JSON_KEYVAL_TPL_t *ipt_info_tpl_ptr = NULL;
    static long last_sysinfo_telemetry = 0;
    static long last_ipt_telemetry = 0;
    static int include_ipt_fist_diff_count = TRUE;
    static int ipt_reporting_diffs = FALSE;
    static int ipt_check_diffs = FALSE;

    // Init the new data set by copying over the last sent data
    memcpy(&new_data, &last_sent, sizeof(new_data));

    // Get the IP and MAC addresses info for LAN
    if(util_get_ipv4(GET_MAIN_LAN_NET_DEV(), new_data.lan_ip) == 0 &&
       strncmp(last_sent.lan_ip, new_data.lan_ip, INET_ADDRSTRLEN) != 0)
    {
        lan_ip = new_data.lan_ip;
    }
    // Get WAN IP and MAC (only if working as a gateway)
    if(IS_OPM(UNUM_OPM_GW))
    {
        if(util_get_ipv4(GET_MAIN_WAN_NET_DEV(), new_data.wan_ip) == 0 &&
           strncmp(last_sent.wan_ip, new_data.wan_ip, INET_ADDRSTRLEN) != 0)
        {
            wan_ip = new_data.wan_ip;
        }
        unsigned char mac[6];
        if(util_get_mac(GET_MAIN_WAN_NET_DEV(), mac) == 0 &&
           snprintf(new_data.wan_mac, MAC_ADDRSTRLEN,
                    MAC_PRINTF_FMT_TPL, MAC_PRINTF_ARG_TPL(mac)) > 0 &&
           strncmp(last_sent.wan_mac, new_data.wan_mac, MAC_ADDRSTRLEN) != 0)
        {
            wan_mac = new_data.wan_mac;
        }
    }

    // The error flags are reported repeatedly (if there are errors)
    char *no_dns = NULL;
    char *no_time = NULL;
    if(conncheck_no_dns()) {
        no_dns = "1";
    }
    if(conncheck_no_time()) {
        no_time = "1";
    }

    // Check if it is time to collect new sysinfo telemetry
    if(unum_config.sysinfo_period > 0 &&
       (last_sysinfo_telemetry == 0 ||
        util_time(1) - last_sysinfo_telemetry >= unum_config.sysinfo_period))
    {
        // Time to process sysinfo telemetry
        update_sysinfo_telemetry();
        // Update the last sysinfo report time
        last_sysinfo_telemetry = util_time(1);
    }
    // Check if memory information should be included into the telemetry
    new_data.mem_info_update_num = get_meminfo_counter();
    if(new_data.mem_info_update_num != last_sent.mem_info_update_num)
    {
        mem_pfint_ptr = mem_info_f;
    } else {
        mem_pfint_ptr = NULL;
    }
    // Check if CPU info has been updated. If yes set the pointers to retrieve
    // the new counters.
    new_data.cpu_info_update_num = get_cpuinfo_counter();
    if(new_data.cpu_info_update_num != last_sent.cpu_info_update_num)
    {
        cpu_pfint_ptr = cpu_usage_f;
    } else {
        cpu_pfint_ptr = NULL;
    }

    // Check if it is time to update iptables telemetry
    if(IS_OPM(UNUM_OPM_GW) && !ipt_check_diffs && unum_config.ipt_period > 0 &&
       ((last_ipt_telemetry == 0) ||
        ((util_time(1) - last_ipt_telemetry) >= unum_config.ipt_period)))
    {
        // If it's not the first diff we send, stop including the 
        // first_diff_count field.
        if(include_ipt_fist_diff_count && last_ipt_telemetry != 0) {
            include_ipt_fist_diff_count = FALSE;
        }

        // Collect iptables data
        ipt_diffs_colect_data();

        // Reset the reported offsets and force the run of the diff
        last_sent.ipt_diff_plus_offset = 0;
        last_sent.ipt_diff_minus_offset = 0;
        new_data.ipt_diff_plus_offset = 0;
        new_data.ipt_diff_minus_offset = 0;
        ipt_reporting_diffs = FALSE;
        ipt_check_diffs = TRUE;

        // Update the last iptables report time
        last_ipt_telemetry = util_time(1);
    }
    // Do we have ipt diffs to send? If yes prepare the offsets for the
    // JSON generation callback function to know what entries to report.
    if(ipt_check_diffs)
    {
        static JSON_OBJ_TPL_t tpl_tbl_ipt_obj = {
          {"ipt_initial_diff",
            {.type = JSON_VAL_PINT,   {.pi = NULL}}},
          {"ipt_diff_complete",
            {.type = JSON_VAL_PINT,   {.pi = NULL}}},
          {"ipt_seq_num",
            {.type = JSON_VAL_PINT,   {.pi = &new_data.ipt_seq_num}}},
          {"ipt_filter_diff", 
            {.type = JSON_VAL_FARRAY, {.fa = ipt_diffs_array_f}}},
          {"ipt_nat_diff",    
            {.type = JSON_VAL_FARRAY, {.fa = ipt_diffs_array_f}}},
          { NULL }
        };

        // This handles "ipt_initial_diff" counter (it's included while we are
        // sending initial table state after startup)
        if(include_ipt_fist_diff_count) {
            new_data.first_ipt_diff_count = last_sent.first_ipt_diff_count + 1;
            tpl_tbl_ipt_obj[0].val.pi = &new_data.first_ipt_diff_count;
        } else {
            tpl_tbl_ipt_obj[0].val.pi = NULL;
        }

        // This handles (prepares) the rule arrays data for sending
        if(ipt_diffs_prep_to_send(last_sent.ipt_diff_plus_offset,
                                  last_sent.ipt_diff_minus_offset,
                                  &new_data.ipt_diff_plus_offset,
                                  &new_data.ipt_diff_minus_offset))
        {
            ipt_check_diffs = TRUE;
            tpl_tbl_ipt_obj[1].val.pi = NULL;
            // We have diffs to send, set the flag so we know that after done
            // sending we need to report that diff sending is complete.
            ipt_reporting_diffs = TRUE;
            new_data.ipt_diff_complete = FALSE;
            // Add ipt info object to telemetry
            ipt_info_tpl_ptr = tpl_tbl_ipt_obj;
            // Update the IPT telemetry global sequence number
            new_data.ipt_seq_num = last_sent.ipt_seq_num + 1;
        }
        else if(ipt_reporting_diffs && !last_sent.ipt_diff_complete)
        {
            // We were reporting diffs. The last ipt diff has to report diff
            // complete flag (it will contain empty diff arrays though).
            // We have not reported it yet, so adding it now.
            ipt_check_diffs = TRUE;
            new_data.ipt_diff_complete = TRUE;
            tpl_tbl_ipt_obj[1].val.pi = &new_data.ipt_diff_complete;
            // Add ipt info object to telemetry
            ipt_info_tpl_ptr = tpl_tbl_ipt_obj;
            // Update the IPT telemetry global sequence number
            new_data.ipt_seq_num = last_sent.ipt_seq_num + 1;
        }
        else if(ipt_reporting_diffs && last_sent.ipt_diff_complete)
        {
            // Diff complete has been reported successfully.
            // No longer need to send ipt info object.
            ipt_check_diffs = FALSE;
            ipt_reporting_diffs = FALSE;
            new_data.ipt_diff_complete = FALSE;
            tpl_tbl_ipt_obj[1].val.pi = NULL;
        } else {
            ipt_check_diffs = FALSE;
        }

    }

    JSON_OBJ_TPL_t tpl = {
      {"lan_ip_address",     {.type = JSON_VAL_STR, {.s = lan_ip }}},
      {"wan_ip_address",     {.type = JSON_VAL_STR, {.s = wan_ip }}},
      {"wan_mac_address",    {.type = JSON_VAL_STR, {.s = wan_mac}}},
      {"no_dns",             {.type = JSON_VAL_STR, {.s = no_dns }}},
      {"no_time",            {.type = JSON_VAL_STR, {.s = no_time}}},
      {"mem_free",           {.type = JSON_VAL_PFINT,{.fpi = mem_pfint_ptr}}},
      {"mem_available",      {.type = JSON_VAL_PFINT,{.fpi = mem_pfint_ptr}}},
      {"cpu_usage",          {.type = JSON_VAL_PFINT,{.fpi = cpu_pfint_ptr}}},
      {"cpu_softirq",        {.type = JSON_VAL_PFINT,{.fpi = cpu_pfint_ptr}}},
      {"cpu_max_usage",      {.type = JSON_VAL_PFINT,{.fpi = cpu_pfint_ptr}}},
      {"cpu_max_softirq",    {.type = JSON_VAL_PFINT,{.fpi = cpu_pfint_ptr}}},
      {"ipt_diff",           {.type = JSON_VAL_OBJ, {.o = ipt_info_tpl_ptr}}},
      {"seq_num",            {.type = JSON_VAL_UL,  {.ul = telemetry_seq_num}}},
      {NULL}
    };

    return util_tpl_to_json_str(tpl);
}

// Updates the last_sent data with the info sent
static void router_telemetry_sent()
{
    // The request was successful update the cached values
    memcpy(&last_sent, &new_data, sizeof(last_sent));
}

// Process router telemetry response commands
static void process_telemetry_response(json_t *obj)
{
    int ii, size;
    const char *cmd_str;
    json_t *cmds = json_object_get(obj, "cmds");

    if(!cmds) {
        log("%s: no 'cmds' in telemetry response\n", __func__);
        return;
    }
    size = json_array_size(cmds);
    for(ii = 0; ii < size; ii++) {
        json_t *cmd_obj = json_array_get(cmds, ii);
        if(!cmd_obj || !(cmd_str = json_string_value(cmd_obj))) {
            log("%s: invalid array element %d in telemetry 'cmds'\n",
                __func__, ii);
            continue;
        }
        cmdproc_add_cmd(cmd_str);
    }
    return;
}

static void telemetry(THRD_PARAM_t *p)
{
    unsigned long no_rsp_t = 0;
    http_rsp *rsp = NULL;
    char *my_mac = util_device_mac();
    char url[256];

    log("%s: started\n", __func__);

    log("%s: waiting for activate to complete\n", __func__);
    wait_for_activate();
    log("%s: done waiting for activate\n", __func__);

    util_wd_set_timeout(HTTP_REQ_MAX_TIME + unum_config.telemetry_period);

    // Prepare the URL string
    util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API, url, sizeof(url),
                   TELEMETRY_PATH, my_mac);

    for(;;) {
        json_t *rsp_root = NULL;
        char *jstr = NULL;
        json_error_t jerr;

        for(;;) {

            // Prepare the telemetry data JSON
            jstr = router_telemetry_json();
            if(!jstr) {
                log("%s: JSON encode failed\n", __func__);
                break;
            }
#ifdef DEBUG
            if(get_test_num() == U_TEST_RTR_TELE) {
                printf("%s: JSON for <%s>:\n%s\n", __func__, url, jstr);
                if(rand() % 100 > 50) {
                    printf("Emulating %s request\n", "failed");
                } else {
                    printf("Emulating %s request\n", "successful");
                    // Pretend request was sent successfully
                    if(telemetry_seq_num == 0) {
                        ++telemetry_seq_num;
                    }
                    router_telemetry_sent();
                }
            } else // send request (function call below) only if not a test
#endif // DEBUG

            // Send the telemetry info
            rsp = http_post_no_retry(url,
                                     "Content-Type: application/json\0"
                                     "Accept: application/json\0",
                                     jstr, strlen(jstr));

            // No longer need the JSON string
            util_free_json_str(jstr);
            jstr = NULL;

            // While the sequence number is 0 we will not bump it up until
            // know that the request is processed by the server. After that
            // bump it up after each attempt to send the router telemetry.
            if(telemetry_seq_num > 0) {
                ++telemetry_seq_num;
            }

            if(rsp == NULL)
            {
                log("%s: no response\n", __func__);
                // If no response for over CONNCHECK_OFFLINE_RESTART time period
                // try to restart the conncheck (this will restart the agent)
                if(no_rsp_t == 0) {
                    no_rsp_t = util_time(1);
                } else if(util_time(1) - no_rsp_t > CONNCHECK_OFFLINE_RESTART) {
                    log("%s: restarting conncheck...\n", __func__);
                    restart_conncheck();
                }
                break;
            }
            no_rsp_t = 0;

            if((rsp->code / 100) != 2)
            {
                log("%s: request error, code %d\n", __func__, rsp->code);
                // If the response code is 400 trigger re-provisioning.
                if(rsp->code == 400) {
                    log("%s: restarting device provisioning...\n", __func__);
                    restart_provision();
                }
                break;
            }

            // The request has been processed by the server. If it had
            // sequence number 0 start incrementing it.
            if(telemetry_seq_num == 0) {
                ++telemetry_seq_num;
            }

            // Update telemetry data cache
            router_telemetry_sent();

            // Process the incoming JSON (commands)
            rsp_root = json_loads(rsp->data, JSON_REJECT_DUPLICATES, &jerr);
            if(!rsp_root) {
                log("%s: error at l:%d c:%d parsing response, msg: '%s'\n",
                    __func__, jerr.line, jerr.column, jerr.text);
                break;
            }

            process_telemetry_response(rsp_root);
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

        if(rsp_root) {
            json_decref(rsp_root);
            rsp_root = NULL;
        }

        util_wd_poll();
        sleep(unum_config.telemetry_period);
    }

    // Never reaches here
    log("%s: done\n", __func__);
}

// Subsystem init fuction
int telemetry_init(int level)
{
    int ret = 0;
    if(level == INIT_LEVEL_TELEMETRY) {
        // Start the telemetry reporting job
        ret = util_start_thrd("telemetry", telemetry, NULL, NULL);
    }
    return ret;
}
#ifdef DEBUG
// Test radio telemetry
void test_telemetry(void)
{
    set_activate_event();
    telemetry(NULL);
}
#endif // DEBUG
