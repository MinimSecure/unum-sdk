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

// unum platform code for collecting wireless radios info

#include "unum.h"

// Include declarations for platform driver specific code
// we share locally.
#include "wireless_platform.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Return the name of the radio
char *wt_get_radio_name(int ii)
{
    return wt_iwinfo_get_phy(ii);
}

// Returns the index of the radio the interface belongs to
// (0, 1, ...), -1 if not a wireless interface
int wt_get_ifname_radio_num(char *ifname)
{
    return wt_iwinfo_get_ifname_phy_num(ifname);
}

// Capture the radio info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_radio_info(WT_JSON_TPL_RADIO_STATE_t *rinfo)
{
    char *phyname = rinfo->name;
    // radio extras values and template
    static char mode[32];   // The name of the hardware mode (11a, 11bg, ...)
    static char country[4]; // 3-digit country code
    static JSON_OBJ_TPL_t extras_obj = {
      // HW mode should be at [0]
      { "hwmode", { .type = JSON_VAL_STR, .s = mode }},
      { "country", { .type = JSON_VAL_STR, .s  = country }},
      { NULL }
    };

    // Get the radio channel
    rinfo->chan = wt_iwinfo_get_channel(phyname);
    if(rinfo->chan <= 0) {
        log("%s: failed to get channel for <%s>\n", __func__, phyname);
        return -2;
    }

    // Get radio hardware mode
    char *hw_mode = wt_iwinfo_get_hwmode(phyname);
    if(!hw_mode) {
        log_dbg("%s: failed to get HW mode for <%s>\n", __func__, phyname);
        extras_obj[0].val.s = NULL;
    } else {
        strncpy(mode, hw_mode, sizeof(mode));
        extras_obj[0].val.s = mode;
    }

    // Get the country code
    char *cc = wt_iwinfo_get_country(phyname);
    if(cc == NULL || *cc == 0)
    {
        log_dbg("%s: failed to get country for <%s>, using 'US'\n",
                __func__, phyname);
        cc = "US";
    }
    memset(country, 0, sizeof(country));
    strncpy(country, cc, sizeof(country) - 1);

    // Set the ptr to the extras object (we always add country, so no need
    // to check if it is empty or not
    rinfo->extras = &(extras_obj[0]);

    // Find all interfaces we have on this phy
    int phyidx = wt_iwinfo_get_phy_num(phyname);
    if(phyidx < 0) {
        log("%s: invalid phy name <%s>\n", __func__, phyname);
        return -3;
    }
    int count = 0;
    int idx = -1;
    while((idx = wt_iwinfo_get_if_num_for_phy(phyidx, idx)) >= 0)
    {
        char *ifname = wt_iwinfo_get_ifname(idx);
        if(ifname == NULL) {
            continue;
        }
        // Skip the interface if not in the AP mode
        int id;
        char *mode = wt_iwinfo_get_mode(ifname, &id);
        if(mode == NULL || id != IWINFO_OPMODE_MASTER) {
            log_dbg("%s: skipping <%s> on <%s>, in <%s> mode\n",
                    __func__, ifname, phyname, (mode ? mode : "unknown"));
            continue;
        }
        // Note: the whole rinfo structure is zeroed before this function call
        strncpy(rinfo->vaps[count], ifname, IFNAMSIZ - 1);
        ++count;
    }
    // Do not report if no interfaces serve as VAPs
    if(count <= 0) {
        log("%s: error, no interfaces on <%s>\n", __func__, phyname);
        return -4;
    }
    rinfo->num_vaps = count;

    return 0;
}

// Capture the VAP info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_vap_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                         WT_JSON_TPL_VAP_STATE_t *vinfo)
{
    char *ifname = vinfo->ifname;
    char buf[IWINFO_ESSID_MAX_SIZE];
    int essid_len;
    static int max_txpower;
    static int txpower_offset;
    static int noise;
    static JSON_OBJ_TPL_t extras_obj = {
      // txpower should be at [0], noise at [1], txpower offset at [2]
      { "max_txpower", { .type = JSON_VAL_PINT, .pi = &max_txpower }},
      { "noise", { .type = JSON_VAL_PINT, .pi  = &noise }},
      { "txpower_offset", { .type = JSON_VAL_PINT, .pi = &txpower_offset }},
      { NULL }
    };

    // We already filtered all but AP interfaces when collected info
    // for the radio, just make sure the interface being reported is UP
    if(!util_net_dev_is_up(ifname)) {
        return 1;
    }

    if(wt_iwinfo_get_bssid(ifname, NULL, vinfo->bssid) != 0) {
        log("%s: failed to get VAP BSSID, skipping %s\n", __func__, ifname);
        return -1;
    }

    if(wt_iwinfo_get_essid(ifname, buf, &essid_len) != 0) {
        log("%s: failed to get VAP SSID, skipping %s\n", __func__, ifname);
        return -2;
    }
    if(wt_mk_hex_ssid(vinfo->ssid, buf, essid_len) != 0)
    {
        log("%s: invalid SSID for %s\n", __func__, ifname);
        return -3;
    }

    int extras_count = 0;

    max_txpower = wt_iwinfo_get_max_txpwr(ifname, TRUE);
    if(max_txpower < 0) {
        extras_obj[0].val.pi = NULL;
    } else {
        extras_obj[0].val.pi = &max_txpower;
        ++extras_count;
    }

    if(wt_iwinfo_get_noise(ifname, &noise) < 0) {
        extras_obj[1].val.pi = NULL;
    } else {
        extras_obj[1].val.pi = &noise;
        ++extras_count;
    }

    if(wt_iwinfo_get_txpwr_offset(ifname, &txpower_offset) < 0) {
        extras_obj[2].val.pi = NULL;
    } else {
        extras_obj[2].val.pi = &txpower_offset;
        ++extras_count;
    }

    // If there is any extra info add the object
    if(extras_count > 0) {
        vinfo->extras = &(extras_obj[0]);
    }

    // Collect STAs info for the interface
    vinfo->num_assocs = wt_rt_get_stas_info(rinfo->num, ifname);
    if(vinfo->num_assocs < 0) {
        log("%s: failed to get STAs for <%s>\n", __func__, ifname);
        return -3;
    }

    return 0;
}
