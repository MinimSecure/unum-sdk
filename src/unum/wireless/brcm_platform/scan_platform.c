// (c) 2019 minim.co
// unum platform code for collecting wireless neighborhood scanlist info
// includes Broadcom definitions as follows:

/*
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
 * Fundamental types and constants relating to 802.11
 *
 * $Id: 802.11.h,v 1.2 2010-12-23 05:37:39 $
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


// Broadcom uses 127K buffer for scans
#define WL_SCAN_BUF_SIZE (127 * 1024)

// Buffer for the scanlist, it's quite large for Broadcom amd we do
// scans rarely, so it is freed after the results are printed out.
// The same buffer is used for all radios (we process one radio at a time).
static void *scanlist_buf = NULL;


// Notification from the common code that it is done with the scan info.
// Using it here to free the scanlist buffer that seems to be way too
// big for holding to.
void wt_scan_info_collected(void)
{
    if(scanlist_buf != NULL) {
        UTIL_FREE(scanlist_buf);
    	scanlist_buf = NULL;
    }
    return;
}

// Capture the scan list for a radio.
// The name and radio # in rscan structure are populated in the common code.
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_scan_radio_info(WT_JSON_TPL_SCAN_RADIO_t *rscan)
{
	int ret;
    char *ifname = rscan->name;

    if(scanlist_buf == NULL) {
    	scanlist_buf = malloc(WL_SCAN_BUF_SIZE);
    }
    if(scanlist_buf == NULL) {
    	log("%s: unable to allocate %d bytes\n", __func__, WL_SCAN_BUF_SIZE);
    	return -1;
    }

	wl_scan_results_t *list = (wl_scan_results_t*)scanlist_buf;
	list->buflen = htod32(WL_SCAN_BUF_SIZE);
	ret = wl_ioctl(ifname, WLC_SCAN_RESULTS, scanlist_buf, WL_SCAN_BUF_SIZE);
	if(ret < 0) {
    	log("%s: WLC_SCAN_RESULTS has failed for %s, %s\n",
            __func__, ifname, strerror(errno));
    	return -2;
    }

    // Reorder bytes for the header and check version
	list->buflen = dtoh32(list->buflen);
	list->version = dtoh32(list->version);
	list->count = dtoh32(list->count);
	if(list->buflen == 0)
    {
    	log_dbg("%s: WLC_SCAN_RESULTS returned no results for %s\n",
                __func__, ifname);
        return 1;
	}
    else if(list->version != WL_BSS_INFO_VERSION)
    {
    	log("%s: WLC_SCAN_RESULTS invalid version for %s, got %d, need %d\n",
            __func__, ifname, list->version, WL_BSS_INFO_VERSION);
		list->buflen = 0;
		list->count = 0;
        return -3;
    }
	if(list->count < 0 ||
       list->count > WL_SCAN_BUF_SIZE / sizeof(wl_bss_info_t))
    {
    	log("%s: invalid BSS count %d\n", __func__, list->count);
		return -4;
    }

    // Store the number of scan entries
    rscan->scan_entries = list->count;

    return 0;
}

// Extract the scan entry info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_scan_entry_info(WT_JSON_TPL_SCAN_RADIO_t *rscan,
                                WT_JSON_TPL_SCAN_ENTRY_t *entry, int ii)
{
    char *ifname = rscan->name;
    int jj;

    // Extras
    static int width;
    static int noise;
    static char spec_str[CHANSPEC_STR_LEN];
    static char caps[160];
    // Populate extras
    static JSON_OBJ_TPL_t extras_obj = { // stats must be first
      { "width",   { .type = JSON_VAL_PINT, {.pi = &width  }}},
      { "cspec",   { .type = JSON_VAL_STR,  {.s  = spec_str}}},
      { "noise",   { .type = JSON_VAL_PINT, {.pi = &noise  }}},
      { "caps",    { .type = JSON_VAL_STR,  {.s  = &caps[1]}}},
      { NULL }
    };

    // We must have the scan data if we are here
    if(scanlist_buf == NULL) {
        return -1;
    }
    // Check if the index is out of range (it should never happen).
    if(ii < 0 || ii >= rscan->scan_entries) {
        return -2;
    }

	wl_scan_results_t *list = (wl_scan_results_t*)scanlist_buf;
	wl_bss_info_t *bi = list->bss_info;

    // Find the requested entry
	for(jj = 0; jj < ii;
        jj++, bi = (wl_bss_info_t*)((void *)bi + dtoh32(bi->length)))
    {
        if((void *)bi < scanlist_buf ||
           (void *)bi >= scanlist_buf + WL_SCAN_BUF_SIZE)
        {
        	log("%s: corrupt BSS list before entry %d\n", __func__, jj);
	    	return -3;
        }
    }

    // If we get here, then the entry is found, first check the entry version
    if(WL_BSS_INFO_VERSION != dtoh32(bi->version)) {
        log("%s: %s BSS info version %d, need %d\n",
            __func__, ifname, dtoh32(bi->version), WL_BSS_INFO_VERSION);
        return -4;
    }

    // SSID
    if(wt_mk_hex_ssid(entry->ssid, bi->SSID, dtoh32(bi->SSID_len)) != 0)
    {
        log("%s: invalid SSID for %s entry %d\n", __func__, ifname, ii);
        return -5;
    }

    // BSSID
    snprintf(entry->bssid, sizeof(entry->bssid),
             MAC_PRINTF_FMT_TPL, MAC_PRINTF_ARG_TPL(&bi->BSSID));

    // RSSI
    entry->rssi = (int)(dtoh16(bi->RSSI));

    // control (main) channel and related extras
    chanspec_t chspec;
    if(wl_chanspec_from_bssinfo(bi, &chspec) < 0)
    {
        log("%s: invalid chanspec for %s entry %d\n", __func__, ifname, ii);
        return 1; // just skip it (not essential), but log (not nmormal)
    }
    // control/main channel
    entry->chan = wf_chspec_ctlchan(chspec);
    // width and spec info in Broadcom's format
    width = wl_get_specstr_and_width(chspec, spec_str);

    // Noise level
    noise = bi->phy_noise;

    // Various capabilities
	unsigned short cap = dtoh16(bi->capability);
    caps[0] = caps[1] = 0; // no way it could be empty, but just in case...
    snprintf(caps, sizeof(caps), "%s%s%s%s%s%s%s%s%s%s%s",
             (bi->n_cap)                 ? " HT"      : "", // 11n capable
             (bi->n_cap && bi->vht_cap)  ? " VHT"     : "", // 11ac capable
	         (cap & DOT11_CAP_ESS)       ? " ESS"     : "", // AP
	         (cap & DOT11_CAP_IBSS)      ? " IBSS"    : "", // ad-hoc
	         //(cap & DOT11_CAP_POLLABLE)  ? " POLL"    : "", // polling
	         //(cap & DOT11_CAP_POLL_RQ)   ? " POLLRQ"  : "", // polling
	         (cap & DOT11_CAP_PRIVACY)   ? " WEP"     : "", // using WEP
	         (cap & DOT11_CAP_SHORT)     ? " SHORTPRE": "", // Short preamble
	         (cap & DOT11_CAP_PBCC)      ? " PBCC"    : "", // 11g modulation
	         (cap & DOT11_CAP_AGILITY)   ? " AGILE"   : "", // Channel Agility
	         (cap & DOT11_CAP_SHORTSLOT) ? " SSLOT"   : "", // 11g short slot
	         (cap & DOT11_CAP_RRM)       ? " RRM"     : "", // 11k support
	         (cap & DOT11_CAP_CCK_OFDM)  ? " CCK/OFDM": "");// 11g coex feature

    // Set the extras object template ptr
    entry->extras = &(extras_obj[0]);

    return 0;
}

// Tell all radios to scan wireless neighborhood
// Return: how many seconds till the scan results are ready
//         0 - if no need to wait, -1 - scan not supported
// Note: if the return value is less than WIRELESS_ITERATE_PERIOD
//       the results are queried on the next iteration
int wireless_do_scan(void)
{
    int ii, count, intval, ret;
    char *ifname;

    // Walk over radios and issue the scan command
    for(count = ii = 0; (ifname = wt_get_radio_name(ii)) != NULL; ++ii)
    {
        // BRCM does not bring the interface down, just turs off the radio,
        // but we might have modules not loaded and devices mssing altogether.
        if(!util_net_dev_is_up(ifname)) {
            continue;
        }

        // If radio is disabled, skip it
        ret = wl_ioctl(ifname, WLC_GET_RADIO, &intval, sizeof(intval));
        if(ret < 0)
        {
            log("%s: radio interface %s is not supported\n", __func__, ifname);
            continue;
        }
        // Ignoring the MPC (minimum power consumption) mode bit, the other bits
        // mean the radio is really off.
        if((intval & ~(WL_RADIO_MPC_DISABLE)) != 0)
        {
            log_dbg("%s: the radio is disabled, status 0x%x\n",
                    __func__, intval);
            continue;
        }

        // Count working radios (even if cannot issue scan)
        ++count;

        // Prepare and send the scan request
        int params_size = WL_SCAN_PARAMS_FIXED_SIZE +
                          WL_NUMCHANNELS * sizeof(unsigned short);
        unsigned char params_buf[params_size];
        wl_scan_params_t *params = (wl_scan_params_t *)params_buf;
    	memset(params_buf, 0, params_size);
    	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
    	params->bss_type = DOT11_BSSTYPE_ANY;
    	params->scan_type = 0;
    	params->nprobes = -1;
    	params->active_time = -1;
    	params->passive_time = -1;
    	params->home_time = -1;
    	params->channel_num = 0;

        ret = wl_ioctl(ifname, WLC_SCAN, params, params_size);
        if(ret < 0) {
            log("%s: scan request failed for <%s>\n",
                __func__, ifname);
            continue;
        }
    }

    // If there are no working radios no point to get the scan report
    if(count == 0) {
        return -1;
    }
    // Tell the code to get the scan report in WT_SCAN_LIST_WAIT_TIME sec.
    return WT_SCAN_LIST_WAIT_TIME;
}
