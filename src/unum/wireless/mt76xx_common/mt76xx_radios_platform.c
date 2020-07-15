// (c) 2018-2019 minim.co
// unum platform code for collecting wireless radios info

#include "unum.h"


// Include declarations for mt76xx
#include "mt76xx_wireless_platform.h"

// Include declarations for platform driver specific code
// provided by the platform.
#include "wireless_platform.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// List of platform radios indexed by the radio number
static char *radio_list[] = {
    WIFI_RADIO_24_PREFIX "0", // 2.4 GHz radio
    WIFI_RADIO_5_PREFIX "0" // 5 GHz radio
};


// Return the name of the radio
char *wt_get_radio_name(int ii)
{
    if(ii < 0 || ii >= UTIL_ARRAY_SIZE(radio_list)) {
        return NULL;
    }
    return radio_list[ii];
}

// Returns the # of the radio the interface belongs to
// (radio 0 - 2.4GHz, 1 - 5GHz), -1 if not a wireless
// interface
int wt_get_ifname_radio_num(char *ifname)
{
    int ifradio = -1;

    // Try to match the 5GHz (longest name)
    if(strncmp(ifname, WIFI_RADIO_5_PREFIX, 3) == 0 ||
       strncmp(ifname, "apclii", 6) == 0)
    {
        ifradio = 1;
    } // Match the 2.4
    else if(strncmp(ifname, WIFI_RADIO_24_PREFIX, 2) == 0 ||
            strncmp(ifname, "apcli", 5) == 0)
    {
        ifradio = 0;
    }

    return ifradio;
}

// Capture the radio info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_radio_info(WT_JSON_TPL_RADIO_STATE_t *rinfo)
{
    char *ifname = rinfo->name;
    // radio extras values and template
    static char mode[16];   // The name of the mode (word: master, repeater...)
    static char country[4]; // 3-digit country code
    static unsigned long stats_time[] = { 0, 0 };
    static char stats[2048];// dump of readable stats from private IOCTL
    static JSON_OBJ_TPL_t extras_obj = {
      { "stats", { .type = JSON_VAL_STR, {.s  = NULL}}}, // must be first
      { "mode",  { .type = JSON_VAL_STR, {.s = mode}}},
      { "country", { .type = JSON_VAL_STR, {.s  = country}}},
      { NULL }
    };

    // For MTK we use real interface names of the first SSID for radio
    // interface names. It should be up if the radio is enabled.
    if(!util_net_dev_is_up(ifname)) {
        return 1; // positive value tells caller to skip
    }

    // Get the radio channel
    rinfo->chan = wt_wlext_get_channel(ifname);
    if(rinfo->chan <= 0) {
        log("%s: failed to get channel for <%s>\n", __func__, ifname);
        return -1;
    }

    // Get radio mode
    char *wl_mode = wt_wlext_get_mode(ifname);
    if(!wl_mode) {
        log("%s: failed to get mode for <%s>\n", __func__, ifname);
        return -2;
    }
    strncpy(mode, wl_mode, sizeof(mode));

    // Get the country code (cache it in static, it does not change till boot)
    char *cc = wt_rt_get_radio_country(ifname);
    if(cc == NULL)
    {
        log("%s: failed to get country code for <%s>\n", __func__, ifname);
        return -3;
    }
    strncpy(country, cc, sizeof(country));

    // Collect stats, but not more often than every RADIO_STATS_PERIOD sec
    unsigned long cur_t = util_time(1);
    if(rinfo->num >= 0 &&
       rinfo->num < UTIL_ARRAY_SIZE(stats_time) &&
       stats_time[rinfo->num] <= cur_t &&
       wt_wlext_priv(ifname, "stat\0\0", stats, sizeof(stats)) > 0)
    {
        stats[sizeof(stats) - 1] = 0;
        util_cleanup_str(stats);
        extras_obj[0].val.s = stats;
        stats_time[rinfo->num] = cur_t + RADIO_STATS_PERIOD;
    } else {
        extras_obj[0].val.s = NULL;
    }

    // Set the ptr to the extras object
    rinfo->extras = &(extras_obj[0]);

    // Enumerate the VAPs active on the radio
    rinfo->num_vaps = wt_wlext_enum_vaps(rinfo->num, rinfo->vaps);
    // There should be at least one
    if(rinfo->num_vaps <= 0) {
        log("%s: error, no VAPs on radio <%s>\n", __func__, ifname);
        return -3;
    }

    // This is conditional since some MTK drivers do it per radio
#ifndef WT_QUERY_STAS_FOR_EACH_VAP
    // Collect STAs info for the radio
    rinfo->num_stas = wt_rt_get_stas_info(rinfo->num, ifname);
    if(rinfo->num_stas < 0) {
        log("%s: failed to get STAs for <%s>\n", __func__, ifname);
        return -4;
    }
#endif // !WT_QUERY_STAS_FOR_EACH_VAP

    return 0;
}

// Capture the VAP info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_vap_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                         WT_JSON_TPL_VAP_STATE_t *vinfo)
{
    char *ifname = vinfo->ifname;
    char buf[IW_ESSID_MAX_SIZE];
    int essid_len;
    int client = FALSE;

    // We only use "ra*" and "apcli*" interfaces, besides that
    // fiter out those that are not UP
    client = (strncmp(ifname, "apcli", 5) == 0);
    if((strncmp(ifname, WIFI_RADIO_24_PREFIX, 2) != 0 && !client) ||
       !util_net_dev_is_up(ifname))
    {
        return 1;
    }

    // Collect the info, some of it is different for the client vs ap
    if(client) {
        // Override default mode (which is "ap") to "sta"
        strncpy(vinfo->mode, WIRELESS_OPMODE_STA, sizeof(vinfo->mode));

        // Get MAC address of the interface
        unsigned char mac[6];
        if(util_get_mac(ifname, mac) < 0) {
            log("%s: error getting MAC address of %s\n", __func__, ifname);
            return -1;
        }
        snprintf(vinfo->mac, sizeof(vinfo->mac),
                 MAC_PRINTF_FMT_TPL, MAC_PRINTF_ARG_TPL(mac));
    }
    // For AP interface get its BSS ID (for STA it's in the assoc_info)
    else if(wt_wlext_get_bssid(ifname, NULL, vinfo->bssid) != 0)
    {
        log("%s: failed to get VAP BSSID, skipping the VAP\n", __func__);
        return -2;
    }

    if(wt_wlext_get_essid(ifname, buf, &essid_len) != 0) {
        log("%s: failed to get VAP SSID, skipping the VAP\n", __func__);
        return -3;
    }
    if(wt_mk_hex_ssid(vinfo->ssid, buf, essid_len) != 0)
    {
        log("%s: invalid SSID for %s\n", __func__, ifname);
        return -4;
    }

    // If client, get the association info, otherwise STAs info
    if(client && wt_rt_get_assoc_info != NULL) {
        vinfo->num_assocs = wt_rt_get_assoc_info(rinfo->num, ifname);
        if(vinfo->num_assocs < 0) {
            log("%s: failed to get association info for <%s>\n",
                __func__, ifname);
            return -5;
        }
    } else {
        // This is conditional since some MTK drivers do it per radio
#ifdef WT_QUERY_STAS_FOR_EACH_VAP
        // Collect STAs info for the VAP
        vinfo->num_assocs = wt_rt_get_stas_info(rinfo->num, ifname);
        if(vinfo->num_assocs < 0) {
            log("%s: failed to get STAs for <%s>\n", __func__, ifname);
            return -6;
        }
#else // WT_QUERY_STAS_FOR_EACH_VAP
        // Save the total # of STAs (for C2 it is per radio, but it correctly
        // reflects the # of entries in the MAC table we have captured and
        // we are going to iterate through all of them skipping ones that
        // do not belong to VAP being reported.
        vinfo->num_assocs = rinfo->num_stas;
#endif // WT_QUERY_STAS_FOR_EACH_VAP
    }

    // No per-VAP extras object for Mediatek yet
    vinfo->extras = NULL;

    return 0;
}
