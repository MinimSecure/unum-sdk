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

// unum cloud connectivity checker

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE

// URLs for checking HTTP & HTTPs connectivity
#define CONNCHECK_HTTP_URL "http://api.minim.co/time"
#define CONNCHECK_HTTPS_URL "https://my.minim.co/time"


// Set if cert and key files are provisioned for the device.
static UTIL_EVENT_t conncheck_complete = UTIL_EVENT_INITIALIZER;

// Local struct for storing connectivity check status
static CONNCHECK_ST_t cc_st;

// conncheck state
static int cc_cstate;
// conncheck state progress in %
static int cc_cstate_progress;

// Flag indicating that conncheck has been activated
// in the process instance
static int cc_activated = FALSE;


// Restart connectivity check and recovery
void restart_conncheck(void) {
    // Just restart the agent to make it go through another conncheck
    // in case it is active and we are in monitored mode (i.e. will restart)
    if(cc_activated && !unum_config.unmonitored) {
        util_restart();
    }
}

// Returns true if agent subsystems have to use IP address for connecting
// to the cloud server (instead of DNS name)
int conncheck_no_dns(void) {
    // For the operation modes that never run the troubleshooter
    // check the existence of the flag file and set the bit in the
    // conncheck state data structure.
    // TBD: we might be able to use conncheck_mk_info_file() to pull
    //      all the discovered info into the local cc_st structure
    if(!cc_activated) {
        struct stat st;
        if(stat(CONNCHECK_NO_DNS_FILE, &st) == 0) {
            cc_st.no_dns = TRUE;
        } else {
            cc_st.no_dns = FALSE;
        }
    }
    return cc_st.no_dns;
}

// Returns true if agent detected bad time and had to set it to the
// cloud time.
// Note: this only works in the context of the process running the
//       conncheck
int conncheck_no_time(void) {
    return (cc_st.use_minim_time || cc_st.set_time_error);
}

// This function blocks the caller till connectivity check is complete
void wait_for_conncheck(void)
{
    // Provisioning is disabled
    if(unum_config.no_conncheck) {
        return;
    }
    // Wait for the event to be set otherwise
    UTIL_EVENT_WAIT(&conncheck_complete);
}

#ifdef LAN_INFO_FOR_MOBILE_APP
// Create/update the mobile app info file
static void conncheck_mk_info_file(void)
{
    char *mac_addr = util_device_mac();
    char *can_connect = cc_st.can_connect ? "1" : NULL;
    char *http_connect = cc_st.http_connect ? "1" : NULL;
    char *use_minim_time = cc_st.use_minim_time ? "1" : NULL;
    char *set_time_error = cc_st.set_time_error ? "1" : NULL;
    char *no_interface = cc_st.no_interface ? "1" : NULL;
    char *no_counters = cc_st.no_counters ? "1" : NULL;
    char *no_rx_pkts = cc_st.no_rx_pkts ? "1" : NULL;
    char *no_tx_pkts = cc_st.no_tx_pkts ? "1" : NULL;
    char *rx_errors = cc_st.rx_errors ? "1" : NULL;
    char *tx_errors = cc_st.tx_errors ? "1" : NULL;
    char *no_ipv4 = cc_st.no_ipv4 ? "1" : NULL;
    char *no_ipv4_gw = cc_st.no_ipv4_gw ? "1" : NULL;
    char *bad_ipv4_gw = cc_st.bad_ipv4_gw ? "1" : NULL;
    char *no_dns = cc_st.no_dns ? "1" : NULL;

    unsigned long *p_connect_uptime = NULL;
    unsigned long *p_cloud_utc = NULL;
    char dev_ip[INET_ADDRSTRLEN];
    char *p_dev_ip = NULL;
    char dev_mask[INET_ADDRSTRLEN];
    char *p_dev_mask = NULL;
    char dev_gw[INET_ADDRSTRLEN];
    char *p_dev_gw = NULL;
    int *p_gw_ping_err_ratio = NULL;
    int *p_gw_ping_rsp_time = NULL;
    int *p_cstate_progress = NULL;
    char *p_dns_fails = NULL;
    char *cstate_str = NULL;
    struct _JSON_KEYVAL_TPL *p_conncheck_obj = NULL;

    // By default include progress unless 0
    if(cc_cstate_progress > 0) {
        p_cstate_progress = &cc_cstate_progress;
    }
    switch(cc_cstate)
    {
        case CSTATE_GRACE_PERIOD:
            cstate_str = "grace_period";
            break;
        case CSTATE_RECOVERY:
            cstate_str = "recovery";
            break;
        case CSTATE_CHECKING_IPV4:
            cstate_str = "checking_ipv4";
            break;
        case CSTATE_CHECKING_DNS:
            cstate_str = "checking_dns";
            break;
        case CSTATE_CHECKING_HTTPS:
            cstate_str = "checking_https";
            break;
        case CSTATE_CHECKING_COUNTERS:
            cstate_str = "checking_counters";
            break;
        case CSTATE_DONE:
            cstate_str = "done";
            p_cstate_progress = NULL;
            break;
        default:
            cstate_str = "unknown";
            p_cstate_progress = NULL;
            break;
    }

    // Collect the info for building JSON
    if(cc_st.can_connect)
    {
        p_connect_uptime = &cc_st.connect_uptime;
    }
    if(cc_st.can_connect || cc_st.use_minim_time || cc_st.http_connect)
    {
        p_cloud_utc = &cc_st.cloud_utc;
    }
    if(!cc_st.no_interface && !cc_st.no_ipv4 && cc_st.ipcfg.ipv4.i != 0)
    {
        sprintf(dev_ip, IP_PRINTF_FMT_TPL,
                IP_PRINTF_ARG_TPL(cc_st.ipcfg.ipv4.b));
        p_dev_ip = dev_ip;
        sprintf(dev_mask, IP_PRINTF_FMT_TPL,
                IP_PRINTF_ARG_TPL(cc_st.ipcfg.ipv4mask.b));
        p_dev_mask = dev_mask;
        if(!cc_st.no_ipv4_gw && cc_st.ipv4gw.i != 0) {
            sprintf(dev_gw, IP_PRINTF_FMT_TPL,
                    IP_PRINTF_ARG_TPL(cc_st.ipv4gw.b));
            p_dev_gw = dev_gw;
            if(!cc_st.bad_ipv4_gw) {
                p_gw_ping_err_ratio = (int *)&cc_st.gw_ping_err_ratio;
                if(cc_st.gw_ping_err_ratio < 100) {
                    p_gw_ping_rsp_time = &cc_st.gw_ping_rsp_time;
                }
            }
        }
    }
    if(cc_st.no_dns)
    {
        p_dns_fails = cc_st.dns_fails;
    }

    JSON_OBJ_TPL_t tpl_conncheck = {
      // flags
      { "can_connect",    {.type = JSON_VAL_STR, {.s = can_connect}}},
      { "http_connect",   {.type = JSON_VAL_STR, {.s = http_connect}}},
      { "use_minim_time", {.type = JSON_VAL_STR, {.s = use_minim_time}}},
      { "set_time_error", {.type = JSON_VAL_STR, {.s = set_time_error}}},
      { "no_interface",   {.type = JSON_VAL_STR, {.s = no_interface}}},
      { "no_counters",    {.type = JSON_VAL_STR, {.s = no_counters}}},
      { "no_rx_pkts",     {.type = JSON_VAL_STR, {.s = no_rx_pkts}}},
      { "no_tx_pkts",     {.type = JSON_VAL_STR, {.s = no_tx_pkts}}},
      { "rx_errors",      {.type = JSON_VAL_STR, {.s = rx_errors}}},
      { "tx_errors",      {.type = JSON_VAL_STR, {.s = tx_errors}}},
      { "no_ipv4",        {.type = JSON_VAL_STR, {.s = no_ipv4}}},
      { "no_ipv4_gw",     {.type = JSON_VAL_STR, {.s = no_ipv4_gw}}},
      { "bad_ipv4_gw",    {.type = JSON_VAL_STR, {.s = bad_ipv4_gw}}},
      { "no_dns",         {.type = JSON_VAL_STR, {.s = no_dns}}},
      // data
      { "connect_uptime", {.type = JSON_VAL_PUL, {.pul = p_connect_uptime}}},
      { "cloud_utc",      {.type = JSON_VAL_PUL, {.pul = p_cloud_utc}}},
      { "ipv4",           {.type = JSON_VAL_STR, {.s   = p_dev_ip}}},
      { "ipv4_mask",      {.type = JSON_VAL_STR, {.s   = p_dev_mask}}},
      { "ipv4_gw",        {.type = JSON_VAL_STR, {.s   = p_dev_gw}}},
      { "ipv4_gw_err_prc",{.type = JSON_VAL_PINT,{.pi  = p_gw_ping_err_ratio}}},
      { "ipv4_gw_rsp_t",  {.type = JSON_VAL_PINT,{.pi  = p_gw_ping_rsp_time}}},
      { "dns_fails",      {.type = JSON_VAL_STR, {.s   = p_dns_fails}}},
      { NULL }
    };

    // Do not include conncheck until we are in the done state
    if(cc_cstate == CSTATE_DONE) {
        p_conncheck_obj = tpl_conncheck;
    }

    JSON_OBJ_TPL_t tpl = {
      { "mac_address",    {.type = JSON_VAL_STR, {.s = mac_addr}}},
      { "cstate",         {.type = JSON_VAL_STR, {.s = cstate_str}}},
      { "cstate_progress",{.type = JSON_VAL_PINT,{.pi = p_cstate_progress}}},
      { "conncheck",      {.type = JSON_VAL_OBJ, {.o = p_conncheck_obj}}},
      { NULL }
    };

    char *tmp_fn = LAN_INFO_FOR_MOBILE_APP_TMP;
    char *fn = LAN_INFO_FOR_MOBILE_APP;

    if(mac_addr == NULL) {
        log("%s: error, unable to get the device base MAC address\n", __func__);
    }

    char *data = util_tpl_to_json_str(tpl);
    if(data == NULL) {
        log("%s: error, failed to generate JSON\n", __func__);
        return;
    }

    int len = strlen(data);
    int err = util_buf_to_file(tmp_fn, data, len, 00666);
    util_free_json_str(data);
    if(err != 0) {
        log("%s: error creating %s\n", __func__, fn);
        return;
    }
    rename(tmp_fn, fn);

    return;
}
#endif // LAN_INFO_FOR_MOBILE_APP

// Update the connectivity check state and if necessary for the
// state transition regenerate the LAN info file for the mobile app.
static void conncheck_update_state_event(int new_state, int progress)
{
    int update_file = TRUE;
    int update_state = TRUE;

    for(;;)
    {
        // If we are in the CSTATE_DONE state, stay in it and update
        // the file only if the new state is also CSTATE_DONE.
        if(CSTATE_DONE == cc_cstate) {
            if(CSTATE_DONE != new_state) {
                update_file = FALSE;
            }
            update_state = FALSE;
            break;
        }

        // The RECOVERY state the same as CSTATE_GRACE_PERIOD, but
        // during the attempt to reconnect after fixing some of the
        // recoverable networking issues.
        if(CSTATE_RECOVERY == cc_cstate &&
           CSTATE_GRACE_PERIOD == new_state)
        {
            new_state = CSTATE_RECOVERY;
            break;
        }

        // In any other state update both
        break;
    }

    // If no changes in either state or the %% bypass the update state code
    if(cc_cstate == new_state && cc_cstate_progress == progress) {
        update_state = FALSE;
    }

    // Update state
    if(update_state)
    {
        log("%s: state transition %d -> %d, progress %d\n",
            __func__, cc_cstate, new_state, progress);
        cc_cstate = new_state;
        cc_cstate_progress = progress;
    }

    // Update the mobile info file
    if(update_file) {
#ifdef LAN_INFO_FOR_MOBILE_APP
        log("%s: updating %s, state %d, progress %d\n",
            __func__, LAN_INFO_FOR_MOBILE_APP, cc_cstate, progress);
        conncheck_mk_info_file();
#endif // LAN_INFO_FOR_MOBILE_APP
    }

    return;
}

// Connectivity troubleshooter for HTTP & HTTPs layer
// Returns: negative - unrecoverable HTTP error detected
//          0 - unable to id the HTTP error, do more troubleshooting
//          positive - recoverable HTTP error detected
static int conncheck_troubleshoot_https(void)
{
    int ret = 0;

    conncheck_update_state_event(CSTATE_CHECKING_HTTPS, 0);

    for(;;)
    {
        http_rsp *rsp = http_get(CONNCHECK_HTTP_URL, NULL);
        if(rsp == NULL) {
            log("%s: no response from %s\n", __func__, CONNCHECK_HTTP_URL);
            ret = -1; // non-recoverable
            break;
        } else if((rsp->code / 100) != 2) {
            log_dbg("%s: error %d from %s\n",
                    __func__, rsp->code, CONNCHECK_HTTP_URL);
            free_rsp(rsp);
            break;
        }
        // Try to set cloud time to resolve the connectivity problems.
        // In theory this might be considered a vulnerability (an attacker
        // can use MItM to make router accept an outdated cert), but in
        // reality it is more likely to save us if we forget to update our
        // cert or the trust list.
        cc_st.http_connect = TRUE;
        sscanf(rsp->data, "%lu", &cc_st.cloud_utc);
        free_rsp(rsp);
        // Assuming connectivity check has failed because of time
        time_t tt = time(NULL);
        if(labs(cc_st.cloud_utc - (unsigned long)tt) > CONNCHECK_MAX_TIME_DIFF)
        {
            // If we have not yet attempted to fix the time this might
            // be a recoverable problem.
            if(!cc_st.use_minim_time) {
                cc_st.use_minim_time = TRUE;
                ret = 1;
            }
            tt = (time_t)cc_st.cloud_utc;
            stime(&tt);
            log("%s: HTTP ok, time set to %lu\n",
                __func__, cc_st.cloud_utc);
            break;
        }
        log("%s: HTTP ok, time ok, offset from cloud %ldsec\n",
            __func__, (long)((unsigned long)tt - cc_st.cloud_utc));
        break;
    }

    return ret;
}

// Connectivity troubleshooter for ipv4 DNS
// Returns: negative - all DNS tests fail
//          0 - all DNS tests passed
//          positive - some DNS tests passed
static int conncheck_troubleshoot_ipv4_dns(void)
{
    char **dns_list = http_dns_name_list();
    char *name;
    int ii, dns_list_count, dns_fails_ii = 0;
    int ok_count = 0, fail_count = 0;
    unsigned int hash_last = 0;

    conncheck_update_state_event(CSTATE_CHECKING_DNS, 0);

    // Count entries in the DNS name list
    for(ii = 0; dns_list && dns_list[ii] != NULL; ++ii);
    dns_list_count = ii;

    // Support portal is not used by HTTP and is not returned in
    // http_dns_name_list(), the loop is designed to check it first
    // before proceeding to the HTTP names list.
    for(ii = 0, name = "support.minim.co";
        dns_list && name; name = dns_list[ii++])
    {
        char buf[128];
        char *n = name;

        // DNS name in HTTP list entries is separated by ':' from the port & IPs
        char *sep = strchr(n, ':');
        if(sep != NULL) {
            int len = sep - n;
            len = (len < sizeof(buf) - 1) ? len : (sizeof(buf) - 1);
            strncpy(buf, n, len);
            buf[len] = 0;
            n = buf;
        }

        // Skip same names (http subsystem can pass a few w/ different ports)
        // The same names are guaranteed to be grouped together.
        unsigned int hash_new = util_hash(n, strlen(n));
        if(ii > 0 && hash_new == hash_last) {
            continue;
        }
        hash_last = hash_new;

        if(util_get_ip4_addr(n, NULL) == 0)
        {
            ++ok_count;
        }
        else if(dns_fails_ii < sizeof(cc_st.dns_fails) - 1)
        {
            ++fail_count;
            if(dns_fails_ii > 0) {
                cc_st.dns_fails[dns_fails_ii++] = ',';
            }
            strncpy(&cc_st.dns_fails[dns_fails_ii], n,
                    sizeof(cc_st.dns_fails) - dns_fails_ii);
            cc_st.dns_fails[sizeof(cc_st.dns_fails) - 1] = 0;
            dns_fails_ii += strlen(&cc_st.dns_fails[dns_fails_ii]);
            log_dbg("%s: DNS failed for <%s>\n", __func__, n);
            res_init();
        }
        else // increment fail count even if no space in cc_st.dns_fails
        {
            ++fail_count;
            res_init();
        }

        conncheck_update_state_event(CSTATE_CHECKING_DNS,
                                     ((ii + 1) * 100) / (dns_list_count + 1));
    }

    if(fail_count <= 0) {
        return 0;
    } else if(ok_count > 0) {
        return 1;
    }
    return -1;
}

// Connectivity troubleshooter for IPv4 layer
// Returns: negative - unrecoverable IPv4 error detected
//          0 - unable to id the IPv4 error, do more troubleshooting
//          positive - recoverable IPv4 error detected
static int conncheck_troubleshoot_ipv4(void)
{
    int ii, err, ret = 0;

    conncheck_update_state_event(CSTATE_CHECKING_IPV4, 0);

    // Check if the interface has the IP address
    err = util_get_ipcfg(cc_st.ifname, &cc_st.ipcfg);
    if(err != 0 || cc_st.ipcfg.ipv4.i == 0) {
        log("%s: %s IP address for %s\n",
            __func__, (err ? "error getting" : "zero"), cc_st.ifname);
        cc_st.no_ipv4 = TRUE;
        ret = -1; // non-recoverable
        return ret;
    }
    log("%s: %s has IPv4 address " IP_PRINTF_FMT_TPL "\n",
         __func__, cc_st.ifname, IP_PRINTF_ARG_TPL(cc_st.ipcfg.ipv4.b));

    // Check if we have the IPv4 default gateway
    err = util_get_ipv4_gw(&cc_st.ipv4gw);
    if(err != 0) {
        log("%s: no IPv4 default gateway\n", __func__);
        cc_st.no_ipv4_gw = TRUE;
        ret = -1; // non-recoverable
        return ret;
    }
    log("%s: IPv4 gateway " IP_PRINTF_FMT_TPL "\n",
         __func__, IP_PRINTF_ARG_TPL(cc_st.ipv4gw.b));

    // We have default gateway, check if it is reachable
    struct sockaddr saddr;
    struct in_addr inaddr = { cc_st.ipv4gw.i };
    if(util_get_ip4_addr(inet_ntoa(inaddr), &saddr) < 0)
    {
        log("%s: invalid IPv4 address " IP_PRINTF_FMT_TPL "\n",
            __func__, IP_PRINTF_ARG_TPL(cc_st.ipv4gw.b));
        cc_st.bad_ipv4_gw = TRUE;
    } else {
        int failed = 0;
        int sum = 0;
        for(ii = 0; ii < CONNCHECK_GW_PINGS; ++ii) {
            int tt = util_ping(&saddr, CONNCHECK_GW_PINGS_TIMEOUT);
            if(tt < 0) {
                ++failed;
            } else {
                sum += tt;
            }
            conncheck_update_state_event(CSTATE_CHECKING_IPV4,
                                         ((ii + 1) * 100) / CONNCHECK_GW_PINGS);
        }
        cc_st.gw_ping_err_ratio = (failed * 100) / CONNCHECK_GW_PINGS;
        if(failed >= CONNCHECK_GW_PINGS) {
            log("%s: IPv4 gw ping failed\n", __func__);
        } else {
            cc_st.gw_ping_rsp_time = sum / (CONNCHECK_GW_PINGS - failed);
            log("%s: IPv4 gw ping error ratio %d%%, avg rsp %dms\n",
                __func__, cc_st.gw_ping_err_ratio, cc_st.gw_ping_rsp_time);
        }
    }
    // In the normal ISP setup it is unlikely that a working gateway
    // is not responding to pings, so no DNS test if all pings fail.
    if(cc_st.gw_ping_err_ratio >= 100) {
        return ret;
    }

    // Check if the name resolution works for the DNS names we need.

    // If we already determined that DNS is not working and retrying
    // the troubleshooting for potentially fixing something else, skip
    // this step.
    if(cc_st.no_dns) {
        log("%s: DNS failure was detected before, skipping DNS test\n",
            __func__);
        return ret;
    }

    // Try a few times if seeing at least some successes
    for(ii = 0; TRUE; ++ii) {
        err = conncheck_troubleshoot_ipv4_dns();
        if(err == 0) {
            break;
        } else if(err > 0 && ii < CONNCHECK_DNS_TRIES) {
            continue;
        }
        cc_st.no_dns = TRUE;
        break;
    }

    if(cc_st.no_dns) {
        log("%s: DNS failed for %s\n", __func__, cc_st.dns_fails);
        ret = 1; // we can try to recover from the DNS problems
    } else {
        log("%s: DNS OK\n", __func__);
    }

    return ret;
}

// Connectivity troubleshooter, tries to determine what's wrong
// with the connection.
// Returns: TRUE if it was able to attempt a recovery and the
//          connectivity test should be rerun without changing the
//          cc_st data structure reset, otherwise it is FALSE.
// Note: Amazon has no IPv6 support yet, so no checking it here.
static int conncheck_troubleshooter(void)
{
    NET_DEV_STATS_t st, prev_st;
    int err, ret = FALSE;

    // record uptime when we started troubleshooter
    cc_st.tb_start_uptime = util_time(1);

    // Record interface name we are going to work with, in the AP
    // mode use main LAN interface (TBD: this might not always be the only
    // way to connnect to Internet), in the router mode use WAN
    // interface (we only support one)
#ifdef AP_MODE
    strncpy(cc_st.ifname, GET_MAIN_LAN_NET_DEV(), sizeof(cc_st.ifname));
#else // AP_MODE
    strncpy(cc_st.ifname, GET_MAIN_WAN_NET_DEV(), sizeof(cc_st.ifname));
#endif // AP_MODE
    cc_st.ifname[sizeof(cc_st.ifname) - 1] = 0;

    // Capture the base counters snapshot
    unsigned long done_at = util_time(1) + CONNCHECK_TB_COUNTERS_WAIT;
    err = util_get_dev_stats(cc_st.ifname, &prev_st);
    if(err != 0) {
        log("%s: failed to get base counters for %s\n",
            __func__, cc_st.ifname);
        cc_st.no_interface = TRUE;
        return ret;
    }

    // Troubleshoot IPv4 connectivity
    if(!ret && !err) {
        err = conncheck_troubleshoot_ipv4();
        ret |= (err > 0);
    }

    // Troubleshoot HTTPs connectivity
    if(!ret && !err) {
        err = conncheck_troubleshoot_https();
        ret |= (err > 0);
    }

    // Get the new counters snapshot (wait if still need to)
    conncheck_update_state_event(CSTATE_CHECKING_COUNTERS, 0);
    int wait_more = done_at - util_time(1);
    if(wait_more > 0) {
        log_dbg("%s: waiting %dsec more for calculating pkt counters\n",
                __func__, wait_more);
        int wait_more_total = wait_more;
        while(wait_more > 0) {
            conncheck_update_state_event(CSTATE_CHECKING_COUNTERS,
                                         ((wait_more_total - wait_more) * 100) /
                                         wait_more_total);
            sleep(2);
            wait_more = done_at - util_time(1);
        }
    }
    err = util_get_dev_stats(cc_st.ifname, &st);
    if(err != 0) {
        log("%s: failed to get the base counters for %s\n",
            __func__, cc_st.ifname);
        cc_st.no_interface = TRUE;
        return ret;
    }
    // Note: the absence of the rx packets is likely the only useful
    //       metric here, the interface is normally connected through
    //       the internal switch, so the real errors are not likely
    //       to show up until we can access the switch port counters
    if(st.rx_packets <= prev_st.rx_packets) {
        cc_st.no_rx_pkts = TRUE;
    }
    if(st.tx_packets <= prev_st.tx_packets) {
        cc_st.no_tx_pkts = TRUE;
    }
    if(st.rx_errors > prev_st.rx_errors             ||
       st.rx_dropped > prev_st.rx_dropped           ||
       st.rx_fifo_errors > prev_st.rx_fifo_errors   ||
       st.rx_frame_errors > prev_st.rx_frame_errors)
    {
        cc_st.rx_errors = TRUE;
    }
    if(st.tx_errors > prev_st.tx_errors             ||
       st.tx_dropped > prev_st.tx_dropped           ||
       st.tx_fifo_errors > prev_st.tx_fifo_errors   ||
       st.tx_carrier_errors > prev_st.tx_carrier_errors)
    {
        cc_st.tx_errors = TRUE;
    }
    log("%s: net activity rx:%s, tx:%s / error check rx:%s, tx:%s\n", __func__,
        (cc_st.no_rx_pkts ? "none" : "ok"), (cc_st.no_tx_pkts ? "none" : "ok"),
        (cc_st.rx_errors ? "fail" : "ok"), (cc_st.tx_errors ? "fail" : "ok"));

    return ret;
}

static void conncheck(THRD_PARAM_t *p)
{
    log("%s: started\n", __func__);

#if CONNCHECK_START_DELAY > 0
    // Random delay at startup
    unsigned int delay = rand() % CONNCHECK_START_DELAY;
    log("%s: delaying startup to %d sec\n", __func__, delay);
    sleep(delay);
#endif // CONNCHECK_START_DELAY > 0

    // Set watchdog to max time we may allow the agent to stay offline before
    // launching connection checker, the checker itself should never block for
    // longer than that.
    util_wd_set_timeout(CONNCHECK_OFFLINE_RESTART);

    // Flag tracking when info file becomes fully populated and no longer
    // should be updated w/ partial updates during recovery attempts
    int info_complete = FALSE;
    // Quiet period delay
    unsigned int quiet_period = 0;
    // Set wake up time to current (i.e. start probing immediately)
    unsigned long wake_up_t = util_time(1);
    // Calculate uptime when to start troubleshooting
    unsigned long start_tb_t = wake_up_t + CONNCHECK_START_GRACE_TIME;

    // Try to connect for up to CONNCHECK_START_GRACE_TIME, then troubleshoot
    // and attempt to recover (if still cannot connect)
    for(;;sleep(CONNCHECK_LOOP_DELAY), util_wd_poll())
    {
        unsigned long cur_t;

        // Get current time for the loop run
        cur_t = util_time(1);

        // If we are within the quiet time period, do nothing
        if(cur_t < wake_up_t) {
            continue;
        }

        // If we are within the startup grace period try to connect and
        // check against the server time
        if(cur_t < start_tb_t)
        {
            conncheck_update_state_event(CSTATE_GRACE_PERIOD,
                                         ((cur_t - wake_up_t) * 100) /
                                         (start_tb_t - wake_up_t));
            http_rsp *rsp = http_get_no_retry(CONNCHECK_HTTPS_URL, NULL);
            if(rsp == NULL) {
                log_dbg("%s: cannot get %s\n", __func__, CONNCHECK_HTTPS_URL);
                continue;
            }
            if((rsp->code / 100) != 2) {
                log_dbg("%s: error %d from %s\n",
                        __func__, rsp->code, CONNCHECK_HTTPS_URL);
                free_rsp(rsp);
                continue;
            }
            sscanf(rsp->data, "%lu", &cc_st.cloud_utc);
            free_rsp(rsp);
            unsigned long dev_utc = (unsigned long)time(NULL);
            if(labs(cc_st.cloud_utc - dev_utc) > CONNCHECK_MAX_TIME_DIFF)
            {
                log_dbg("%s: device time is off, offset from cloud %ldsec\n",
                        __func__, (long)(dev_utc - cc_st.cloud_utc));
                // If this is the first time we see too much of the time diff
                // for the device vs cloud set the flag and try to get it again
                if(!cc_st.use_minim_time)
                {
                    cc_st.use_minim_time = TRUE;
                    log_dbg("%s: retrying due to the time (NTP wait)\n",
                            __func__);
                    continue;
                }
                // If we shoud use cloud time set it and repeat the test,
                // If the set_time_error flag is set then we already set
                // the time, but it still did not work (e.g. NTP is bad)
                if(!cc_st.set_time_error)
                {
                    time_t tt = (time_t)cc_st.cloud_utc;
                    stime(&tt);
                    log_dbg("%s: retrying, time set to %lu\n", __func__, tt);
                    cc_st.set_time_error = TRUE;
                    continue;
                }
                // Proceed w/ bad time
                log("%s: unable to fix time, offset %ld, proceeding anyway\n",
                    __func__, (long)(cc_st.cloud_utc - dev_utc));
            } else {
                log_dbg("%s: device time is good, offset from cloud %ldsec\n",
                        __func__, (long)(dev_utc - cc_st.cloud_utc));
                // The set time (if was used) worked, so clear the flag
                cc_st.set_time_error = FALSE;
            }
            cc_st.can_connect = TRUE;
            cc_st.connect_uptime = util_time(1);
            break;
        }

        // We failed to connect within the grace time, so run the troubleshooter
        // and if it was able to recover try the connectivity test again.
        // Keeping the cc_st state unchaged so troubleshooter can see what it
        // has tried already.

        int recover_attempt = conncheck_troubleshooter();

        // If we are attempting a recovery the troubleshooting data might
        // not yet contain all the discovered info. Since the troubleshooter
        // can be restarted multiple times we have to keep track when the
        // full info has been filed and avoid overriding it with the partial
        // updates from that point on.
        info_complete |= !recover_attempt;

#ifdef LAN_INFO_FOR_MOBILE_APP
        // Write the findings to the file for the mobile app
        if(!recover_attempt) {
            conncheck_update_state_event(CSTATE_DONE, 0);
        } else if(!info_complete) {
            conncheck_update_state_event(CSTATE_RECOVERY, 0);
        }
#endif // LAN_INFO_FOR_MOBILE_APP

        // Calculate new quiet period (do it here to increase the quiet period
        // even if it isn't going to be used yet in case of attempting recovery)
        if(quiet_period == 0) {
            quiet_period = CONNCHECK_QUIET_TIME_START;
        } else if(quiet_period < CONNCHECK_QUIET_TIME_MAX) {
            quiet_period += CONNCHECK_QUIET_TIME_INC;
        }

        // If we can recover, attempt it immediately
        if(recover_attempt) {
            wake_up_t = util_time(1);
            start_tb_t = wake_up_t + CONNCHECK_RECOVER_GRACE_TIME;
            log("%s: cannot connect to cloud, attempting recover\n", __func__);
            continue;
        }

        // Set new wake up and troubleshoot times (the connectivity check
        // will start again after the quiet time period)
        wake_up_t = util_time(1) + quiet_period;
        // Troubleshoot at CONNCHECK_START_GRACE_TIME after the wakeup is still
        // not connected
        start_tb_t = wake_up_t + CONNCHECK_RETRY_GRACE_TIME;

        log("%s: cannot connect to cloud, retry in %ldsec\n",
            __func__, quiet_period);
        // Clean the state data structure to start from scratch (allows the
        // code to retry the recovery periodically)
        memset(&cc_st, 0, sizeof(cc_st));

        continue;
    }

    // Create/delete the 'no-DNS' flag file for processes not running conncheck
    if(cc_st.no_dns) {
        util_buf_to_file(CONNCHECK_NO_DNS_FILE, "", 0, 00666);
    } else {
        unlink(CONNCHECK_NO_DNS_FILE);
    }

#ifdef LAN_INFO_FOR_MOBILE_APP
    // Connected, update the mobile app info file
    conncheck_update_state_event(CSTATE_DONE, 0);
#endif // LAN_INFO_FOR_MOBILE_APP

    log("%s: connectivity test completed, uptime %lu sec\n",
        __func__, (unsigned long)util_time(1));
    UTIL_EVENT_SETALL(&conncheck_complete);

    log("%s: done\n", __func__);
    return;
}

// Subsystem init fuction
int conncheck_init(int level)
{
    int ret = 0;

    if(level == INIT_LEVEL_CONNCHECK)
    {
        // Start the device connection checker if the command line option
        // was not used to turn it off.
        if(unum_config.no_conncheck) {
            log("%s: disabled, skipping\n", __func__);
        } else {
            ret = util_start_thrd("conncheck", conncheck, NULL, NULL);
            cc_activated = (ret == 0);
        }
    }

    return ret;
}

// Conncheck test entry point
#ifdef DEBUG
void test_conncheck(void)
{
    log_dbg("%s: starting conncheck_init()\n", __func__);
    int res = conncheck_init(INIT_LEVEL_CONNCHECK);
    log_dbg("%s: conncheck_init() returned %d, waiting for check to complete\n",
            __func__, res);
    wait_for_conncheck();
    log_dbg("%s: connectivity check is complete\n", __func__);
    util_shutdown(0);
}
#endif // DEBUG
