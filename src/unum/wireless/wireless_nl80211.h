// (c) 2020 minim.co
// unum wireless APIs include file (shared by nl80211 based
// platforms)

#ifndef _WIRELESS_NL80211_H
#define _WIRELESS_NL80211_H
#include "unum.h"
#include <errno.h>
#include <netlink/errno.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <linux/nl80211.h>

// Scan buffer size
#define SCANLIST_BUF_SIZE  8192
#define ESSID_MAX_SIZE 64
#define ENC_BUF_SIZE 128

// Driver informs the scan status to user space process
struct scan_status {
    int done;
};

struct nl80211_scanlist_entry {
    uint8_t bssid[ETHER_ADDR_LEN];
    char ssid[ESSID_MAX_SIZE + 1];
    uint8_t channel;
    uint8_t ht_chwidth;
    uint8_t ht_secondary;
    uint8_t vht_chwidth;
    uint8_t vht_secondary1;
    uint8_t vht_secondary2;
    int signal;
    char enc_buf[ENC_BUF_SIZE];
};

// Scan and dump results
int wt_nl80211_get_scan(char *ifname, char *buf);

// Get the first VAP name
char* __attribute__((weak)) wt_platform_nl80211_get_vap(char *phyname);

// Get country code
char* wt_nl80211_get_country(char *ifname);
#endif
