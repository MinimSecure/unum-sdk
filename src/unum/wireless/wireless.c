// (c) 2017-2019 minim.co
// unum ieee802.11 code shared between platforms

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// URLs to send the wireless radios and neighborhood scan info to,
// parameters:
// - the URL prefix
// - MAC addr of the router (in xx:xx:xx:xx:xx:xx format)
#define WIRELESS_RADIOS_PATH "/v3/unums/%s/radios"
#define WIRELESS_SCAN_PATH "/v3/unums/%s/neighboring_access_points"


// Forward declarations
static JSON_VAL_TPL_t *wt_tpl_radios_array_f(char *key, int ii);
static JSON_VAL_TPL_t *wt_tpl_vaps_array_f(char *key, int ii);
static JSON_VAL_TPL_t *wt_tpl_stas_array_f(char *key, int ii);
static JSON_VAL_TPL_t *wt_tpl_assoc_array_f(char *key, int ii);
static JSON_VAL_TPL_t *wt_tpl_scan_radios_array_f(char *key, int ii);
static JSON_VAL_TPL_t *wt_tpl_scanlist_array_f(char *key, int ii);
static JSON_KEYVAL_TPL_t *wt_tpl_radio_extras_f(char *);
static JSON_KEYVAL_TPL_t *wt_tpl_vap_extras_f(char *);
static JSON_KEYVAL_TPL_t *wt_tpl_sta_extras_f(char *);
static JSON_KEYVAL_TPL_t *wt_tpl_scan_extras_f(char *);

// The structure stores state of the current radio we are building
// telemetry JSON template for
static WT_JSON_TPL_RADIO_STATE_t wt_radio_state;

// The structure stores state of the current VAP we are building
// telemetry JSON template for
static WT_JSON_TPL_VAP_STATE_t wt_vap_state;

// The structure stores state of the current STA we are building
// telemetry JSON template for or if we are in STA mode the state
// our own association
static WT_JSON_TPL_STA_STATE_t wt_sta_state;

// The structure stores the scan list entry data we are building
// telemetry JSON template for
static WT_JSON_TPL_SCAN_RADIO_t wt_scan_radio;

// The structure stores the scan list entry data we are building
// telemetry JSON template for
static WT_JSON_TPL_SCAN_ENTRY_t wt_scan_entry;

// Root template for generating scan info JSON
static JSON_OBJ_TPL_t wt_tpl_scan_root = {
  { "radios", { .type = JSON_VAL_FARRAY, {.fa = wt_tpl_scan_radios_array_f}}},
  { NULL }
};

// Radio scan info template
static JSON_OBJ_TPL_t tpl_tbl_scan_radio_obj = {
  { "name", { .type = JSON_VAL_STR, {.s = wt_scan_radio.name}}},
  { "scanlist", { .type = JSON_VAL_FARRAY, {.fa = wt_tpl_scanlist_array_f}}},
  { NULL }
};

// Scanlist entry info template
static JSON_OBJ_TPL_t tpl_tbl_scan_obj = {
  { "bssid",  { .type = JSON_VAL_STR, {.s = wt_scan_entry.bssid}}},
  { "ssid",   { .type = JSON_VAL_STR, {.s = wt_scan_entry.ssid}}},
  { "chan",   { .type = JSON_VAL_PINT, {.pi = &wt_scan_entry.chan}}},
  { "rssi",   { .type = JSON_VAL_PINT, {.pi = &wt_scan_entry.rssi}}},
  { "extras", { .type = JSON_VAL_FOBJ, {.fo = wt_tpl_scan_extras_f}}},
  { NULL }
};

// Root template for generating radio telemetry JSON
static JSON_OBJ_TPL_t wt_tpl_root = {
  { "radios", { .type = JSON_VAL_FARRAY, {.fa = wt_tpl_radios_array_f}}},
  { NULL }
};

// Radio info template
static JSON_OBJ_TPL_t tpl_tbl_radios_obj = {
  { "name", { .type = JSON_VAL_STR, {.s = wt_radio_state.name}}},
  { "channel", { .type = JSON_VAL_PINT, {.pi = &wt_radio_state.chan}}},
  { "extras",  { .type = JSON_VAL_FOBJ, {.fo = wt_tpl_radio_extras_f}}},
  { "vaps", { .type = JSON_VAL_FARRAY, {.fa = wt_tpl_vaps_array_f}}},
  { NULL }
};

// VAP info template
static JSON_OBJ_TPL_t tpl_tbl_vaps_obj = {
  { "interface", { .type = JSON_VAL_STR, {.s = wt_vap_state.ifname}}},
  { "ssid",  { .type = JSON_VAL_STR, {.s = wt_vap_state.ssid}}},
  { "bssid", { .type = JSON_VAL_STR, {.s = wt_vap_state.bssid}}},
  { "mode",  { .type = JSON_VAL_STR, {.s = wt_vap_state.mode }}},
  { "kind",  { .type = JSON_VAL_PINT, {.pi = &wt_vap_state.kind }}},
  { "extras",{ .type = JSON_VAL_FOBJ, {.fo = wt_tpl_vap_extras_f}}},
  { "stas",  { .type = JSON_VAL_FARRAY, {.fa = wt_tpl_stas_array_f}}},
  { NULL }
};

// STA VAP (not really VAP, STA interface) info template
static JSON_OBJ_TPL_t tpl_tbl_vapsta_obj = {
  { "interface",   { .type = JSON_VAL_STR, {.s = wt_vap_state.ifname}}},
  { "sta_mac",     { .type = JSON_VAL_STR, {.s = wt_vap_state.mac}}},
  { "ssid",        { .type = JSON_VAL_STR, {.s = wt_vap_state.ssid}}},
  { "mode",        { .type = JSON_VAL_STR, {.s = wt_vap_state.mode }}},
  { "kind",        { .type = JSON_VAL_PINT, {.pi = &wt_vap_state.kind }}},
  { "extras",      { .type = JSON_VAL_FOBJ, {.fo = wt_tpl_vap_extras_f}}},
  { "assoc_info",  { .type = JSON_VAL_FARRAY, {.fa = wt_tpl_assoc_array_f}}},
  { NULL }
};

// STA/association info template
static JSON_OBJ_TPL_t tpl_tbl_sta_state_obj = {
  { "mac",    { .type = JSON_VAL_STR, {.s = wt_sta_state.mac}}},
  { "rssi",   { .type = JSON_VAL_PINT, {.pi = &wt_sta_state.rssi}}},
  { "extras", { .type = JSON_VAL_FOBJ, {.fo = wt_tpl_sta_extras_f}}},
  { NULL }
};

// Template value for skipping an element of an array
static JSON_VAL_TPL_t tpl_tbl_array_skip_val = { .type = JSON_VAL_SKIP };

// Flag forcing scan at the next radio telemetry iteration
static int force_neighborhood_scan = FALSE;


// Dynamically builds JSON template for the scanlist radios array.
static JSON_VAL_TPL_t *wt_tpl_scan_radios_array_f(char *key, int ii)
{
    static JSON_VAL_TPL_t tpl_tbl_scan_radios_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_scan_radio_obj }
    };

    memset(&wt_scan_radio, 0, sizeof(wt_scan_radio));

    // Get the name for the current radio #
    wt_scan_radio.num = ii;
    char *radio_name = wt_get_radio_name(ii);
    // If platform has no more radios return NULL
    if(radio_name == NULL) {
        wt_scan_radio.name[0] = 0;
        return NULL;
    }
    strncpy(wt_scan_radio.name, radio_name,
            sizeof(wt_scan_radio.name));
    wt_scan_radio.name[sizeof(wt_scan_radio.name) - 1] = 0;

    // Collect the scan data for the radio
    int rc = wt_tpl_fill_scan_radio_info(&wt_scan_radio);
    if(rc < 0) {
        log("%s: unable to collect scan data for <%s>, skipping\n",
            __func__, radio_name);
        return &tpl_tbl_array_skip_val;
    } else if(rc > 0) { // No error, platform code asks to skip
        return &tpl_tbl_array_skip_val;
    }

    return &tpl_tbl_scan_radios_obj_val;
}

// Return pointer to scan "extras" JSON object template
static JSON_KEYVAL_TPL_t *wt_tpl_scan_extras_f(char *key)
{
    return wt_scan_entry.extras;
}

// Dynamically builds JSON template for the wireless telemetry
// neighborhood scan array.
static JSON_VAL_TPL_t *wt_tpl_scanlist_array_f(char *key, int ii)
{
    static JSON_VAL_TPL_t tpl_tbl_scan_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_scan_obj }
    };

    memset(&wt_scan_entry, 0, sizeof(wt_scan_entry));

    // Check if the current index is within the number of entries we have
    if(ii >= wt_scan_radio.scan_entries) {
        return NULL;
    }

    // Populate the data we need to report for the STA
    int rc = wt_tpl_fill_scan_entry_info(&wt_scan_radio, &wt_scan_entry, ii);
    if(rc < 0) {
        log("%s: unable to get the data for scan entry %d, skipping\n",
            __func__, ii);
        return &tpl_tbl_array_skip_val;
    } else if(rc > 0) { // No error, platform code asks to skip
        return &tpl_tbl_array_skip_val;
    }

    return &tpl_tbl_scan_obj_val;
}

// Return pointer to STA "extras" JSON object template
static JSON_KEYVAL_TPL_t *wt_tpl_sta_extras_f(char *key)
{
    return wt_sta_state.extras;
}

// Dynamically builds JSON template for the wireless telemetry
// STAs array.
static JSON_VAL_TPL_t *wt_tpl_stas_array_f(char *key, int ii)
{
    static JSON_VAL_TPL_t tpl_tbl_stas_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_sta_state_obj }
    };

    memset(&wt_sta_state, 0, sizeof(wt_sta_state));

    // Check if the current VAP has that many STAs
    if(ii < 0 || ii >= wt_vap_state.num_assocs) {
        return NULL;
    }

    // Populate the data we need to report for the STA
    int rc = wt_tpl_fill_sta_info(&wt_radio_state,
                                  &wt_vap_state,
                                  &wt_sta_state, ii);
    if(rc < 0) {
        log("%s: unable to get the necessary data for STA %d, skipping\n",
            __func__, ii);
        return &tpl_tbl_array_skip_val;
    } else if(rc > 0) { // No error, platform code asks to skip
        return &tpl_tbl_array_skip_val;
    }

    return &tpl_tbl_stas_obj_val;
}

// Dynamically builds JSON template for the wireless telemetry
// assoc info array we send if reporting for an interface in the STA mode.
static JSON_VAL_TPL_t *wt_tpl_assoc_array_f(char *key, int ii)
{
    static JSON_VAL_TPL_t tpl_tbl_assoc_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_sta_state_obj }
    };

    memset(&wt_sta_state, 0, sizeof(wt_sta_state));

    // Check if we have the association info to report. Normally there will
    // be just 1, the aray template here is used just for the code parody with
    // the AP's STA list (but who knows, could be used for reporting preauth)
    if(ii < 0 || ii >= wt_vap_state.num_assocs) {
        return NULL;
    }

    // Populate the data we need to report for the association
    int rc = wt_tpl_fill_assoc_info(&wt_radio_state,
                                    &wt_vap_state,
                                    &wt_sta_state, ii);
    if(rc < 0) {
        log("%s: unable to get the necessary data for STA %d, skipping\n",
            __func__, ii);
        return &tpl_tbl_array_skip_val;
    } else if(rc > 0) { // No error, platform code asks to skip
        return &tpl_tbl_array_skip_val;
    }

    return &tpl_tbl_assoc_obj_val;
}

// Return pointer to VAP "extras" JSON object template
static JSON_KEYVAL_TPL_t *wt_tpl_vap_extras_f(char *key)
{
    return wt_vap_state.extras;
}

// Dynamically builds JSON template for the wireless telemetry
// VAPs array.
static JSON_VAL_TPL_t *wt_tpl_vaps_array_f(char *key, int ii)
{
    static JSON_VAL_TPL_t tpl_tbl_vaps_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_vaps_obj }
    };
    static JSON_VAL_TPL_t tpl_tbl_vapsta_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_vapsta_obj }
    };

    memset(&wt_vap_state, 0, sizeof(wt_vap_state));

    // Check if the current radio has that many VAPs
    if(ii >= wt_radio_state.num_vaps) {
        return NULL;
    }

    // Get the ifname for the current VAP
    wt_vap_state.num = ii;
    strncpy(wt_vap_state.ifname, wt_radio_state.vaps[ii], IFNAMSIZ);
    wt_vap_state.ifname[IFNAMSIZ - 1] = 0;

    // Preset the default operation mode to "ap", the platform can
    // override when reporting non-AP interfaces (like 802.11s mesh points)
    strncpy(wt_vap_state.mode, WIRELESS_OPMODE_AP, sizeof(wt_vap_state.mode));
    wt_vap_state.mode[sizeof(wt_vap_state.mode) - 1] = 0;

    // Populate the data we need to report for the VAP
    int rc = wt_tpl_fill_vap_info(&wt_radio_state, &wt_vap_state);
    if(rc < 0) {
        log("%s: unable to get the necessary data for VAP <%s>, skipping\n",
            __func__, wt_vap_state.ifname);
        return &tpl_tbl_array_skip_val;
    } else if(rc > 0) { // No error, platform code asks to skip
        return &tpl_tbl_array_skip_val;
    }

    if(strcmp(wt_vap_state.mode, WIRELESS_OPMODE_STA) == 0) {
        return &tpl_tbl_vapsta_obj_val;
    }

    return &tpl_tbl_vaps_obj_val;
}

// Return pointer to radio "extras" JSON object template
static JSON_KEYVAL_TPL_t *wt_tpl_radio_extras_f(char *key)
{
    return wt_radio_state.extras;
}

// Dynamically builds JSON template for the wireless telemetry
// radios array.
static JSON_VAL_TPL_t *wt_tpl_radios_array_f(char *key, int ii)
{
    static JSON_VAL_TPL_t tpl_tbl_radios_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_radios_obj }
    };

    memset(&wt_radio_state, 0, sizeof(wt_radio_state));

    // Get the name for the current radio #
    wt_radio_state.num = ii;
    char *radio_name = wt_get_radio_name(ii);
    // If platform has no more radios return NULL
    if(radio_name == NULL) {
        wt_radio_state.name[0] = 0;
        return NULL;
    }
    strncpy(wt_radio_state.name, radio_name, sizeof(wt_radio_state.name));
    wt_radio_state.name[sizeof(wt_radio_state.name) - 1] = 0;

    // Populate the data we need to report for the radio
    int rc = wt_tpl_fill_radio_info(&wt_radio_state);
    if(rc < 0) {
        if (rc != WIRELESS_RADIO_IS_DOWN) {
            log("%s: unable to get the necessary data for <%s>, skipping\n",
                __func__, radio_name);
        }
        return &tpl_tbl_array_skip_val;
    } else if(rc > 0) { // No error, platform code asks to skip
        return &tpl_tbl_array_skip_val;
    }

    return &tpl_tbl_radios_obj_val;
}

// Colect and report to the cloud radio telemetry info
static void wireless_do_radio_telemetry(void)
{
    char *jstr = NULL;
    http_rsp *rsp = NULL;
    char *my_mac = util_device_mac();
    char url[256];
    int err;

    // Check that we have MAC address
    if(!my_mac) {
        log("%s: cannot get device MAC\n", __func__);
        return;
    }

    // Call platform telemetry submission init function
    err = wt_platform_subm_init();
    if(err != 0) {
        log("%s: wt_platform_subm_init() returned error %d\n", __func__, err);
        return;
    }

    // Prepare the URL string
    util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API, url,
                   sizeof(url), WIRELESS_RADIOS_PATH, my_mac);

    for(;;) {

        // Build the the radio telemetry data JSON
        jstr = util_tpl_to_json_str(wt_tpl_root);
        if(!jstr) {
            log("%s: JSON encode failed\n", __func__);
            break;
        }

#ifdef DEBUG
        if(get_test_num() == U_TEST_WL_RT)
        {
            printf("%s: JSON for <%s>:\n%s\n", __func__, url, jstr);
            break;
        } else if(get_test_num() == U_TEST_WL_SCAN) {
            break;
        }
#endif // DEBUG

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

    // Call platform telemetry submission deinit function
    wt_platform_subm_deinit();

    return;
}

// Colect and report to the cloud wireless scan results
static void wireless_do_scan_report(void)
{
    char *jstr = NULL;
    http_rsp *rsp = NULL;
    char *my_mac = util_device_mac();
    char url[256];

    for(;;) {

        // Check that we have MAC address
        if(!my_mac) {
            log("%s: cannot get device MAC\n", __func__);
            break;
        }

        // Prepare the URL string
        util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API, url,
                       sizeof(url), WIRELESS_SCAN_PATH, my_mac);

        // Build the the radio telemetry data JSON
        jstr = util_tpl_to_json_str(wt_tpl_scan_root);
        // Notify subsystems that scan info has been collected
        wt_scan_info_collected();
        if(!jstr) {
            log("%s: JSON encode failed\n", __func__);
            break;
        }

#ifdef DEBUG
        if(get_test_num() == U_TEST_WL_SCAN)
        {
            printf("%s: JSON for <%s>:\n%s\n", __func__, url, jstr);
            break;
        } else if(get_test_num() == U_TEST_WL_RT) {
            break;
        }
#endif // DEBUG

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

    return;
}

// Wireless monitoring thread. All the wireless finctions are called from
// here. With a few exceptions the calls are made through the routine
// generating JSON from the template data structures.
static void wireless(THRD_PARAM_t *p)
{
    static unsigned long do_telemetry_at = 0;
    static unsigned long do_scan_at = 0;
    static unsigned long do_scan_report_at = 0xFFFFFFFF;
    static int platform_init_done = FALSE;
    unsigned long cur_t;

    log("%s: started\n", __func__);

    log("%s: waiting for activation to complete\n", __func__);
    wait_for_activate();
    log("%s: done waiting for activation\n", __func__);

    util_wd_set_timeout(2 * HTTP_REQ_MAX_TIME + WIRELESS_MAX_DELAY);

    for(;;) {
        sleep(WIRELESS_ITERATE_PERIOD);
        util_wd_poll();

        // Run platform specific init
        if(!platform_init_done) {
            if(!(platform_init_done = wt_platform_init())) {
                log("%s: platform init failed, skipping wireless telemetry\n",
                    __func__);
                continue;
            }
        }

        // Capture current time
        cur_t = util_time(1);

        // Check if it is time to report radio telemetry
        if(do_telemetry_at < cur_t) {
            do_telemetry_at = cur_t + unum_config.wireless_telemetry_period;
            wireless_do_radio_telemetry();
        }

        // Check if it is time to do scan (but only unless a report already
        // pending)
        int force_scan =
            __sync_bool_compare_and_swap(&force_neighborhood_scan, TRUE, FALSE);
        // Check if scan has been disabled
        // scan is not forced
        // and scan is not already triggered
        if(do_scan_report_at == 0xFFFFFFFF && !force_scan &&
            unum_config.wireless_scan_period == 0) {
            continue;
        }
        // Scan only if not waiting for report and it is time to (or forced)
        if(do_scan_report_at == 0xFFFFFFFF &&
           (do_scan_at < cur_t || force_scan))
        {
            do_scan_at = cur_t + unum_config.wireless_scan_period;
            int scan_rep_t = wireless_do_scan();
            if(scan_rep_t >= 0) {
                do_scan_report_at = cur_t + scan_rep_t;
            } else {
                do_scan_report_at = 0xFFFFFFFF;
            }
        }

        // Check if it is time to report the scan results
        if(do_scan_report_at < cur_t) {
            wireless_do_scan_report();
            do_scan_report_at = 0xFFFFFFFF;
        }
    }

    log("%s: done\n", __func__);
}

// Function requesting the wireless scan to be performed immediately.
void cmd_wireless_initiate_scan(void)
{
    __sync_bool_compare_and_swap(&force_neighborhood_scan, FALSE, TRUE);
    return;
}

// Subsystem init function
int wireless_init(int level)
{
    int ret = 0;
    // Initialize wireless monitoring subsystem
    if(level == INIT_LEVEL_WIRELESS) {
        // Start the wireless telemetry reporting job
        ret = util_start_thrd("wireless", wireless, NULL, NULL);
    }
    return ret;
}

#ifdef DEBUG
// Test wireless info collection
void test_wireless(void)
{
    set_activate_event();
    wireless(NULL);
}
#endif // DEBUG
