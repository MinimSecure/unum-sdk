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

// unum platform code for collecting wireless neighborhood scanlist info

#include "unum.h"

// Include declarations for platform driver specific code
// we share locally.
#include "wireless_platform.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Buffer for the scanlist, once allocated it is never freed.
// The same buffer is used for all radios (we process one radio at a time).
static char *scanlist_buf = NULL;

// Capture the scan list for a radio.
// The phy name and # in rscan structure are populated in the common code.
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_scan_radio_info(WT_JSON_TPL_SCAN_RADIO_t *rscan)
{
    char *phyname = rscan->name;

    // If we have not yet allocated memory buffer for scan lists, do it
    // first (the bufer is reused and is never freed)
    if(scanlist_buf == NULL) {
        scanlist_buf = UTIL_MALLOC(IWINFO_BUFSIZE);
    }
    if(scanlist_buf == NULL) {
        log("%s: failed to allocate scan list memory\n", __func__);
        return -1;
    }

    // Capture the scan list on the phy.
    int len = wt_iwinfo_get_scan(phyname, scanlist_buf);
    if(len < 0)
    {
        // scan might not work due to DFS channel, log error in debug mode only
        // BTW, the upper layer still going to log an error
        log_dbg("%s: iwinfo scan error on <%s>, returned %d\n",
                __func__, phyname, len);
        return -2;
    }
    if(len >= IWINFO_BUFSIZE / sizeof(struct iwinfo_scanlist_entry))
    {
        log_dbg("%s: iwinfo scan for <%s>, too many (%d) entries\n",
                __func__, phyname, len);
        return -3;
    }
    // If 0 then either nothing scanned or radio is not allowed to scan
    if(len == 0)
    {
        log_dbg("%s: no data from iwinfo scan request on <%s>\n",
                __func__, phyname);
        return 1;
    }

    // Store the number of scan entries
    rscan->scan_entries = len;

    return 0;
}

// Extract the scan entry info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_scan_entry_info(WT_JSON_TPL_SCAN_RADIO_t *rscan,
                                WT_JSON_TPL_SCAN_ENTRY_t *entry, int ii)
{
    char *pos;

    // scan entry extras template and values
    static int quality, max_quality;
    static char encryption[64];
    static char mode[32];

    static JSON_OBJ_TPL_t extras_obj = {
      { "quality",     { .type = JSON_VAL_PINT, .pi = &quality }},
      { "max_quality", { .type = JSON_VAL_PINT, .pi = &max_quality }},
      { "mode",        { .type = JSON_VAL_STR, .s = mode }},
      { "encryption",  { .type = JSON_VAL_STR, .s = encryption }},
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

    // Calculate pointer to the requested entry
    struct iwinfo_scanlist_entry *e = (void *)scanlist_buf;
    e += ii;

    // Make sure we are not going beyond the buffer when accessing the entry
    if((void *)e + sizeof(struct iwinfo_scanlist_entry) >
       (void *)scanlist_buf + IWINFO_BUFSIZE)
    {
        return -3;
    }

    // Capture main channel
    entry->chan = e->channel;

    // BSSID
    snprintf(entry->bssid, sizeof(entry->bssid), MAC_PRINTF_FMT_TPL,
             MAC_PRINTF_ARG_TPL(e->mac));

    // SSID. Due to the limitation of the data we will not catch
    // SSIDs w/ 0 in the middle
    if(wt_mk_hex_ssid(entry->ssid, e->ssid, strlen(e->ssid)) != 0)
    {
        log("%s: invalid SSID for %s entry %d\n", __func__, rscan->name, ii);
        return -4;
    }

    // RSSI
    entry->rssi = e->signal - 0x100;

    // Set the extras

    // Signal quality and its max
    quality = e->quality;
    max_quality = e->quality_max;

    // mode (not sure why we need it here...)
    if(e->mode >= WT_NUM_IWINFO_MODES) {
        e->mode = 0; // unknown
    }
    strncpy(mode, IWINFO_OPMODE_NAMES[e->mode], sizeof(mode) - 1);
    mode[sizeof(mode) - 1] = 0;
    str_tolower(mode);

    // encryption
    struct iwinfo_crypto_entry *c = &e->crypto;
    if(!c->enabled || !(c->auth_algs || c->wpa_version))
    {
        snprintf(encryption, sizeof(encryption), "%s", "none");
    }
    else if(!c->wpa_version) // WEP
    {
        char cstr[32] = "unknown";
        pos = cstr;
        int ciphers = c->pair_ciphers;
        if(ciphers & IWINFO_CIPHER_WEP40) {
            pos += sprintf(pos, "wep-40,");
        }
        if(ciphers & IWINFO_CIPHER_WEP104) {
            pos += sprintf(pos, "wep-104,");
        }
        if(pos > cstr) {
            *(pos - 1) = 0;
        }
        snprintf(encryption, sizeof(encryption), "%s", cstr);
    }
    else // WPA
    {
        char *wpa_type = "wpa-unknown";
        switch(c->wpa_version) {
            case 3:
                wpa_type = "wpa/wpa2";
                break;
            case 2:
                wpa_type = "wpa2";
                break;
            case 1:
                wpa_type = "wpa";
                break;
        }

        char encr[64] = "auth-unknown";
        pos = encr;
        if(c->auth_suites & IWINFO_KMGMT_PSK) {
            pos += sprintf(pos, "psk/");
        }
        if(c->auth_suites & IWINFO_KMGMT_8021x) {
            pos += sprintf(pos, "802.1x/");
        }
        if(!c->auth_suites || (c->auth_suites & IWINFO_KMGMT_NONE)) {
            pos += sprintf(pos, "none/");
        }
        if(pos > encr) {
            *(pos - 1) = 0;
        }

        char cstr[64] = "unknown";
        pos = cstr;
        int ciphers = c->group_ciphers | c->pair_ciphers;
        if(ciphers & IWINFO_CIPHER_TKIP) {
            pos += sprintf(pos, "tkip,");
        }
        if(ciphers & IWINFO_CIPHER_CCMP) {
            pos += sprintf(pos, "ccmp,");
        }
        if(ciphers & IWINFO_CIPHER_WRAP) {
            pos += sprintf(pos, "wrap,");
        }
        if(ciphers & IWINFO_CIPHER_AESOCB) {
            pos += sprintf(pos, "aes-ocb,");
        }
        if(ciphers & IWINFO_CIPHER_CKIP) {
            pos += sprintf(pos, "ckip,");
        }
        if(!ciphers || (ciphers & IWINFO_CIPHER_NONE)) {
            pos += sprintf(pos, "none,");
        }
        if(pos > cstr) {
            *(pos - 1) = 0;
        }

        snprintf(encryption, sizeof(encryption),
                 "%s %s (%s)", wpa_type, encr, cstr);
    }

    // Set the extras object template ptr
    entry->extras = &(extras_obj[0]);

    return 0;
}

// This normally is used to tell all radios to scan wireless neighborhood.
// The libiwinfo initiates the scan automatically, so just return 0 here.
// Return: how many seconds till the scan results are ready
//         0 - if no need to wait, -1 - scan not supported
int wireless_do_scan(void)
{
    return 0;
}
