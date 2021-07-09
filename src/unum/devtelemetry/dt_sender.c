// (c) 2017-2019 minim.co
// device telemetry information sender

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// URL to send the router telemetry to, parameters:
// - the URL prefix
// - MAC addr (in xx:xx:xx:xx:xx:xx format)
#define DEVTELEMETRY_PATH "/v3/unums/%s/devices/telemetry"


// Forward declarations
static JSON_VAL_TPL_t *tpl_tbl_stats_array_f(char *key, int ii);
static JSON_VAL_TPL_t *tpl_interfaces_array_f(char *key, int ii);
static JSON_VAL_TPL_t *tpl_dns_array_f(char *key, int idx);
static JSON_VAL_TPL_t *tpl_devcon_array_f(char *key, int idx);

// Devices telemetry JSON data pointer (set by tpcap handler,
// consumed by the dt_sender() thread).
static char *dev_telemetry_json;

// The event is set once the devices telemetry JSON is prepared for
// the transmission
static UTIL_EVENT_t json_ready = UTIL_EVENT_INITIALIZER;

// Template for root device telemetry JSON object
static JSON_OBJ_TPL_t tpl_dt_root = {
  // the "devices" entry has to be first
  { "devices",     { .type = JSON_VAL_FARRAY, {.fa = tpl_devcon_array_f}}},
  { "dns",         { .type = JSON_VAL_FARRAY, {.fa = tpl_dns_array_f}}},
  { "interfaces",  { .type = JSON_VAL_FARRAY, {.fa = tpl_interfaces_array_f}}},
  { "table_stats", { .type = JSON_VAL_FARRAY, {.fa = tpl_tbl_stats_array_f}}},
  { "fingerprint", { .type = JSON_VAL_FOBJ,   {.fo = fp_mk_json_tpl_f}}},
  { NULL }
};


// Dynamically builds JSON template for the devices telemetry
// connections array of a specific device. It is a helper for
// tpl_devcon_array_f().
// Note: the generated jansson objects can refer to string
//       pointers that can change in tpcap task later since
//       serialization of the whole telemetry object is done
//       in the context of the tpcap thread before it resumes
//       and all the objects are released after JSON is generated.
static JSON_VAL_TPL_t *tpl_con_array_f(DT_DEVICE_t *dev, int idx)
{
    int ii;
    // Buffer for IP address string
    static char con_ip[INET_ADDRSTRLEN];
    // Buffer for connection IP protocol number
    static unsigned long proto;
    // Buffer for connection TCP/UDP port
    static unsigned long port;
    // SYN and current window size for TCP connections
    static unsigned long syn_tcp_win_size;
#ifdef REPORT_CURRENT_TCP_WIN_SIZE
    static unsigned long cur_tcp_win_size;
#endif // REPORT_CURRENT_TCP_WIN_SIZE

    // Template array for bytes in and out
    static JSON_VAL_TPL_t b_in[DEVTELEMETRY_NUM_SLICES + 1] = {
        [0 ... (DEVTELEMETRY_NUM_SLICES - 1)] = { .type = JSON_VAL_UL },
        [ DEVTELEMETRY_NUM_SLICES ] = { .type = JSON_VAL_END }
    };
    static JSON_VAL_TPL_t b_out[DEVTELEMETRY_NUM_SLICES + 1] = {
        [0 ... (DEVTELEMETRY_NUM_SLICES - 1)] = { .type = JSON_VAL_UL },
        [ DEVTELEMETRY_NUM_SLICES ] = { .type = JSON_VAL_END }
    };
    // Template for generating device info object JSON.
    static JSON_OBJ_TPL_t tpl_tbl_con_obj = {
      { "l_port", { .type = JSON_VAL_PUL, {.pul = NULL}}}, // should be first
      { "r_port", { .type = JSON_VAL_PUL, {.pul = NULL}}}, // should be second
      { "syn_ws", { .type = JSON_VAL_PUL, {.pul = NULL}}}, // shuld be third
#ifdef REPORT_CURRENT_TCP_WIN_SIZE
      { "ws",     { .type = JSON_VAL_PUL, {.pul = NULL}}}, // should be forth
#endif // REPORT_CURRENT_TCP_WIN_SIZE
      { "r_ip",   { .type = JSON_VAL_STR, {.s = con_ip}}},
      { "proto",  { .type = JSON_VAL_PUL, {.pul = &proto}}},
      { "b_in",   { .type = JSON_VAL_ARRAY, {.a = b_in}}},
      { "b_out",  { .type = JSON_VAL_ARRAY, {.a = b_out}}},
      { NULL }
    };
    static JSON_VAL_TPL_t tpl_tbl_con_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_con_obj }
    };
    static DT_CONN_t *last_con = NULL; // last connection pointer
    static int last_idx = 0; // track last query index
    DT_CONN_t *p_con;

    // If the device pointer is NULL cannot proceed
    if(!dev) {
        return NULL;
    }

    // If starting over
    if(idx == 0) {
        last_idx = 0;
        last_con = p_con = &(dev->conn);
    }
    // The next query index should be the last +1, otherwise error
    // We should also have the last connection pointer for any idx != 0
    // and that should point to a valid next entry or NULL (if no more entries)
    else if(idx == last_idx + 1 && last_con != NULL && last_con->next != NULL &&
            last_con->next->hdr.dev == dev)
    {
        // Get the ptr to the next connection entry
        last_con = p_con = last_con->next;
    }
    // if something is not right or no more connections return NULL
    // to stop iterating
    else
    {
        return NULL;
    }
    last_idx = idx;

    // Prepare the template data
    snprintf(con_ip, sizeof(con_ip), IP_PRINTF_FMT_TPL,
             IP_PRINTF_ARG_TPL(p_con->hdr.ipv4.b));
    proto = p_con->hdr.proto;
    tpl_tbl_con_obj[0].val.pul = tpl_tbl_con_obj[1].val.pul = NULL;
    tpl_tbl_con_obj[2].val.pul = NULL;
#ifdef REPORT_CURRENT_TCP_WIN_SIZE
    tpl_tbl_con_obj[3].val.pul = NULL;
#endif // REPORT_CURRENT_TCP_WIN_SIZE
    if(proto == 6 || proto == 17) {
        port = p_con->hdr.port;
        if(p_con->hdr.rev) { // device's port
            tpl_tbl_con_obj[0].val.pul = &port;
        } else {             // peer's port
            tpl_tbl_con_obj[1].val.pul = &port;
        }
        if(proto == 6) {
            if(p_con->syn_tcp_win_size > 0) {
                syn_tcp_win_size = p_con->syn_tcp_win_size;
                tpl_tbl_con_obj[2].val.pul = &syn_tcp_win_size;
            }
#ifdef REPORT_CURRENT_TCP_WIN_SIZE
            if(p_con->cur_tcp_win_size > 0) {
                cur_tcp_win_size = p_con->cur_tcp_win_size;
                tpl_tbl_con_obj[3].val.pul = &cur_tcp_win_size;
            }
#endif // REPORT_CURRENT_TCP_WIN_SIZE
        }
    }
    for(ii = 0; ii < DEVTELEMETRY_NUM_SLICES; ii++) {
        b_in[ii].ul = p_con->bytes_to[ii];
        b_out[ii].ul = p_con->bytes_from[ii];
    }

    return &tpl_tbl_con_obj_val;
}

// Dynamically builds JSON template for the devices telemetry
// main device + connections info array.
// The function stores the pointer to the device being processed
// and uses it when called again for listing the device's
// connections (it is handler for both "devices" key in the root
// and "peers" key in the device's info object)
// Note: the generated jansson objects can refer to string
//       pointers that can change in tpcap task later since
//       serialization of the whole telemetry object is done
//       in the context of the tpcap thread before it resumes
//       and all the objects are released after JSON is generated.
static JSON_VAL_TPL_t *tpl_devcon_array_f(char *key, int idx)
{
    int ii;
    // Buffer for device MAC & IP address strings
    static char dev_mac[MAC_ADDRSTRLEN];
    static char dev_ip[INET_ADDRSTRLEN];
    // Template for generating device info object JSON.
    static JSON_OBJ_TPL_t tpl_tbl_dev_obj = {
      { "ifname", { .type = JSON_VAL_STR, {.s = NULL}}}, // has to be first
      { "ip",     { .type = JSON_VAL_STR, {.s = dev_ip}}},
      { "mac",    { .type = JSON_VAL_STR, {.s = dev_mac}}},
      { "conn",   { .type = JSON_VAL_FARRAY, {.fa = tpl_devcon_array_f}}},
      { NULL }
    };
    static JSON_VAL_TPL_t tpl_tbl_dev_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_dev_obj }
    };
    static DT_DEVICE_t *last_dev = NULL; // last device pointer
    static int last_idx = 0; // track last query index
    static int last_ii = 0;  // array index we stopped at for last_idx

    // If we are called for connections, call the helper and pass it the
    // pointer to the device being handled.
    if(tpl_dt_root[0].key != key) {
        return tpl_con_array_f(last_dev, idx);
    }

    // If starting over
    if(idx == 0) {
        last_idx = last_ii = 0;
    }
    // The next query index should be the last +1, otherwise error
    else if(idx != last_idx + 1) {
        return NULL;
    }
    last_idx = idx;

    // Get the ptr to the devices table
    DT_DEVICE_t **pp_dev = dt_get_dev_tbl();

    // Continue searching from where we ended last +1
    for(ii = last_ii + 1; ii < DTEL_MAX_DEV; ii++) {
        DT_DEVICE_t *dev = pp_dev[ii];
        if(!dev || dev->rating == 0) {
            continue;
        }
#ifdef FEATURE_LAN_ONLY
        // For the standalone device firmware ignore connection unless see in &
        // out traffic to avoid entries popping up when traffic to unknown MACs
        // is flooded
        if((dev->rating & 6) != 6) {
            continue;
        }
#endif // FEATURE_LAN_ONLY
        snprintf(dev_ip, sizeof(dev_ip), IP_PRINTF_FMT_TPL,
                 IP_PRINTF_ARG_TPL(dev->ipv4.b));
        snprintf(dev_mac, sizeof(dev_mac), MAC_PRINTF_FMT_TPL,
                 MAC_PRINTF_ARG_TPL(dev->mac));
        tpl_tbl_dev_obj[0].val.s = dev->ifname;
        last_dev = dev;
        break;
    }
    last_ii = ii;

    if(ii >= DTEL_MAX_DEV) {
        return NULL;
    }

    return &tpl_tbl_dev_obj_val;
}

// Dynamically builds JSON template for the devices telemetry
// DNS info array.
// Note: the generated jansson objects can refer to string
//       pointers that can change in tpcap task later since
//       serialization of the whole telemetry object is done
//       in the context of the tpcap thread before it resumes
//       and all the objects are released after JSON is generated.
static JSON_VAL_TPL_t *tpl_dns_array_f(char *key, int idx)
{
    int ii;
    // Buffer for DNS IP address string
    static char dns_ip[INET_ADDRSTRLEN];
    // Template for generating dns mappings JSON.
    static JSON_OBJ_TPL_t tpl_tbl_dns_obj = {
      { "name", { .type = JSON_VAL_STR, {.s = NULL}}}, // has to be first
      { "ip", { .type = JSON_VAL_STR, {.s = dns_ip}}},
      { NULL }
    };
    static JSON_VAL_TPL_t tpl_tbl_dns_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_dns_obj }
    };
    static int last_idx = 0; // track last query index
    static int last_ii = 0;  // array index we stopped at for last_idx

    // Starting over
    if(idx == 0) {
        last_idx = last_ii = 0;
    }
    // The next query index should be the last +1, otherwise error
    else if(idx != last_idx + 1) {
        return NULL;
    }
    last_idx = idx;

    // Get the ptr to the DNS IP addresses table
    DT_DNS_IP_t *p_ip = dt_get_dns_ip_tbl();

    // Continue searching from where we ended last +1
    for(ii = last_ii + 1; ii < DTEL_MAX_DNS_IPS; ii++) {
        DT_DNS_IP_t *ip_item = &(p_ip[ii]);
        if(ip_item->ipv4.i == 0) {
            continue;
        }
        DT_DNS_NAME_t *n_item = ip_item->dns;
        while(n_item && n_item->cname) {
            n_item = n_item->cname;
        }
        if(!n_item) {
            continue;
        }
        snprintf(dns_ip, sizeof(dns_ip), IP_PRINTF_FMT_TPL,
                 IP_PRINTF_ARG_TPL(ip_item->ipv4.b));
        tpl_tbl_dns_obj[0].val.s = n_item->name;
        break;
    }
    last_ii = ii;

    if(ii >= DTEL_MAX_DNS_IPS) {
        return NULL;
    }

    return &tpl_tbl_dns_obj_val;
}

// Dynamically builds JSON template for the devices telemetry
// interfaces array.
// Note: the generated jansson objects can refer to string
//       pointers that can change in tpcap task later since
//       serialization of the whole telemetry object is done
//       in the context of the tpcap thread before it resumes
//       and all the objects are released after JSON is generated.
static JSON_VAL_TPL_t *tpl_interfaces_array_f(char *key, int ii)
{
    // Static buffer tpl_tbl_interfaces_obj refers to (the array builder just
    // have to memcpy() data here for the template to pick it up.
    static DT_IF_STATS_t dtst;
    // Static buffers tpl_tbl_interfaces_obj refers to for ip addr/mask/mac strs
    static char ip[INET_ADDRSTRLEN];
    static char mask[INET_ADDRSTRLEN];
    static char mac[MAC_ADDRSTRLEN];
    // Template for generating interfaces JSON.
    static JSON_OBJ_TPL_t tpl_tbl_interfaces_obj = {
      { "ifname", { .type = JSON_VAL_STR, {.s = NULL}}}, // has to be first
      { "mac", { .type = JSON_VAL_STR, {.s = mac}}},
      { "ip", { .type = JSON_VAL_STR, {.s = ip}}},
      { "mask", { .type = JSON_VAL_STR, {.s = mask}}},
      { "wan",  { .type = JSON_VAL_PINT, {.pi = &dtst.wan}}},
      { "kind",  { .type = JSON_VAL_PINT, {.pi = &dtst.kind}}},
      { "slice",  { .type = JSON_VAL_PINT, {.pi = &dtst.slice}}},
      { "end_sec", { .type = JSON_VAL_PUL, {.pul = &dtst.sec}}},
      { "end_msec", { .type = JSON_VAL_PUL, {.pul = &dtst.msec}}},
      { "len_msec", { .type = JSON_VAL_PUL, {.pul = &dtst.len_t}}},
      { "bytes_in", { .type = JSON_VAL_PUL, {.pul = &dtst.bytes_in}}},
      { "bytes_out", { .type = JSON_VAL_PUL, {.pul = &dtst.bytes_out}}},
      { "proc_pkts", { .type = JSON_VAL_PUL, {.pul = &dtst.proc_pkts}}},
      { "pcap_pkts", { .type = JSON_VAL_PUL, {.pul = &dtst.tp_packets}}},
      { "miss_pkts", { .type = JSON_VAL_PUL, {.pul = &dtst.tp_drops}}},
      { NULL }
    };
    static JSON_VAL_TPL_t tpl_tbl_interfaces_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_interfaces_obj }
    };
    static JSON_VAL_TPL_t tpl_tbl_skip_val = { .type = JSON_VAL_SKIP };

    DT_IF_STATS_t *pdtst = dt_get_if_stats();

    if(!pdtst || ii >= (TPCAP_STAT_IF_MAX * DEVTELEMETRY_NUM_SLICES)) {
        return NULL;
    }

    if(pdtst[ii].len_t <= 0) {
        return &tpl_tbl_skip_val;
    }

    tpl_tbl_interfaces_obj[0].val.s = pdtst[ii].name;
    memcpy(&dtst, &(pdtst[ii]), sizeof(dtst));
    *mac = 0;
    char null_mac[6] = {};
    if(memcmp(pdtst[ii].mac, null_mac, sizeof(null_mac)) != 0) {
        snprintf(mac, MAC_ADDRSTRLEN,
                 MAC_PRINTF_FMT_TPL, MAC_PRINTF_ARG_TPL(dtst.mac));
    }
    *ip = *mask = 0;
    if(dtst.ipcfg.ipv4.i != 0) {
        snprintf(ip, sizeof(ip), IP_PRINTF_FMT_TPL,
                 IP_PRINTF_ARG_TPL(dtst.ipcfg.ipv4.b));
        snprintf(mask, sizeof(mask), IP_PRINTF_FMT_TPL,
                 IP_PRINTF_ARG_TPL(dtst.ipcfg.ipv4mask.b));
    }

    return &tpl_tbl_interfaces_obj_val;
}

// Dynamically builds JSON template for the devices telemetry
// table stats array.
static JSON_VAL_TPL_t *tpl_tbl_stats_array_f(char *key, int ii)
{
    // Static buffer tpl_tbl_stats_obj refers to. The array builder just
    // has to memcpy() data here for the template to pick it up.
    static DT_TABLE_STATS_t tbl_stats;
    // Template for generating table stats JSON.
    static JSON_OBJ_TPL_t tpl_tbl_stats_obj = {
      { "name", { .type = JSON_VAL_STR, {.s = NULL}}}, // should be first
      { "add_all", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.add_all}}},
      { "add_limit", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.add_limit}}},
      { "add_busy", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.add_busy}}},
      { "add_10", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.add_10}}},
      { "add_found", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.add_found}}},
      { "add_repl", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.add_repl}}},
#ifdef DT_CONN_MAP_IP_TO_DNS
      { "find_all", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.find_all}}},
      { "find_fails", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.find_fails}}},
      { "find_10", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.find_10}}},
#endif // DT_CONN_MAP_IP_TO_DNS
      { NULL }
    };
    static JSON_VAL_TPL_t tpl_tbl_stats_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_stats_obj }
    };
    static char *name[] = {
        "dns_names", "dns_ips", "connections", "devices", "festats"
    };
    static DT_TABLE_STATS_t *(*func_ptr[])(int) = {
        dt_dns_name_tbl_stats, dt_dns_ip_tbl_stats,
        dt_conn_tbl_stats, dt_dev_tbl_stats, fe_conn_tbl_stats
    };

    if(ii >= UTIL_ARRAY_SIZE(func_ptr)) {
        return NULL;
    }

    DT_TABLE_STATS_t *stats = (func_ptr[ii])(FALSE);
    if(!stats) {
        memset(&tbl_stats, 0, sizeof(tbl_stats));
    } else {
        memcpy(&tbl_stats, stats, sizeof(tbl_stats));
    }
    tpl_tbl_stats_obj[0].val.s = name[ii];

    return &tpl_tbl_stats_obj_val;
}

// Serializes devices telemetry data and returns pointer to
// the generated JSON.
// The function is called from the TPCAP thread/handler.
// Avoid blocking if possible.
static char *serialize_data(void)
{
#ifdef DEBUG
    if(tpcap_test_param.int_val == TPCAP_TEST_DT) {
        dt_main_tbls_test();
        return NULL;
    }
    if(tpcap_test_param.int_val == TPCAP_TEST_DNS) {
        dt_dns_tbls_test();
        return NULL;
    }
    if(tpcap_test_param.int_val == TPCAP_TEST_IFSTATS) {
        dt_if_counters_test();
        return NULL;
    }
    if(tpcap_test_param.int_val == TPCAP_TEST_DT_JSON ||
       tpcap_test_param.int_val == TPCAP_TEST_FP_JSON)
    {
        char *jstr = util_tpl_to_json_str(tpl_dt_root);
        if(!jstr) {
            printf("Failed to build device template JSON\n");
        }
        printf("Device telemetry JSON:\n%s\n", jstr);
        util_free_json_str(jstr);
        return NULL;
    }
#endif // DEBUG

    return util_tpl_to_json_str(tpl_dt_root);
}

// Generate and pass to the sender the devices telemetry JSON.
// If the previously submitted JSON buffer is not yet consumed
// replace it with the new one.
// The function is called from the TPCAP thread/handler.
// Avoid blocking if possible.
void dt_sender_data_ready(void)
{
    char *old_json;
    char *new_json;
    int ii;

    new_json = serialize_data();
    if(!new_json) {
        log("%s: Error, failed to build devices telemetry JSON\n",
            __func__);
        return;
    }

    for(ii = 0; ii < 3; ii++)
    {
        old_json = dev_telemetry_json;
        if(__sync_bool_compare_and_swap(&dev_telemetry_json,
                                        old_json, new_json))
        {
            break;
        }
    }
    if(ii >= 3) {
        // This really should never happen, but if it does free
        // the buffer and hope that the next time it will work.
        log("%s: Error submitting new JSON buffer", __func__);
        util_free_json_str(new_json);
        return;
    }
    // If the sender got stuck and has not yet consumed
    // the JSON buffer submitted before, free it and log a message.
    if(old_json) {
        log("%s: Warning, last JSON buffer wasn't sent, dropping\n",
            __func__);
        util_free_json_str(old_json);
        old_json = NULL;
    }
    // Notify the sender that the new pointer is ready
    UTIL_EVENT_SETALL(&json_ready);

    return;
}

// Grab the prepared for sending JSON buffer when it is ready
// and return to the caller.
// The function is called from the devices telemetry sender thread.
static char *consume_devices_telemetry_json()
{
    char *json = NULL;

    while(!json)
    {
        // Wait till we are signalled that the JSON is ready
        UTIL_EVENT_WAIT(&json_ready);
        // Note: this construct does not guarantee submission of
        //       every buffer when the system is under stress and
        //       fails to either collect or transmit information
        //       within the telemetry period time.
        UTIL_EVENT_RESET(&json_ready);
        // Grab the pointer to the data buffer
        json = dev_telemetry_json;
        if(!json) {
            continue;
        }
        if(!__sync_bool_compare_and_swap(&dev_telemetry_json,
                                         json, NULL))
        {
            continue;
        }
        // We have pulled a valid pointer out of dev_telemetry_json
        break;
    }

    return json;
}

// Device telemetry info sender
static void dt_sender(THRD_PARAM_t *p)
{
    http_rsp *rsp = NULL;
    char url[256];
    char *my_mac = util_device_mac();

    log("%s: started\n", __func__);

    // Check that we have MAC address
    if(!my_mac) {
        log("%s: cannot get device MAC\n", __func__);
        return;
    }

    log("%s: waiting for activate to complete\n", __func__);
    wait_for_activate();
    log("%s: done waiting for activate\n", __func__);

    util_wd_set_timeout(HTTP_REQ_MAX_TIME +
                        DEVTELEMETRY_NUM_SLICES * unum_config.tpcap_time_slice);

    // Prepare the URL string
    util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API, url, sizeof(url),
                   DEVTELEMETRY_PATH, my_mac);

    for(;;) {
        char *jstr = NULL;


        for(;;) {

            // Retrieve the telemetry data JSON
            jstr = consume_devices_telemetry_json();
            if(!jstr) {
                log("%s: JSON encode failed\n", __func__);
                break;
            }

            // Send the telemetry info
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
    }

    // Never reaches here
    log("%s: done\n", __func__);
}

// Sender init fuction
int dt_sender_start()
{
    return util_start_thrd("devtelemetry", dt_sender, NULL, NULL);
}

