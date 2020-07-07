// (c) 2018-2020 minim.co
// unum wl driver APIs, used only by platforms that are based on
// on Broadcom wl drivers and can access APIs in BCM's libshared
// Note: wt_wl_mk_if_list() must be called before the functions
//       capable of resolving phys/interfaces/indices can be used

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Vars wl sets at init, for now requirements and IOCTL version
// must be the same for all radios.
int wl_need_byte_swap = FALSE;
int wl_ioctl_version = 0;
static int wl_initialized = FALSE;

// List of platform radios indexed by the radio number
static char *radio_list[WIRELESS_MAX_RADIOS] = PLATFRM_RADIO_IFS;

// List of platform VAP interfaces for each radio number
// (2D array, indexed by radio #, then VAP #)
static char *vap_list[WIRELESS_MAX_RADIOS][WIRELESS_MAX_VAPS] = PLATFRM_VAP_IFS;

// Pointer to wireless telemetry temp buffer. Should only be used in the
// wireless telemetry main thread.It is of WLC_IOCTL_MAXLEN, so can be used
// in all IOCTLs.
static void *wlcval = NULL;


// ASCII strings (from bcmwifi_channels.c) for chanspec printout
/*
 * Misc utility routines used by kernel or app-level.
 * Contents are wifi-specific, used by any kernel or app-level
 * software that might want wifi things as it grows.
 *
 * Copyright (C) 2018, Broadcom Corporation. All Rights Reserved.
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
 * $Id: bcmwifi_channels.c 309193 2012-01-19 00:03:57Z $
 */

// Bandwidth
static const char *wf_chspec_bw_str[] = {
    "5",
    "10",
    "20",
    "40",
    "80",
    "160",
    "80+80",
    "na"
};
static const uint8 wf_chspec_bw_mhz[] = {
    5, 10, 20, 40, 80, 160, 160
};
#define WF_NUM_BW (sizeof(wf_chspec_bw_mhz)/sizeof(uint8))

// 80MHz channels in 5GHz band
static const uint8 wf_5g_80m_chans[] = {
    42, 58, 106, 122, 138, 155
};
#define WF_NUM_5G_80M_CHANS \
        (sizeof(wf_5g_80m_chans)/sizeof(uint8))



#ifdef WIRELESS_WL_NO_LIBSHARED
// WIRELESS_WL_NO_LIBSHARED enables inclusion of various Broadcom
// routines that are typically accessed by linking with libshared.so
// Some of the platforms compile them in directly so we have to
// add the code to our sources to be able accessing the routines.
// Broadcom licensing:
/*
 * Misc utility routines used by kernel or app-level.
 * Contents are wifi-specific, used by any kernel or app-level
 * software that might want wifi things as it grows.
 *
 * Copyright (C) 2018, Broadcom Corporation. All Rights Reserved.
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
 * $Id: bcmwifi_channels.c 309193 2012-01-19 00:03:57Z $
 */

// convert bandwidth from chanspec to MHz
static uint bw_chspec_to_mhz(chanspec_t chspec)
{
    unsigned int bw;
    bw = (chspec & WL_CHANSPEC_BW_MASK) >> WL_CHANSPEC_BW_SHIFT;
    return (bw >= WF_NUM_BW ? 0 : wf_chspec_bw_mhz[bw]);
}

// bw in MHz, return the channel count from the center channel to the
// the channel at the edge of the band
static uint8 center_chan_to_edge(uint bw)
{
    // edge channels separated by BW - 10MHz on each side
    // delta from cf to edge is half of that,
    // MHz to channel num conversion is 5MHz/channel
    return (uint8)(((bw - 20) / 2) / 5);
}

// return channel number of the low edge of the band
// given the center channel and BW
static uint8 channel_low_edge(uint center_ch, uint bw)
{
    return (uint8)(center_ch - center_chan_to_edge(bw));
}

// return control channel given center channel and side band
static uint8 channel_to_ctl_chan(uint center_ch, uint bw, uint sb)
{
    return (uint8)(channel_low_edge(center_ch, bw) + sb * 4);
}

// Verify the chanspec is using a legal set of parameters, i.e. that the
// chanspec specified a band, bw, ctl_sb and channel and that the
// combination could be legal given any set of circumstances.
// RETURNS: TRUE is the chanspec is malformed, false if it looks good.
bool wf_chspec_malformed(chanspec_t chanspec)
{
    uint chspec_bw = CHSPEC_BW(chanspec);
    uint chspec_ch = CHSPEC_CHANNEL(chanspec);

    // must be 2G or 5G band
    if(CHSPEC_IS2G(chanspec))
    {
        // must be valid bandwidth
        if(chspec_bw != WL_CHANSPEC_BW_20 && chspec_bw != WL_CHANSPEC_BW_40)
        {
            return TRUE;
        }
    }
    else if(CHSPEC_IS5G(chanspec))
    {
        if(chspec_bw == WL_CHANSPEC_BW_8080)
        {
            uint ch1_id, ch2_id;
            // channel IDs in 80+80 must be in range 
            ch1_id = CHSPEC_CHAN1(chanspec);
            ch2_id = CHSPEC_CHAN2(chanspec);
            if(ch1_id >= WF_NUM_5G_80M_CHANS || ch2_id >= WF_NUM_5G_80M_CHANS) {
                return TRUE;
            }
        }
        else if(chspec_bw == WL_CHANSPEC_BW_20 ||
                chspec_bw == WL_CHANSPEC_BW_40 ||
                chspec_bw == WL_CHANSPEC_BW_80 ||
                chspec_bw == WL_CHANSPEC_BW_160)
        {
            if(chspec_ch > MAXCHANNEL) {
                return TRUE;
            }
        }
        else
        {
            // invalid bandwidth
            return TRUE;
        }
    }
    else
    {
        // must be 2G or 5G band
        return TRUE;
    }

    // side band needs to be consistent with bandwidth
    if(chspec_bw == WL_CHANSPEC_BW_20) {
        if(CHSPEC_CTL_SB(chanspec) != WL_CHANSPEC_CTL_SB_LLL) {
            return TRUE;
        }
    } else if(chspec_bw == WL_CHANSPEC_BW_40) {
        if(CHSPEC_CTL_SB(chanspec) > WL_CHANSPEC_CTL_SB_LLU) {
            return TRUE;
        }
    } else if(chspec_bw == WL_CHANSPEC_BW_80 ||
              chspec_bw == WL_CHANSPEC_BW_8080)
    {
        if(CHSPEC_CTL_SB(chanspec) > WL_CHANSPEC_CTL_SB_LUU) {
            return TRUE;
        }
    }
    else if(chspec_bw == WL_CHANSPEC_BW_160) {
        if(CHSPEC_CTL_SB(chanspec) > WL_CHANSPEC_CTL_SB_UUU) {
            return TRUE;
        }
    }

    // it's good
    return FALSE;
}

// This function returns the channel number that control traffic is being
// sent on, for 20MHz channels this is just the channel number, for 40MHZ,
// 80MHz, 160MHz channels it is the 20MHZ sideband depending on the
// chanspec selected
unsigned char wf_chspec_ctlchan(chanspec_t chspec)
{
    uint center_chan;
    uint bw_mhz;
    uint sb;

    if(wf_chspec_malformed(chspec)) {
        log("%s: malformed chanspec: 0x04x\n",
            __func__, (unsigned short)chspec);
    }

    /* Is there a sideband ? */
    if(CHSPEC_IS20(chspec)) {
        return CHSPEC_CHANNEL(chspec);
    } else {
        sb = CHSPEC_CTL_SB(chspec) >> WL_CHANSPEC_CTL_SB_SHIFT;

        if(CHSPEC_IS8080(chspec)) {
            // For an 80+80 MHz channel, the sideband 'sb' field is an 80 MHz
            // sideband (LL, LU, UL, LU) for the 80 MHz frequency segment 0.
            uint chan_id = CHSPEC_CHAN1(chspec);
            bw_mhz = 80;
            // convert from channel index to channel number
            center_chan = wf_5g_80m_chans[chan_id];
        }
        else
        {
            bw_mhz = bw_chspec_to_mhz(chspec);
            center_chan = CHSPEC_CHANNEL(chspec) >> WL_CHANSPEC_CHAN_SHIFT;
        }

        return (channel_to_ctl_chan(center_chan, bw_mhz, sb));
    }
}

// Generic Broadcom's ioctl function
// ifname - interface name
// cmd - IOCTL command code
// buf - buffer for/with data
// len - buffer size
// Returns: 0 - ok, negative - error
int wl_ioctl(const char *ifname, int cmd, void *buf, int len)
{
    struct ifreq ifr;
    wl_ioctl_t ioc;
    int ret, s;

    memset(&ioc, 0, sizeof(ioc));
    ioc.cmd = cmd;
    ioc.buf = buf;
    ioc.len = len;

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_data = (caddr_t) &ioc;

    if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return -1;
    }
    ret = ioctl(s, SIOCDEVPRIVATE, &ifr);
    close(s);

    return ret;
}

// Generic Broadcom's WLC_GET_VAR ioctl command helper
// ifname - interface name
// cmd - text command accepted by WLC_GET_VAR
// arg - command data (can be binary or text)
// arglen - command data length
// buf - buffer where to store the cmd & arg and where the data will be returned
// buflen - buffer size
// Returns: 0 - ok, negative - error
int wl_iovar_getbuf(const char *ifname, const char *cmd, void *arg,
                    int arglen, void *buf, int buflen)
{
    int cmdlen = strlen(cmd) + 1;

    if(cmdlen + arglen > buflen) {
        return -11;
    }
    memcpy(buf, cmd, cmdlen);
    if(arg && arglen > 0) {
        memcpy(buf + cmdlen, arg, arglen);
    }

    return wl_ioctl(ifname, WLC_GET_VAR, buf, buflen);
}

// Another generic Broadcom's WLC_GET_VAR ioctl command helper
// ifname - interface name
// cmd - text command accepted by WLC_GET_VAR
// buf - buffer where to store the cmd & arg and where the data will be returned
// buflen - buffer size
// Returns: 0 - ok, negative - error
int wl_iovar_get(char *ifname, char *cmd, void *buf, int buflen)
{
    int ret;
    char sbuf[WLC_IOCTL_SMLEN];

    if(buflen > sizeof(sbuf)) {
        ret = wl_iovar_getbuf(ifname, cmd, NULL, 0, buf, buflen);
    } else {
        ret = wl_iovar_getbuf(ifname, cmd, NULL, 0, sbuf, sizeof(sbuf));
        if(ret == 0) {
           memcpy(buf, sbuf, buflen);
        }
    }

    return ret;
}

#endif // WIRELESS_WL_NO_LIBSHARED

// Return the name of the radio (from its number)
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
    int ii, jj, ifradio = -1;

    for(ii = 0; ii < UTIL_ARRAY_SIZE(radio_list); ++ii)
    {
        for(jj = 0; jj < WIRELESS_MAX_VAPS; ++jj)
        {
            if(strcmp(ifname, vap_list[ii][jj]))
            {
                ifradio = ii;
                break;
            }
        }
        if(ifradio >= 0) {
            break;
        }
    }

    return ifradio;
}

// Return the name of the VAP interface given its radio # and VAP #.
// Returns NULL if radio # or VAP # is out of range.
char *wt_get_vap_name(int radio_num, int ii)
{
    if(radio_num < 0 || radio_num >= UTIL_ARRAY_SIZE(radio_list)) {
        return NULL;
    }
    if(ii < 0 || ii >= WIRELESS_MAX_VAPS) {
        return NULL;
    }
    return vap_list[radio_num][ii];
}

// Returns the # of the radio and the VAP the interface belongs to
// (radio 0 - 2.4GHz, 1 - 5GHz), -1 if not a wireless interface
int wt_get_ifname_radio_vap_num(char *ifname, int *p_radio_num)
{
    int ifradio = -1;
    int ifvap = -1;
    int ii, jj;

    for(ii = 0; ii < UTIL_ARRAY_SIZE(radio_list); ++ii)
    {
        for(jj = 0; jj < WIRELESS_MAX_VAPS; ++jj)
        {
            if(strcmp(ifname, vap_list[ii][jj]) == 0)
            {
                ifradio = ii;
                ifvap = jj;
                break;
            }
        }
        if(ifvap >= 0) {
            break;
        }
    }

    if(p_radio_num != NULL) {
        *p_radio_num = ifradio;
    }

    return ifvap;
}

// Check the radio interface to be wl and have the same interface
// requiremnts as the radios that were initialized before.
static int wl_check(char *ifname)
{
    int ret, val, need_swap, ioctl_version;

    ret = wl_ioctl(ifname, WLC_GET_MAGIC, &val, sizeof(int));
    if(ret < 0) {
        log_dbg("%s: WLC_GET_MAGIC has failed for %s\n", __func__, ifname);
        return -1;
    }
    // Check if need data byte swap
    need_swap = FALSE;
    if(val == (int)(__bswap_32(WLC_IOCTL_MAGIC)))
    {
        val = __bswap_32(val);
        need_swap = TRUE;
    }
    if(val != WLC_IOCTL_MAGIC)
    {
        log("%s: invalid magic 0x%x (not 0x%x) for %s\n",
            __func__, val, WLC_IOCTL_MAGIC, ifname);
        return -2;
    }
    // Check IOCTL version
    ret = wl_ioctl(ifname, WLC_GET_VERSION, &val, sizeof(int));
    if(ret < 0) {
        log("%s: WLC_GET_VERSION has failed for %s\n", __func__, ifname);
        return -3;
    }
    ioctl_version = need_swap ? __bswap_32(val) : val;
    if(ioctl_version != WLC_IOCTL_VERSION && ioctl_version != 1)
    {
        log("%s: wl version mismatch, expected %d or 1, got %d\n",
            __func__, WLC_IOCTL_VERSION, ioctl_version);
        return -4;
    }
    // Make sure its the same for all radios
    if(wl_initialized) {
        if(wl_need_byte_swap != need_swap) {
            log("%s: different byte swap requirement for radios\n", __func__);
            return -5;
        }
        if(wl_ioctl_version != ioctl_version) {
            log("%s: different IOCTL versions for radios\n", __func__);
            return -6;
        }
    }
    wl_need_byte_swap = need_swap;
    wl_ioctl_version = ioctl_version;
    wl_initialized = TRUE;

    log_dbg("%s: radio %s, IOCTL version %d, byteswap <%s>\n",
            __func__, ifname, wl_ioctl_version, (need_swap ? "yes" : "no"));

    return 0;
}

// Retrieve chanspec from wl_bss_info_t structure
// (the same way Broadcom does it)
// The chanspec is returned in lower bits of the value.
// The return value is negative if it fails.
int wl_chanspec_from_bssinfo(wl_bss_info_t *bi, chanspec_t *cspec)
{
    chanspec_t chanspec = dtoh16(bi->chanspec);
    if(wl_ioctl_version == 1)
    {
        chanspec = LCHSPEC_CHANNEL(bi->chanspec);
        // Convert the band
        if(LCHSPEC_IS2G(bi->chanspec)) {
            chanspec |= WL_CHANSPEC_BAND_2G;
        } else {
            chanspec |= WL_CHANSPEC_BAND_5G;
        }
        // Convert the bw and sideband
        if(LCHSPEC_IS20(bi->chanspec)) {
            chanspec |= WL_CHANSPEC_BW_20;
        } else {
            chanspec |= WL_CHANSPEC_BW_40;
            if(LCHSPEC_CTL_SB(bi->chanspec) == WL_LCHANSPEC_CTL_SB_LOWER) {
                chanspec |= WL_CHANSPEC_CTL_SB_L;
            } else {
                chanspec |= WL_CHANSPEC_CTL_SB_U;
            }
        }
    }

    // Check the spec we have now
    if(wf_chspec_malformed(chanspec)) {
        log_dbg("%s: invalid chanspec 0x%04x\n", __func__, chanspec);
        return -4;
    }

    // Return the result
    if(cspec != NULL) {
        *cspec = chanspec;
    }

    return 0;
}

// Extracts readable chanspec description string and channel width integer.
// Returns: channel width integer,
//          fills in buf w/ chanspec str (must be of CHANSPEC_STR_LEN),
//          should never fail (but might return a mess)
int wl_get_specstr_and_width(chanspec_t chspec, char *buf)
{
    int width, ctl_chan;
    char *band = "";

    // prepare band string
    if((CHSPEC_IS2G(chspec) && CHSPEC_CHANNEL(chspec) > CH_MAX_2G_CHANNEL) ||
       (CHSPEC_IS5G(chspec) && CHSPEC_CHANNEL(chspec) <= CH_MAX_2G_CHANNEL))
    {
        band = (CHSPEC_IS2G(chspec)) ? "2g" : "5g";
    }

    // control channel
    ctl_chan = wf_chspec_ctlchan(chspec);

    // width and spec info string in Broadcom's format
    if(CHSPEC_IS20(chspec))
    {
        snprintf(buf, CHANSPEC_STR_LEN, "%s%d", band, ctl_chan);
        width = 20;
    }
    else if(!CHSPEC_IS8080(chspec))
    {
        const char *bw;
        const char *sb = "";
        int bwidx = (chspec & WL_CHANSPEC_BW_MASK) >> WL_CHANSPEC_BW_SHIFT;

        bw = wf_chspec_bw_str[bwidx];
        width = wf_chspec_bw_mhz[bwidx];
        // ctl sideband string if needed for 2g 40MHz
        if(CHSPEC_IS40(chspec) && CHSPEC_IS2G(chspec)) {
            sb = CHSPEC_SB_UPPER(chspec) ? "u" : "l";
        }
        snprintf(buf, CHANSPEC_STR_LEN, "%s%d/%s%s", band, ctl_chan, bw, sb);
    }
    else
    {
        // 80+80
        unsigned int chan1 =
            (chspec & WL_CHANSPEC_CHAN1_MASK) >> WL_CHANSPEC_CHAN1_SHIFT;
        unsigned int chan2 =
            (chspec & WL_CHANSPEC_CHAN2_MASK) >> WL_CHANSPEC_CHAN2_SHIFT;
        // convert to channel number
        chan1 = (chan1 < WF_NUM_5G_80M_CHANS) ? wf_5g_80m_chans[chan1] : 0;
        chan2 = (chan2 < WF_NUM_5G_80M_CHANS) ? wf_5g_80m_chans[chan2] : 0;
        // Outputs a max of CHANSPEC_STR_LEN chars including '\0'
        snprintf(buf, CHANSPEC_STR_LEN, "%d/80+80/%d-%d",
                 ctl_chan, chan1, chan2);
        width = 160;
    }

    return width;
}

// Pointer to wireless telemetry temp buffer. Should only be used in the
// wireless telemetry main thread. It is of WLC_IOCTL_MAXLEN, so can be used
// in all IOCTLs. It is allocated at init and is never freed.
void *wt_get_ioctl_tmp_buf(void)
{
    return wlcval;
}

// Get association list for a VAP interface, write it into buf, the buf has to
// be at least WLC_IOCTL_MAXLEN bytes long. It is filled in with "maclist_t".
// The first 32 bit word - "count", the rest is "struct ether_addr" array "ea".
// Returns: the "count" if successful, negative if fails
int wl_get_stations(char *ifname, void *sta_info_buf)
{
    int ret;

    maclist_t *maclist = (struct maclist *)sta_info_buf;
#if WT_ASSOC_ALT
    // Alternate way to fetch the clients for some Broadcom platforms
    maclist->count = htod32((WLC_IOCTL_MAXLEN - sizeof(int)) / ETHER_ADDR_LEN);
#else
    strcpy(sta_info_buf, "assoclist");
#endif
    ret = wl_ioctl(ifname, WLC_GET_ASSOCLIST, sta_info_buf, WLC_IOCTL_MAXLEN);
    if(ret < 0) {
        log("%s: WLC_GET_ASSOCLIST has failed for %s\n", __func__, ifname);
        return -1;
    }
    if(maclist->count > (WLC_IOCTL_MAXLEN - UTIL_OFFSETOF(maclist_t, ea)) /
                        sizeof(struct ether_addr))
    {
        log("%s: WLC_IOCTL_MAXLEN invalid number of stations %u\n",
            __func__, ifname, maclist->count);
        return -2;
    }

    return (int)(maclist->count);
}

// Retrieve, verify and return STA info in the supplied buffer.
// The buffer has to be at least WLC_IOCTL_MEDLEN bytes long.
// The function does the byte swap to covert the host byte order.
// Returns: negative if fails, zero if successful
int wl_get_sta_info(char *ifname, unsigned char *mac, void *buf)
{
    int ret;

    strcpy(buf, "sta_info");
    memcpy(buf + strlen(buf) + 1, mac, ETHER_ADDR_LEN);
    ret = wl_ioctl(ifname, WLC_GET_VAR, buf, WLC_IOCTL_MEDLEN);
    if(ret < 0)
    {
        //log("%s: STA query for " MAC_PRINTF_FMT_TPL " on %s has failed\n",
        //    __func__, MAC_PRINTF_ARG_TPL(mac), ifname);
        return -1;
    }

    unsigned short ver = dtoh16(*(unsigned short *)buf);
    // Check the STA info version
    if(ver < WL_STA_VER && ver > WL_STA_VER_MAX) {
        log("%s: %s sta_info version %hu is unrecognized\n",
            __func__, ver, ifname);
        return -2;
    }

    // Convert the byte order
    sta_info_t *si = (sta_info_t *)buf;

    si->ver = dtoh16(si->ver);
    si->len = dtoh16(si->len);
    si->cap = dtoh16(si->cap);

    si->flags = dtoh32(si->flags);
    si->idle = dtoh32(si->idle);
    si->in = dtoh32(si->in);
    si->listen_interval_inms = dtoh32(si->listen_interval_inms);
    si->tx_pkts = dtoh32(si->tx_pkts);
    si->tx_failures = dtoh32(si->tx_failures);
    si->rx_ucast_pkts = dtoh32(si->rx_ucast_pkts);
    si->rx_mcast_pkts = dtoh32(si->rx_mcast_pkts);
    si->tx_rate = dtoh32(si->tx_rate);
    si->rx_rate = dtoh32(si->rx_rate);
    si->rx_decrypt_succeeds = dtoh32(si->rx_decrypt_succeeds);
    si->rx_decrypt_failures = dtoh32(si->rx_decrypt_failures);
    si->tx_tot_pkts = dtoh32(si->tx_tot_pkts);
    si->rx_tot_pkts = dtoh32(si->rx_tot_pkts);
    si->tx_mcast_pkts = dtoh32(si->tx_mcast_pkts);

    si->tx_tot_bytes = dtoh64(si->tx_tot_bytes);
    si->rx_tot_bytes = dtoh64(si->rx_tot_bytes);
    si->tx_ucast_bytes = dtoh64(si->tx_ucast_bytes);
    si->tx_mcast_bytes = dtoh64(si->tx_mcast_bytes);
    si->rx_ucast_bytes = dtoh64(si->rx_ucast_bytes);
    si->rx_mcast_bytes = dtoh64(si->rx_mcast_bytes);

    si->aid = dtoh16(si->aid);
    si->ht_capabilities = dtoh16(si->ht_capabilities);
    si->vht_flags = dtoh16(si->vht_flags);

    si->tx_pkts_retried = dtoh32(si->tx_pkts_retried);
    si->tx_pkts_retry_exhausted = dtoh32(si->tx_pkts_retry_exhausted);
    si->tx_pkts_total = dtoh32(si->tx_pkts_total);
    si->tx_pkts_retries = dtoh32(si->tx_pkts_retries);
    si->tx_pkts_fw_total = dtoh32(si->tx_pkts_fw_total);
    si->tx_pkts_fw_retries = dtoh32(si->tx_pkts_fw_retries);
    si->tx_pkts_fw_retry_exhausted = dtoh32(si->tx_pkts_fw_retry_exhausted);
    si->rx_pkts_retried = dtoh32(si->rx_pkts_retried);
    si->tx_rate_fallback = dtoh32(si->tx_rate_fallback);

    return 0;
}

// Initializes wl subsystem
// Returns: 0 - success or negative int if error
int wt_wl_init(void)
{
    int ii;

    // Prepare to work with the radio interfaces on the platform
    for(ii = 0; ii < UTIL_ARRAY_SIZE(radio_list); ++ii) {
        if(wl_check(wt_get_radio_name(ii)) < 0) {
            // Something isn't right or we are not ready yet
            return -1;
        }
    }

    // Allocate temp buf for IOCTL calls from telemetry subsystems
    if(wlcval == NULL) {
        wlcval = UTIL_MALLOC(WLC_IOCTL_MAXLEN);
    }
    if(wlcval == NULL) {
        log_dbg("%s: failed to allocate %d bytes\n",
                __func__, WLC_IOCTL_MAXLEN);
        return -2;
    }

    return 0;
}
