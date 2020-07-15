// (c) 2018-2020 minim.co
// unum platform code for collecting wireless clients info

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


// Buffer for STA info, once allocated it is never freed.
// The same buffer is used for all radios and/or VAPs.
// Old drivers reports clients for all VAPs at the same time.
// WT_QUERY_STAS_FOR_EACH_VAP define controll how we query. If not
// defined the buffer is populated while processing the radios and
// while processing the VAPs we pull only the entries matching.
static void *sta_info_buf = NULL;


// Capture the STAs info for a radio (C2 gets it for all radio VAPs)
// Returns: # of STAs - if successful, negative error otherwise
int wt_rt_get_stas_info(int radio_num, char *ifname)
{
    // If we do not yet have a buffer, allocate it (never freed)
    if(sta_info_buf == NULL) {
        sta_info_buf = UTIL_MALLOC(WT_STAS_MAC_TABLE_MAX_SIZE);
    }
    if(sta_info_buf == NULL) {
        log("%s: failed to allocate memory for STAs\n", __func__);
        return -1;
    }

    int len = wt_wlext_priv_direct(ifname, RTPRIV_IOCTL_GET_MAC_TABLE_STRUCT, 0,
                                   sta_info_buf, WT_STAS_MAC_TABLE_MAX_SIZE);

    if(len != WT_STAS_MAC_TABLE_SIZE(radio_num)) {
        log("%s: MAC table IOCTL for <%s> returned %d, expected %d\n",
            __func__, ifname, len, WT_STAS_MAC_TABLE_SIZE(radio_num));
        return -2;
    }

    // The total number of STAs is reported in the 4-byte integer at the
    // top of the buffer. Check to make sure it makes sense.
    int num_stas = *((int*)sta_info_buf);
    if(num_stas < 0 ||
       num_stas * WT_STAS_MAC_TABLE_ENTRY_SIZE(radio_num) > len - 4)
    {
        log("%s: invalid number of STAs %d for <%s>\n",
            __func__, num_stas, num_stas);
        return -3;
    }

    return num_stas;
}

// Capture the STA info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_sta_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                         WT_JSON_TPL_VAP_STATE_t *vinfo,
                         WT_JSON_TPL_STA_STATE_t *sinfo,
                         int ii)
{
    // STA extras template and values
    static char e_mode[8];
    static int e_ps;
    static int e_mcs;
    static int e_bw;
    static int e_short_gi;
    static int e_active;
    static JSON_OBJ_TPL_t extras_obj = {
      { "mode",     { .type = JSON_VAL_STR,  {.s = e_mode}}},
      { "ps",       { .type = JSON_VAL_PINT, {.pi = &e_ps}}},
      { "mcs",      { .type = JSON_VAL_PINT, {.pi = &e_mcs}}},
      { "bw",       { .type = JSON_VAL_PINT, {.pi = &e_bw}}},
      { "short_gi", { .type = JSON_VAL_PINT, {.pi = &e_short_gi}}},
      { "active",   { .type = JSON_VAL_PINT, {.pi = &e_active}}},
      { NULL }
    };

    // We must have the STA data if we are here
    if(sta_info_buf == NULL) {
        return -1;
    }

    int num_stas = *((int*)sta_info_buf);
    RT_802_11_MAC_ENTRY *prt = (RT_802_11_MAC_ENTRY *)((char*)sta_info_buf + 4);
    prt = (RT_802_11_MAC_ENTRY *)
              ((char *)prt + ii * WT_STAS_MAC_TABLE_ENTRY_SIZE(rinfo->num));

    // We already verified that the num_stas is valid when captured the STAs
    // info, just check that ii is in the range (it should never happen).
    if(ii < 0 || ii >= num_stas) {
        return -2;
    }

    // Check that the STA is connected to the VAP
    int vap_id = -1;
    if(strncmp(vinfo->ifname, WIFI_RADIO_5_PREFIX, 3) == 0) {
        vap_id = vinfo->ifname[3] - '0';
    } else if(strncmp(vinfo->ifname, WIFI_RADIO_24_PREFIX, 2) == 0) {
        vap_id = vinfo->ifname[2] - '0';
    }
    if(vap_id < 0 || vap_id > 3) {
        log("%s: failed to get VAP id from <%s>\n", __func__, vinfo->ifname);
        return -3;
    }

#ifndef WT_QUERY_STAS_FOR_EACH_VAP
    // If STA is not connected to the current VAP skip it
    if(prt->ApIdx != vap_id) {
        return 1; // just skip, no error
    }
#endif

    // Capture the common info
    sprintf(sinfo->mac, MAC_PRINTF_FMT_TPL, MAC_PRINTF_ARG_TPL(prt->Addr));
    int rssi = prt->AvgRssi0;
    if(rssi == 0 || (rssi < prt->AvgRssi1 && prt->AvgRssi1 != 0)) {
        rssi = prt->AvgRssi1;
    }
    if(rssi == 0 || (rssi < prt->AvgRssi2 && prt->AvgRssi2 != 0)) {
        rssi = prt->AvgRssi2;
    }
    sinfo->rssi = rssi;

    // Capture the extras
    static int bw_map[] = {20, 40, 80};
    e_bw = 20; // default
    static char *mode_str[] = {"cckm", "ofdm", "htmix", "gf", "vht"};
    int mode_id;
    if(rinfo->num == 0)  { // radio 0 (2.4GHz)
        mode_id = prt->TxRate.field24.MODE;
#if Rate_BW_20 != 0
        if(prt->TxRate.field24.BW >= Rate_BW_20 &&
           prt->TxRate.field24.BW <= Rate_BW_80)
#else  // Rate_BW_20 != 0
        if(prt->TxRate.field24.BW <= Rate_BW_80)
#endif // Rate_BW_20 != 0
        {
           e_bw = bw_map[prt->TxRate.field24.BW];
        }
        e_short_gi = prt->TxRate.field24.ShortGI;
        e_mcs = prt->TxRate.field24.MCS;
    } else { // radio 1 (2.4GHz)
        mode_id = prt->TxRate.field5.MODE;
#if Rate_BW_20 != 0
        if(prt->TxRate.field5.BW >= Rate_BW_20 &&
           prt->TxRate.field5.BW <= Rate_BW_80)
#else  // Rate_BW_20 != 0
        if(prt->TxRate.field5.BW <= Rate_BW_80)
#endif // Rate_BW_20 != 0
        {
           e_bw = bw_map[prt->TxRate.field5.BW];
        }
        e_short_gi = prt->TxRate.field5.ShortGI;
        e_mcs = prt->TxRate.field5.MCS;
    }
    if(mode_id >= MODE_CCK && mode_id <= MODE_VHT)
    {
        strcpy(e_mode, mode_str[mode_id]);
    } else {
        strcpy(e_mode, "n/a");
    }
    e_ps = (prt->Psm == 0) ? 0 : 1;
    e_active = prt->ConnectedTime;

    // Set the object template ptr
    sinfo->extras = &(extras_obj[0]);

    return 0;
}
