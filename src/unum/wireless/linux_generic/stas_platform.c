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

// unum platform code for collecting wireless clients info

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
        sta_info_buf = UTIL_MALLOC(IWINFO_BUFSIZE);
    }
    if(sta_info_buf == NULL) {
        log("%s: failed to allocate memory for STAs\n", __func__);
        return -1;
    }

    return wt_iwinfo_get_stations(ifname, sta_info_buf);
}

// Capture the STA info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_sta_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                         WT_JSON_TPL_VAP_STATE_t *vinfo,
                         WT_JSON_TPL_STA_STATE_t *sinfo,
                         int ii)
{
    struct iwinfo_assoclist_entry *e;
    // STA extras template and values
    static int rx_rate;
    static int rx_mcs;
    static int rx_bw;
    static int rx_short_gi;
    static int rx_is_ht;
    static int rx_is_vht;
    static int rx_nss;
    static JSON_OBJ_TPL_t extras_obj_rx_rate = {
      { "rate",     { .type = JSON_VAL_PINT, .pi = &rx_rate }},
      { "mcs",      { .type = JSON_VAL_PINT, .pi = &rx_mcs }},
      { "bw",       { .type = JSON_VAL_PINT, .pi = &rx_bw }},
      { "short_gi", { .type = JSON_VAL_PINT, .pi = &rx_short_gi }},
      { "ht",       { .type = JSON_VAL_PINT, .pi = &rx_is_ht }},
      { "vht",      { .type = JSON_VAL_PINT, .pi = &rx_is_vht }},
      { "nss",      { .type = JSON_VAL_PINT, .pi = &rx_nss }},
      { NULL }
    };
    static int tx_rate;
    static int tx_mcs;
    static int tx_bw;
    static int tx_short_gi;
    static int tx_is_ht;
    static int tx_is_vht;
    static int tx_nss;
    static JSON_OBJ_TPL_t extras_obj_tx_rate = {
      { "rate",     { .type = JSON_VAL_PINT, .pi = &tx_rate }},
      { "mcs",      { .type = JSON_VAL_PINT, .pi = &tx_mcs }},
      { "bw",       { .type = JSON_VAL_PINT, .pi = &tx_bw }},
      { "short_gi", { .type = JSON_VAL_PINT, .pi = &tx_short_gi }},
      { "ht",       { .type = JSON_VAL_PINT, .pi = &tx_is_ht }},
      { "vht",      { .type = JSON_VAL_PINT, .pi = &tx_is_vht }},
      { "nss",      { .type = JSON_VAL_PINT, .pi = &tx_nss }},
      { NULL }
    };
    static int e_inactive;
    static unsigned long e_tx_pkts;
    static unsigned long e_tx_retr;
    static unsigned long e_tx_fail;
    static unsigned long e_rx_pkts;
    static unsigned long e_tx_bytes;
    static unsigned long e_rx_bytes;
    static int e_authorized;
    static int e_wme;
    static JSON_OBJ_TPL_t extras_obj = {
      // Rx and Tx rate objects should be at [0] and [1]
      { "rx_rate",  { .type = JSON_VAL_OBJ,  .o = extras_obj_rx_rate }},
      { "tx_rate",  { .type = JSON_VAL_OBJ,  .o = extras_obj_tx_rate }},
      { "inact_ms", { .type = JSON_VAL_PINT, .pi = &e_inactive }},
      { "tx_pkts",  { .type = JSON_VAL_PUL,  .pul = &e_tx_pkts }},
      { "tx_retr",  { .type = JSON_VAL_PUL,  .pul = &e_tx_retr }},
      { "tx_fail",  { .type = JSON_VAL_PUL,  .pul = &e_tx_fail }},
      { "rx_pkts",  { .type = JSON_VAL_PUL,  .pul = &e_rx_pkts }},
      { "tx_bytes", { .type = JSON_VAL_PUL,  .pul = &e_tx_bytes }},
      { "rx_bytes", { .type = JSON_VAL_PUL,  .pul = &e_rx_bytes }},
      { "auth_ok",  { .type = JSON_VAL_PINT, .pi = &e_authorized }},
      { "wme",      { .type = JSON_VAL_PINT, .pi = &e_wme }},
      { NULL }
    };

    // We must have the STA data if we are here
    if(sta_info_buf == NULL) {
        return -1;
    }

    // The index is already verified to be in the proper range in the
    // common wireless code, but check it anyway after calculating
    // the pointer (just in case)
    e = ((struct iwinfo_assoclist_entry *)sta_info_buf) + ii;
    if(((void*)e) < sta_info_buf ||
       ((void*)(e + 1)) > (sta_info_buf + IWINFO_BUFSIZE))
    {
        return -2;
    }

    // Capture the common info
    sprintf(sinfo->mac, MAC_PRINTF_FMT_TPL, MAC_PRINTF_ARG_TPL(e->mac));
    if(e->signal >= 0) {
        log("%s: invalid signal %d for STA %s on %s, skipping\n",
            __func__, e->signal, sinfo->mac, vinfo->ifname);
        return 1;
    }
    sinfo->rssi = e->signal;

    // Capture the extras
    e_inactive = e->inactive;
    e_tx_pkts = e->tx_packets;
    e_tx_retr = e->tx_retries;
    e_tx_fail = e->tx_failed;
    e_rx_pkts = e->rx_packets;
    e_tx_bytes = e->tx_bytes; // 32-bit
    e_rx_bytes = e->rx_bytes; // 32-bit

    e_authorized = e->is_authorized;
    e_wme = e->is_wme;

    rx_rate = e->rx_rate.rate;
    if(rx_rate > 0) {
        rx_mcs = e->rx_rate.mcs;
        rx_bw = e->rx_rate.mhz;
        rx_short_gi = e->rx_rate.is_short_gi;
        rx_is_ht = e->rx_rate.is_ht;
        rx_is_vht = e->rx_rate.is_vht;
        rx_nss = 1;
        if(rx_is_vht && e->rx_rate.nss > 0) {
            rx_nss = e->rx_rate.nss;
        }
        extras_obj[0].val.o = extras_obj_rx_rate;
    } else {
        extras_obj[0].val.o = NULL;
    }

    tx_rate = e->tx_rate.rate;
    if(tx_rate > 0) {
        tx_mcs = e->tx_rate.mcs;
        tx_bw = e->tx_rate.mhz;
        tx_short_gi = e->tx_rate.is_short_gi;
        tx_is_ht = e->tx_rate.is_ht;
        tx_is_vht = e->tx_rate.is_vht;
        tx_nss = 1;
        if(tx_is_vht && e->tx_rate.nss > 0) {
            tx_nss = e->tx_rate.nss;
        }
        extras_obj[1].val.o = extras_obj_tx_rate;
    } else {
        extras_obj[1].val.o = NULL;
    }

    sinfo->extras = &(extras_obj[0]);

    return 0;
}
