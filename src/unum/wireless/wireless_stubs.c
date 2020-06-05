// (c) 2017-2019 minim.co
// unum ieee802.11 code stubs
// these stubs are overridden by platform implementation

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Capture the radio info
// Returns: 0 - if successful (all required info captured),
//          error otherwise
int __attribute__((weak)) wt_tpl_fill_radio_info(WT_JSON_TPL_RADIO_STATE_t *r)
{
    log_dbg("%s: not implemented\n", __func__);
    return -1;
}

// Tell all radios to scan wireless neighborhood
// Return: how many seconds till the scan results are ready
//         0 - if no need to wait, -1 - scan not supported
// Note: if the return value is less than WIRELESS_ITERATE_PERIOD
//       the results are queried on the next iteration
int __attribute__((weak)) wireless_do_scan(void)
{
    log_dbg("%s: not implemented\n", __func__);
    return 0;
}

// Return the name of the radio (must be static string)
char * __attribute__((weak)) wt_get_radio_name(int ii)
{
    log_dbg("%s: no radio %d\n", __func__, ii);
    return NULL;
}

// Init/deinit functions for each telemetry submission.
int __attribute__((weak)) wt_platform_subm_init(void)
{
    // If platform did not define it, assume it is just not needed, no error.
    return 0;
}
int __attribute__((weak)) wt_platform_subm_deinit(void)
{
    // If platform did not define it, assume it is just not needed, no error.
    return 0;
}

// Capture the STA info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int __attribute__((weak)) wt_tpl_fill_sta_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                                               WT_JSON_TPL_VAP_STATE_t *vinfo,
                                               WT_JSON_TPL_STA_STATE_t *sinfo,
                                               int ii)
{
    log_dbg("%s: not implemented\n", __func__);
    return 0;
}

// Capture the association info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int __attribute__((weak)) wt_tpl_fill_assoc_info(WT_JSON_TPL_RADIO_STATE_t *r,
                                                 WT_JSON_TPL_VAP_STATE_t *v,
                                                 WT_JSON_TPL_STA_STATE_t *s,
                                                 int ii)
{
    log_dbg("%s: not implemented\n", __func__);
    return 0;
}

// Extract the scan entry info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int __attribute__((weak))
    wt_tpl_fill_scan_entry_info(WT_JSON_TPL_SCAN_RADIO_t *rscan,
                                WT_JSON_TPL_SCAN_ENTRY_t *entry, int ii)
{
    log_dbg("%s: not implemented\n", __func__);
    return 0;
}

// Capture the scan list for a radio.
// The name and radio # in rscan structure are populated in the common code.
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int __attribute__((weak))
    wt_tpl_fill_scan_radio_info(WT_JSON_TPL_SCAN_RADIO_t *rscan)
{
    log_dbg("%s: not implemented\n", __func__);
    return 0;
}

// Notify platfrom code that the reported scan info has been collected
void __attribute__((weak)) wt_scan_info_collected(void)
{
    return;
}

// Capture the VAP info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int __attribute__((weak))
    wt_tpl_fill_vap_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                         WT_JSON_TPL_VAP_STATE_t *vinfo)
{
    log_dbg("%s: not implemented\n", __func__);
    return 0;
}

// Get wireless counters for a station.
// Parameters:
// ifname - The ifname to look up the STA on (could be NULL, search all)
// mac_str - STA MAC address (binary)
// wc - ptr to WIRELESS_COUNTERS_t buffer to return the data in
// Returns:
// 0 - ok, negative - unable to get the counters for the given MAC address
int __attribute__((weak))
    wireless_get_sta_counters(char *ifname, unsigned char* mac,
                              WIRELESS_COUNTERS_t *wc)
{
    log_dbg("%s: not supported\n", __func__);
    return -1;
}

// Init function for the platform wireless APIs (if needed).
// It is called at WIRELESS_ITERATE_PERIOD until TRUE is returned.
// Unless it returns true no other wireless APIs are called and
// no wireless telemetry is sent to the cloud.
int __attribute__((weak)) wt_platform_init(void)
{
    // If platform did not define it, assume it is just not needed.
    return TRUE;
}
