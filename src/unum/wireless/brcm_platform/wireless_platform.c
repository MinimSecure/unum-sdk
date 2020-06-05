// (c) 2019 minim.co
// unum platform wireless helper routines that do not fit into
// a specific 'radios', 'STAs' and/or 'scan' category
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


// Get wireless counters for a station.
// Parameters:
// ifname - The ifname to look up the STA on (could be NULL, search all)
// mac_str - STA MAC address (binary)
// wc - ptr to WIRELESS_COUNTERS_t buffer to return the data in
// Returns:
// 0 - ok, negative - unable to get the counters for the given MAC address
int wireless_get_sta_counters(char *ifname, unsigned char* mac,
                              WIRELESS_COUNTERS_t *wc)
{
    int ii, jj, ret;
    unsigned int rec_idle_time = (unsigned int)(-1);
    char *cur_ifname;
    unsigned char buf[WLC_IOCTL_MEDLEN]; // About 1.5k

    // Unless we are given specific interface, try all of them and
    // use the one with the least idle time
    memset(wc, 0, sizeof(WIRELESS_COUNTERS_t));
    ret = -1;
    for(jj = 0; jj < WIRELESS_MAX_RADIOS; ++jj)
    {
        for(ii = 0; ii < WIRELESS_MAX_VAPS; ++ii)
        {
            cur_ifname = wt_get_vap_name(jj, ii);
            if(!cur_ifname || *cur_ifname == 0) {
                continue;
            }
            if(ifname && strcmp(ifname, cur_ifname) != 0) {
                continue;
            }
            if(if_nametoindex(cur_ifname) <= 0) {
                continue;
            }
            if(wl_get_sta_info(cur_ifname, mac, buf) != 0) {
                continue;
            }
            if(ret == 0) { // already saw MAC on one of the interfaces
                wc->flags |= W_COUNTERS_MIF;
            }
            sta_info_t *si = (sta_info_t *)buf;
            if(rec_idle_time < si->idle) { // not the most recent
                continue;
            }
            rec_idle_time = si->idle;
            wc->rx_p = si->rx_tot_pkts;
            wc->tx_p = si->tx_tot_pkts;
            wc->rx_b = si->rx_tot_bytes;
            wc->tx_b = si->tx_tot_bytes;
            strncpy(wc->ifname, cur_ifname, sizeof(wc->ifname) - 1);
            wc->ifname[sizeof(wc->ifname) - 1] = 0;
            ret = 0;
        }
    }
    return ret;
}

// Platform init/deinit functions for each telemetry submission.
int wt_platform_subm_init(void)
{
    // Nothing here for Broadcom's wl for now
    return 0;
}
int wt_platform_subm_deinit(void)
{
    // Nothing here for Broadcom's wl for now
    return 0;
}

// Init function for the platform specific wireless APIs (if needed)
// It is called at WIRELESS_ITERATE_PERIOD until TRUE is returned.
// Until it returns TRUE no other wireless APIs are called and no wireless
// telemetry is sent to the cloud. After it returns TRUE the platform init
// is assumed to be completed and it is no longer called.
int wt_platform_init(void)
{
    // Call the wl subsystem initializer
    if(wt_wl_init() != 0) {
        // Not ready yet
        return FALSE;
    }
    return TRUE;
}
