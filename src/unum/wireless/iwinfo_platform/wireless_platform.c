// (c) 2019 minim.co
// unum platform wireless helper routines that do not fit into
// a specific 'radios', 'STAs' and/or 'scan' category

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
// Note: doing it through iwinfo (not efficient, but hides the driver type)
// Parameters:
// ifname - The ifname to look up the STA on (could be NULL, search all)
// mac_str - STA MAC address (binary)
// wc - ptr to WIRELESS_COUNTERS_t buffer to return the data in
// Returns:
// 0 - ok, negative - unable to get the counters for the given MAC address
int wireless_get_sta_counters(char *ifname, unsigned char* mac,
                              WIRELESS_COUNTERS_t *wc)
{
    if(wireless_iwinfo_platform_get_sta_counters != NULL) {
        return wireless_iwinfo_platform_get_sta_counters(ifname, mac, wc);
    }
    char *sysnetdev_dir = "/sys/class/net";
    struct dirent *de;
    int ret, ii;
    DIR *dd;
    void *buf = NULL;
    struct iwinfo_assoclist_entry *e;
    unsigned int rec_idle_time = (unsigned int)(-1);

    dd = opendir(sysnetdev_dir);
    if(dd == NULL) {
        log("%s: error opening %s: %s\n",
            __func__, sysnetdev_dir, strerror(errno));
        return -1;
    }
    // Allocate association list buffer
    buf = UTIL_MALLOC(IWINFO_BUFSIZE);
    if(!buf) {
        log("%s: error allocating %d bytes\n", __func__, IWINFO_BUFSIZE);
        closedir(dd);
        return -2;
    }
    ret = -1;
    while((de = readdir(dd)) != NULL)
    {
        // Skip anything that starts w/ .
        if(de->d_name[0] == '.') {
            continue;
        }
        char *cur_ifname = de->d_name;
        const struct iwinfo_ops *iw = iwinfo_backend(cur_ifname);
        if(!iw) {
            continue;
        }
        int len = 0;
        if(iw->assoclist(cur_ifname, buf, &len) != 0 || len <= 0)
        {
            continue;
        }
        len /= sizeof(struct iwinfo_assoclist_entry);
        e = NULL;
        for(ii = 0; ii < len; ++ii)
        {
            e = ((struct iwinfo_assoclist_entry *)buf) + ii;
            if(memcmp(e->mac, mac, ETHER_ADDR_LEN) != 0) {
                continue;
            }
            break;
        }
        if(e == NULL || ii >= len) { // MAC not found in the interface assoclist
            continue;
        }
        if(ret == 0) { // already saw MAC on one of the interfaces
            wc->flags |= W_COUNTERS_MIF;
        }
        if(rec_idle_time < e->inactive) { // not the most recent
            continue;
        }
        rec_idle_time = e->inactive;
        wc->rx_p = e->rx_packets;
        wc->tx_p = e->tx_packets;
        wc->rx_b = e->rx_bytes;
        wc->tx_b = e->tx_bytes;
        if(sizeof(e->tx_bytes) < sizeof(wc->tx_b)) { // 32 bit counters
            wc->flags |= W_COUNTERS_32B;
        }
        strncpy(wc->ifname, cur_ifname, sizeof(wc->ifname) - 1);
        wc->ifname[sizeof(wc->ifname) - 1] = 0;
        ret = 0;
    }
    if(buf) {
        UTIL_FREE(buf);
        buf = NULL;
    }

    closedir(dd);
    return ret;
}

// Platform init/deinit functions for each telemetry submission.
int wt_platform_subm_init(void)
{
    int err;
    err = wt_iwinfo_mk_if_list();
    return err;
}
int wt_platform_subm_deinit(void)
{
    iwinfo_finish();
    return 0;
}

// Init function for the platform specific wireless APIs (if needed)
// It is called at WIRELESS_ITERATE_PERIOD until TRUE is returned.
// Until it returns TRUE no other wireless APIs are called and no wireless
// telemetry is sent to the cloud. After it returns TRUE the platform init
// is assumed to be completed and it is no longer called.
int wt_platform_init(void)
{
    // Nothing here for LEDE for now
    return TRUE;
}
