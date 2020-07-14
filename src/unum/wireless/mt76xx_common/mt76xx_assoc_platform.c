// (c) 2020 minim.co
// unum platform code for collecting wireless clients info
// for Mediatek 76xx radios

#include "unum.h"

// Include declarations for mt76xx
#include "mt76xx_wireless_platform.h"

// Include declarations for platform driver specific code
// we share locally.
#include "wireless_platform.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Buffer for association info, once allocated it is never freed.
// The same buffer is used for all STA interfaces.
static void *assoc_info_buf = NULL;


// Capture the association(s) (normally 1) info for an interface
// Returns: # of asociations (i.e. 1 or 0) - if successful,
//          negative error otherwise
int wt_rt_get_assoc_info(int radio_num, char *ifname)
{
    int num_assoc = 0;
    int buf_size = WT_STAS_MAC_TABLE_ENTRY_SIZE(radio_num) + sizeof(int);

    // If we do not yet have a buffer, allocate it (never freed)
    if(assoc_info_buf == NULL) {
        assoc_info_buf = UTIL_MALLOC(buf_size);
    }
    if(assoc_info_buf == NULL) {
        log("%s: failed to allocate memory for association info\n", __func__);
        return -1;
    }

    memset(assoc_info_buf, 0, buf_size);

#ifdef MT76XX_APCLI_MAC_TBL_SUPPORT

    int len = wt_wlext_priv_direct(ifname, RTPRIV_IOCTL_GET_MAC_TABLE_STRUCT, 0,
                                   assoc_info_buf, buf_size);

    if(len != buf_size) {
        log("%s: MAC table IOCTL for <%s> returned %d, expected %d\n",
            __func__, ifname, len, buf_size);
        return -2;
    }

    num_assoc = *((int*)assoc_info_buf);

#else // MT76XX_APCLI_MAC_TBL_SUPPORT

    // The RTPRIV_IOCTL_GET_MAC_TABLE_STRUCT not supported
    // MTK does not have interfaces to get any useful info
    // about the client connection besides BSSID, which also
    // serves as an indicator of association.

    RT_802_11_MAC_ENTRY *rt = (RT_802_11_MAC_ENTRY *)
                                  ((char*)assoc_info_buf + sizeof(int));
    for(;;)
    {
        if(wt_wlext_get_bssid(ifname, rt->Addr, NULL) != 0)
        {
            break;
        }
        // If BSSID is zeroed, then no association
        char null_mac[6] = {};
        if(memcmp(null_mac, rt->Addr, sizeof(null_mac)) == 0) {
            break;
        }

        // RSSI is mandatory, but we can't get it, way to go MTK!
        rt->AvgRssi0 = rt->AvgRssi1 = rt->AvgRssi2 = 0;
        num_assoc = 1;

        break;
    }

    *((int*)assoc_info_buf) = num_assoc;

#endif // MT76XX_APCLI_MAC_TBL_SUPPORT

    return num_assoc;
}

// Capture the association info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_assoc_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                           WT_JSON_TPL_VAP_STATE_t *vinfo,
                           WT_JSON_TPL_STA_STATE_t *sinfo,
                           int ii)
{
    int num_assocs = *((int*)assoc_info_buf);
    RT_802_11_MAC_ENTRY *rt = (RT_802_11_MAC_ENTRY *)
                                  ((char*)assoc_info_buf +  + sizeof(int));
    rt = (RT_802_11_MAC_ENTRY *)
             ((char *)rt + ii * WT_STAS_MAC_TABLE_ENTRY_SIZE(rinfo->num));

    // We already verified that the num_assocs is valid when captured the STAs
    // info, just check that ii is in the range.
    if(ii < 0 || ii >= num_assocs) {
        return -1;
    }

    // Capture the common info
    sprintf(sinfo->mac, MAC_PRINTF_FMT_TPL, MAC_PRINTF_ARG_TPL(rt->Addr));

    int rssi = rt->AvgRssi0;
    if(rssi == 0 || (rssi < rt->AvgRssi1 && rt->AvgRssi1 != 0)) {
        rssi = rt->AvgRssi1;
    }
    if(rssi == 0 || (rssi < rt->AvgRssi2 && rt->AvgRssi2 != 0)) {
        rssi = rt->AvgRssi2;
    }
    sinfo->rssi = rssi;

    // At the moment we have no extras for the association info here
    
    return 0;
}
