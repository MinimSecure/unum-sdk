// (c) 2019 minim.co
// unum platform code for collecting wireless radios info
// includes Broadcom definitions as follows:
/*
 * Custom OID/ioctl definitions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Definitions subject to change without notice.
 *
 * Copyright (C) 2016, Broadcom Corporation. All Rights Reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: wlioctl_defs.h 403826 2013-05-22 16:40:55Z $
 */

#include "unum.h"

// Include declarations for platform driver specific code
// we share locally.
#include "wireless_platform.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Capture the radio info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_radio_info(WT_JSON_TPL_RADIO_STATE_t *rinfo)
{
    int ret, intval, ii;
    char *ifname = rinfo->name;
    static int width;
    static int kind;
    static int noise;
    static char country[4];
    static char band_str[8];
    static char spec_str[CHANSPEC_STR_LEN];
    static unsigned long stats_time[WIRELESS_MAX_RADIOS];
    static char counters_str[WT_COUNTERS_SIZE];
    // Populate extras
    static JSON_OBJ_TPL_t extras_obj = { // stats must be first
      { "stats",   { .type = JSON_VAL_STR,  {.s  = NULL    }}},
      { "band",    { .type = JSON_VAL_STR,  {.s  = band_str}}},
      { "width",   { .type = JSON_VAL_PINT, {.pi = &width  }}},
      { "kind",    { .type = JSON_VAL_PINT, {.pi = &kind   }}},
      { "cspec",   { .type = JSON_VAL_STR,  {.s  = spec_str}}},
      { "noise",   { .type = JSON_VAL_PINT, {.pi = &noise  }}},
      { "country", { .type = JSON_VAL_STR,  {.s  = country }}},
      { NULL }
    };

    // If radio is disabled, skip it
    // BRCM does not bring the interface down, just turs off the radio
    ret = wl_ioctl(ifname, WLC_GET_RADIO, &intval, sizeof(intval));
    if(ret < 0)
    {
        log("%s: radio interface %s is not supported\n", __func__, ifname);
        return -1;
    }
    // Ignoring the MPC (minimum power consumption) mode bit
    if((intval & ~(WL_RADIO_MPC_DISABLE)) != 0)
    {
        log_dbg("%s: the radio is disabled, status 0x%x\n",
                __func__, intval);
        return 1; // just skip the interface, no error
    }

    // Buffer for IOCTL calls
    void *wlcval = wt_get_ioctl_tmp_buf();
    if(wlcval == NULL) {
        log("%s: no temporary buffer for IOCTLs\n", __func__);
        return -2;
    }

    // Get the BSS info data for the primary VAP interface
    *(unsigned int *)wlcval = htod32(WLC_IOCTL_MAXLEN);
    ret = wl_ioctl(ifname, WLC_GET_BSS_INFO, wlcval, WLC_IOCTL_MAXLEN);
    if(ret < 0)
    {
        log("%s: WLC_GET_BSS_INFO for %s error %d\n", __func__, ifname, errno);
        return -3;
    }
    wl_bss_info_t *bi = (wl_bss_info_t *)(wlcval + 4);
    if(WL_BSS_INFO_VERSION != dtoh32(bi->version)) {
        log("%s: %s WLC_GET_BSS_INFO version %d, need %d\n",
            __func__, ifname, dtoh32(bi->version), WL_BSS_INFO_VERSION);
        return -4;
    }

    // Get the radio channel and related info
    chanspec_t chspec;
    ret = wl_chanspec_from_bssinfo(bi, &chspec);
    if(ret < 0)
    {
        log("%s: invalid chanspec for %s\n", __func__, ifname);
        return -5;
    }
    // Extract what we can from the retrieved chspec band
    if((CHSPEC_IS2G(chspec) && CHSPEC_CHANNEL(chspec) > CH_MAX_2G_CHANNEL) ||
       (CHSPEC_IS5G(chspec) && CHSPEC_CHANNEL(chspec) <= CH_MAX_2G_CHANNEL))
    {
        strncpy(band_str, "unknown", sizeof(band_str));
    } else {
        strncpy(band_str, ((CHSPEC_IS2G(chspec))?"2":"5"), sizeof(band_str)-1);
    }
    band_str[sizeof(band_str)-1] = '\0';
    // control/main channel
    rinfo->chan = wf_chspec_ctlchan(chspec);
    // width and spec info in Broadcom's format
    width = wl_get_specstr_and_width(chspec, spec_str);

    if(util_get_radio_kind != NULL) {
        kind = util_get_radio_kind(ifname);
    } else {
        kind = -1; // Not supported yet on this platform
    }

    // Get noise level
    noise = bi->phy_noise;

    // Get country setting
    wl_country_t cspec;
    memset(&cspec, 0, sizeof(cspec));
    ret = wl_iovar_get(ifname, "country", &cspec, sizeof(cspec));
    if(ret < 0)
    {
        log("%s: country info for %s is not avaialble\n", __func__, ifname);
        return -6;
    }
    strncpy(country, cspec.country_abbrev, sizeof(country));

    // Collect stats, but not more often than every RADIO_STATS_PERIOD sec
    unsigned long cur_t = util_time(1);
    extras_obj[0].val.s = NULL;
    while(cur_t >= stats_time[rinfo->num])
    {
        // Get stats (requires WLC_IOCTL_MEDLEN, but we already
        // have WLC_IOCTL_MAXLEN buffer available).
        // We can get them efficiently with
        // memset(wlcval, 0, WLC_IOCTL_MEDLEN);
        // ret = wl_iovar_get(ifname, "counters", wlcval, WLC_IOCTL_MEDLEN);
        // but they come in TLVs and parsing those is a headache.
        // At least for now going an easy way - capturing wl command output
        // (do not do this for something that is called often or can be done
        // properly)
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "%s -i %s counters", BCM_WL_PATH, ifname);
        if(util_get_cmd_output(cmd, counters_str,
                               sizeof(counters_str), NULL) > 0)
        {
            extras_obj[0].val.s = counters_str;
            stats_time[rinfo->num] = cur_t + RADIO_STATS_PERIOD;
        }
        break;
    }

    // Set the ptr to the extras object
    rinfo->extras = &(extras_obj[0]);

    // Populate the VAP names and count
    for(ii = 0; ii < WIRELESS_MAX_VAPS; ++ii) {
        char *vifname = wt_get_vap_name(rinfo->num, ii);
        if(vifname == NULL) {
            break;
        }
        strncpy(rinfo->vaps[ii], vifname, sizeof(rinfo->vaps[ii]));
        rinfo->vaps[ii][sizeof(rinfo->vaps[ii]) - 1] = 0;
    }
    // How many VAPs we will try to report for the radio
    rinfo->num_vaps = ii;

    return 0;
}

// Capture the VAP info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_vap_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                         WT_JSON_TPL_VAP_STATE_t *vinfo)
{
    char *ifname = vinfo->ifname;
    int ii, ret;

    // Find out if the BSS is set up
    if(if_nametoindex(ifname) == 0) {
        return 1;
    }

    // Buffer for IOCTL calls
    void *wlcval = wt_get_ioctl_tmp_buf();
    if(wlcval == NULL) {
        log("%s: no temporary buffer for IOCTLs\n", __func__);
        return -1;
    }

    // figure out what mode the interface is in
    int ap, infra;
    ret = wl_ioctl(ifname, WLC_GET_AP, &ap, sizeof(ap));
    if(ret < 0) {
        log("%s: error getting WLC_GET_AP for %s\n", __func__, ifname);
        return -2;
    }
    ap = dtoh32(ap);
    ret = wl_ioctl(ifname, WLC_GET_INFRA, &infra, sizeof(infra));
    if(ret < 0) {
        log("%s: error getting WLC_GET_INFRA for %s\n", __func__, ifname);
        return -3;
    }
    infra = dtoh32(infra);
    // Detect modes of operation similarly to iwinfo for Broadcom.
    // We only care for AP and STA for now though.
    if(!infra) {
        log_dbg("%s: %s is in adhoc mode\n", __func__, ifname);
        return 3;
    } else if(ap) {
        strncpy(vinfo->mode, WIRELESS_OPMODE_AP, sizeof(vinfo->mode));
    } else {
        strncpy(vinfo->mode, WIRELESS_OPMODE_STA, sizeof(vinfo->mode));
    }

    // Get operating mode if it is mesh backhaul interface
    memset(wlcval, 0, WLC_IOCTL_MAXLEN);
    char *var = "map";
    ret = wl_iovar_get(ifname, var, wlcval, WLC_IOCTL_MAXLEN);
    if(ret < 0)
    {
        log("%s: Error while getting map on %s\n", __func__, ifname);
        // Not returning error for now
    } else {
        char apmode = ((char *)wlcval)[0];
        if (apmode == 4) {
            // Broadcom mesh backhaul station
            strncpy(vinfo->mode, WIRELESS_OPMODE_MESHSTA_BCM, sizeof(vinfo->mode));
        } else if (apmode == 2) {
            // Broadcom mesh backhaul AP
            strncpy(vinfo->mode, WIRELESS_OPMODE_MESHAP_BCM, sizeof(vinfo->mode));
        }
        // We are not handling all other values for now
    }

    if(util_get_interface_kind != NULL) {
        vinfo->kind = util_get_interface_kind(ifname);
    } else {
        vinfo->kind = -1; // Not supported yet on this platform
    }

    // Find out if BSS is enabled (for AP mode) up/down (for STA mode) 
    int bsscfg_idx = ~0;
    ret = wl_iovar_getbuf(ifname, "bss", &bsscfg_idx, sizeof(bsscfg_idx),
                          wlcval, WLC_IOCTL_MAXLEN);
    if(ret < 0) {
        log("%s: error getting bss status for %s\n", __func__, ifname);
        return -5;
    }
    int bss_up = dtoh32(*(int *)wlcval);
    if(ap && !bss_up) {
        log_dbg("%s: %s VAP is disabled\n", __func__, ifname);
        return 4;
    }

    // Get MAC address of the interface (only used in STA mode)
    unsigned char mac[6];
    ret = util_get_mac(ifname, mac);
    if(ret < 0) {
        log("%s: error getting MAC address of %s\n", __func__, ifname);
        return -6;
    }
    snprintf(vinfo->mac, sizeof(vinfo->mac),
             MAC_PRINTF_FMT_TPL, MAC_PRINTF_ARG_TPL(mac));

    // Get the BSSID and SSID of the VAP or associated BSSID/SSID of the
    // STA interface, the STA assoc info is only availble if associated.
    if(!bss_up)
    {
        *(vinfo->bssid) = 0;
        *(vinfo->ssid) = 0;
        vinfo->extras = NULL;
        return 0;
    }

    ret = wl_ioctl(ifname, WLC_GET_BSSID, wlcval, ETHER_ADDR_LEN);
    if(ret < 0)
    {
        log("%s: WLC_GET_BSSID for %s error %d\n", __func__, ifname, errno);
        return -7;
    }
    snprintf(vinfo->bssid, sizeof(vinfo->bssid),
             MAC_PRINTF_FMT_TPL, MAC_PRINTF_ARG_TPL(wlcval));

    ret = wl_ioctl(ifname, WLC_GET_SSID, wlcval, WLC_IOCTL_MAXLEN);
    if(ret < 0)
    {
        log("%s: WLC_GET_SSID for %s error %d\n", __func__, ifname, errno);
        return -8;
    }
    wlc_ssid_t *wlssid = (wlc_ssid_t *)wlcval;
    if(wt_mk_hex_ssid(vinfo->ssid, wlssid->SSID,
                      dtoh32(wlssid->SSID_len)) != 0)
    {
        log("%s: invalid SSID for %s\n", __func__, ifname);
        return -9;
    }

    // Flag to track if we correctly populated the extras
    int extras_ready = TRUE;

    // Template array for max txpower
    static JSON_VAL_TPL_t max_txpower[WIRELESS_RADIO_CORES + 1] = {
        [0 ... (WIRELESS_RADIO_CORES - 1)] = { .type = JSON_VAL_INT },
        [ WIRELESS_RADIO_CORES ] = { .type = JSON_VAL_END }
    };
    // Get values and fill in the txpower arrays
    memset(wlcval, 0, sizeof(txpwr_target_max_t));
    var = "txpwr_target_max";
    ret = wl_iovar_get(ifname, var, wlcval, sizeof(txpwr_target_max_t));
    if(ret < 0)
    {
        log("%s: %s has failed for %s\n", __func__, var, ifname);
        extras_ready = FALSE;
    } else {
        txpwr_target_max_t *txmax = (txpwr_target_max_t *)wlcval;
        for(ii = 0; ii < WIRELESS_RADIO_CORES; ii++) {
            max_txpower[ii].i = (txmax->txpwr[ii] + 2) / 4;
        }
    }

    // Template array for txpower offset
    static JSON_VAL_TPL_t txpower_off[WIRELESS_RADIO_CORES + 1] = {
        [0 ... (WIRELESS_RADIO_CORES - 1)] = { .type = JSON_VAL_INT },
        [ WIRELESS_RADIO_CORES ] = { .type = JSON_VAL_END }
    };
    memset(wlcval, 0, sizeof(wl_txchain_pwr_offsets_t));
    var = "txchain_pwr_offset";
    ret = wl_iovar_get(ifname, var, wlcval, sizeof(wl_txchain_pwr_offsets_t));
    if(ret < 0)
    {
        log("%s: %s has failed for %s\n", __func__, var, ifname);
        extras_ready = FALSE;
    } else {
        wl_txchain_pwr_offsets_t *off = (wl_txchain_pwr_offsets_t *)wlcval;
        for(ii = 0; ii < WIRELESS_RADIO_CORES; ii++) {
            txpower_off[ii].i = (off->offset[ii] + 2) / 4;
        }
    }

    // Fill in and store the VAP extras object ptr
    if(extras_ready) {
        static JSON_OBJ_TPL_t extras_obj = {
          { "max_txpower",    { .type = JSON_VAL_ARRAY, {.a = max_txpower} }},
          { "txpower_offset", { .type = JSON_VAL_ARRAY, {.a = txpower_off} }},
          { NULL }
        };
        vinfo->extras = extras_obj;
    } else {
        vinfo->extras = NULL;
    }

    // Collect associations info for AP (list of associated STAs) or
    // STA (its own association) interface
    if(ap && wt_rt_get_stas_info != NULL) {
        vinfo->num_assocs = wt_rt_get_stas_info(rinfo->num, ifname);
    } else if(wt_rt_get_assoc_info != NULL) {
        vinfo->num_assocs = wt_rt_get_assoc_info(rinfo->num, ifname);
    } else {
        log("%s: no API to get associations info for <%s>\n", __func__, ifname);
        return -10;;
    }
    if(vinfo->num_assocs < 0) {
        log("%s: failed to get associations info for <%s>\n", __func__, ifname);
        return -11;
    }

    return 0;
}
