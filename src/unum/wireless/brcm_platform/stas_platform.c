// (c) 2017-2020 minim.co
// unum platform code for collecting wireless clients info
// contains some definitions from Broadcom as follows:
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


// Buffer for STA info, once allocated it is never freed.
// The same buffer is used for all VAPs.
// It is pretty big (24KB for v17.01.04)
static void *sta_info_buf = NULL;


// Capture the STAs info for an interface
// Returns: # of STAs - if successful, negative error otherwise
int wt_rt_get_stas_info(int radio_num, char *ifname)
{
    // If we do not yet have a buffer, allocate it (never freed)
    if(sta_info_buf == NULL) {
        sta_info_buf = UTIL_MALLOC(WLC_IOCTL_MAXLEN);
    }
    if(sta_info_buf == NULL) {
        log("%s: failed to allocate memory for STAs\n", __func__);
        return -1;
    }

    return wl_get_stations(ifname, sta_info_buf);
}

// Capture the STA info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_sta_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                         WT_JSON_TPL_VAP_STATE_t *vinfo,
                         WT_JSON_TPL_STA_STATE_t *sinfo,
                         int ii)
{
    char *ifname = vinfo->ifname;
    static sta_info_t si;
    static int authorized;
    static int avg_rssi;
    static int noise_fl;
    static char caps[160];
    static char ht_caps[80];
    static char vht_caps[160];
    static json_int_t tx_tot_bytes;
    static json_int_t rx_tot_bytes;
    static JSON_OBJ_TPL_t extras_obj = {
      { "idle_sec",  {.type = JSON_VAL_PUINT,{.pui = &si.idle    }}},
      { "assoc_sec", {.type = JSON_VAL_PUINT,{.pui = &si.in      }}},
      { "auth_ok",   {.type = JSON_VAL_PINT, {.pi  = &authorized }}},
      { "caps",      {.type = JSON_VAL_STR,  {.s   = &caps[1]    }}},
      { "ht_caps",   {.type = JSON_VAL_STR,  {.s   = &ht_caps[1] }}},
      { "vht_caps",  {.type = JSON_VAL_STR,  {.s   = &vht_caps[1]}}},
      { "avg_rssi",  {.type = JSON_VAL_PINT, {.pi  = &avg_rssi   }}},
      { "noise_fl",  {.type = JSON_VAL_PINT, {.pi  = &noise_fl   }}},
      { "tx_bytes",  {.type = JSON_VAL_PJINT,{.pji = &tx_tot_bytes       }}},
      { "rx_bytes",  {.type = JSON_VAL_PJINT,{.pji = &rx_tot_bytes       }}},
      { "tx_kbps",   {.type = JSON_VAL_PUINT,{.pui = &si.tx_rate         }}},
      { "rx_kbps",   {.type = JSON_VAL_PUINT,{.pui = &si.rx_rate         }}},
      { "tx_pkts",   {.type = JSON_VAL_PUINT,{.pui = &si.tx_tot_pkts     }}},
      { "tx_retr",   {.type = JSON_VAL_PUINT,{.pui = &si.tx_pkts_retried }}},
      { "tx_fail",   {.type = JSON_VAL_PUINT,{.pui = &si.tx_failures     }}},
      { "rx_pkts",   {.type = JSON_VAL_PUINT,{.pui = &si.rx_tot_pkts     }}},
      { "rx_retr",   {.type = JSON_VAL_PUINT,{.pui = &si.rx_pkts_retried }}},
      {"rx_dec_fail",{.type = JSON_VAL_PUINT,{.pui = &si.rx_decrypt_failures}}},
      { NULL }
    };

    // We must have the STA data if we are here
    if(sta_info_buf == NULL) {
        log("%s: no STA list buffer\n", __func__);
        return -1;
    }

    // Buffer for IOCTL calls
    void *wlcval = wt_get_ioctl_tmp_buf();
    if(wlcval == NULL) {
        log("%s: no temporary buffer for IOCTLs\n", __func__);
        return -2;
    }

    // Get MAC of the STA requested
    maclist_t *maclist = (struct maclist *)sta_info_buf;
    if(maclist->count < ii) {
        // We are out of the range (already checked, should never happen)
        log("%s: STA %d is out of range, there are %hu STAs\n",
            __func__, ii, maclist->count);
        return -3;
    }
    unsigned char *mac = (unsigned char *)&(maclist->ea[ii]);

    // Retrieve the STA information into wlcval buffer
    if(wl_get_sta_info(ifname, mac, wlcval) < 0) {
        log_dbg("%s: %s STA info for "
                MAC_PRINTF_FMT_TPL " is not available\n",
                __func__, ifname, MAC_PRINTF_ARG_TPL(mac));
        return 1;
    }

    // Populate static STA info data buffer
    memcpy(&si, wlcval, sizeof(si));

    // Auth state, other state bitflags WL_STA_AUTHE, WL_STA_ASSOC
    authorized = (si.flags & WL_STA_AUTHO) ? 1 : 0;

    // Only report authorized stations.
    if(!authorized) {
        log_dbg("%s: %s: skipping unauthorized STA "
                MAC_PRINTF_FMT_TPL "\n",
                __func__, ifname, MAC_PRINTF_ARG_TPL(mac));
        return 2;
    }

    // Make sure we have extended station info and stats
    // (we require rssi and it is only available in the stats)
    if(si.len < sizeof(sta_info_t)) {
        log("%s: %s: no extended STA info for "
            MAC_PRINTF_FMT_TPL ", skipping\n",
            __func__, ifname, MAC_PRINTF_ARG_TPL(mac));
        return 3;
    }
    if((si.flags & WL_STA_SCBSTATS) == 0) {
        log("%s: %s: no STA stats for "
            MAC_PRINTF_FMT_TPL ", skipping\n",
            __func__, ifname, MAC_PRINTF_ARG_TPL(mac));
        return 4;
    }

    // Capture the common info
    sprintf(sinfo->mac, MAC_PRINTF_FMT_TPL, MAC_PRINTF_ARG_TPL(mac));
    // Find max rssi for the last pkt received
    int i, rssi, rssi_avg;
    noise_fl = rssi = rssi_avg = -101;
    for(i = WL_ANT_IDX_1; i < WL_RSSI_ANT_MAX; i++)
    {
	    if(si.rx_lastpkt_rssi[i] < 0 && rssi < si.rx_lastpkt_rssi[i]) {
            rssi = si.rx_lastpkt_rssi[i];
        }
	    if(si.rssi[i] < 0 && rssi_avg < si.rssi[i]) {
            rssi_avg = si.rssi[i];
        }
	    if(si.nf[i] < 0 && noise_fl < si.nf[i]) {
            noise_fl = si.nf[i];
        }
    }
    sinfo->rssi = rssi;
    avg_rssi = rssi_avg; // this one is extras, not common info

    // Capabilities bitflags
    caps[0] = caps[1] = 0; // no way it could be empty, but just in case...
    snprintf(caps, sizeof(caps), "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
	         (si.flags & WL_STA_BRCM)      ? " BRCM"    : "",
	         (si.flags & WL_STA_WME)       ? " WME"     : "",
	         (si.flags & WL_STA_PS)        ? " PS"      : "",
	         (si.flags & WL_STA_NONERP)    ? " No-ERP"  : "",
	         (si.flags & WL_STA_APSD_BE)   ? " APSD_BE" : "",
	         (si.flags & WL_STA_APSD_BK)   ? " APSD_BK" : "",
	         (si.flags & WL_STA_APSD_VI)   ? " APSD_VI" : "",
	         (si.flags & WL_STA_APSD_VO)   ? " APSD_VO" : "",
	         (si.flags & WL_STA_N_CAP)     ? " N_CAP"   : "",
	         (si.flags & WL_STA_VHT_CAP)   ? " VHT_CAP" : "",
	         (si.flags & WL_STA_AMPDU_CAP) ? " AMPDU"   : "",
	         (si.flags & WL_STA_AMSDU_CAP) ? " AMSDU"   : "",
	         (si.flags & WL_STA_MIMO_PS)   ? " MIMO-PS" : "",
	         (si.flags & WL_STA_MIMO_RTS)  ? " MIMO-PS-RTS" : "",
	         (si.flags & WL_STA_RIFS_CAP)  ? " RIFS"    : "",
	         (si.flags & WL_STA_DWDS_CAP)  ? " DWDS_CAP": "",
	         (si.flags & WL_STA_DWDS)      ? " DWDS_ACTIVE" : "");
    // Hight Throughput Capabilities (11n)
    ht_caps[0] = ht_caps[1] = 0;
    if(si.flags & (WL_STA_N_CAP | WL_STA_VHT_CAP))
    {
        snprintf(ht_caps, sizeof(ht_caps), "%s%s%s%s%s%s%s%s%s",
	             (si.ht_capabilities & WL_STA_CAP_LDPC_CODING) ? " LDPC":"",
	             (si.ht_capabilities & WL_STA_CAP_40MHZ)       ? " 40MHz":"",
	             (si.ht_capabilities & WL_STA_CAP_GF)          ? " GF":"",
	             (si.ht_capabilities & WL_STA_CAP_SHORT_GI_20) ? " SGI20":"",
	             (si.ht_capabilities & WL_STA_CAP_SHORT_GI_40) ? " SGI40":"",
	             (si.ht_capabilities & WL_STA_CAP_TX_STBC)     ? " STBC-Tx":"",
	             (si.ht_capabilities & WL_STA_CAP_RX_STBC_MASK)? " STBC-Rx":"",
	             (si.ht_capabilities & WL_STA_CAP_DELAYED_BA)  ? " D-BlkAck":"",
	             (si.ht_capabilities &
                  WL_STA_CAP_40MHZ_INTOLERANT)                 ? " 40-Intl":"");
    }
    // Very Hight Throughput Capabilities (11ac)
    vht_caps[0] = vht_caps[1] = 0;
    if(si.flags & WL_STA_VHT_CAP)
    {
        snprintf(vht_caps, sizeof(vht_caps), "%s%s%s%s%s%s%s%s%s%s%s",
			     (si.vht_flags & WL_STA_VHT_LDPCCAP)    ? " LDPC" : "",
			     (si.vht_flags & WL_STA_SGI80)          ? " SGI80" : "",
			     (si.vht_flags & WL_STA_SGI160)         ? " SGI160" : "",
			     (si.vht_flags & WL_STA_VHT_TX_STBCCAP) ? " STBC-Tx" : "",
			     (si.vht_flags & WL_STA_VHT_RX_STBCCAP) ? " STBC-Rx" : "",
			     (si.vht_flags & WL_STA_SU_BEAMFORMER)  ? " SU-BFR" : "",
			     (si.vht_flags & WL_STA_SU_BEAMFORMEE)  ? " SU-BFE" : "",
			     (si.vht_flags & WL_STA_MU_BEAMFORMER)  ? " MU-BFR" : "",
			     (si.vht_flags & WL_STA_MU_BEAMFORMEE)  ? " MU-BFE" : "",
			     (si.vht_flags & WL_STA_VHT_TXOP_PS)    ? " TXOPPS" : "",
			     (si.vht_flags & WL_STA_HTC_VHT_CAP)    ? " VHT-HTC" : "");
    }

    // Byte counters that have to be converted into json_int_t
    tx_tot_bytes = (json_int_t)si.tx_tot_bytes;
    rx_tot_bytes = (json_int_t)si.rx_tot_bytes;

    // Report extras
    sinfo->extras = &(extras_obj[0]);

    return 0;
}
