#include "unum.h"
// Include declarations for platform driver specific code
// we share locally.
#include "wireless_platform.h"

// Get wireless counters for a station.
// Note: doing it through iwinfo (not efficient, but hides the driver type)
// Parameters:
// ifname - The ifname to look up the STA on (could be NULL, search all)
// mac_str - STA MAC address (binary)
// wc - ptr to WIRELESS_COUNTERS_t buffer to return the data in
// Returns:
// 0 - ok, negative - unable to get the counters for the given MAC address
int wireless_iwinfo_platform_get_sta_counters(char *ifname, unsigned char* mac,
                              WIRELESS_COUNTERS_t *wc)
{
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
        if (strlen(cur_ifname) > 9) {
            // Per QCA iwinfo, name length should always be less than 9
            // On DLink, there is one interface called, bonding_masters
            // iwinfo_backend iterates and checks through all the
            // iwinfo backends, with first being QCA iwinfo and followed-by
            // wext. Since bonding_masters does n't match with QCA interfaces,
            // it will be checked with wext backend. However with wext backend,
            // it seems to be crashing with Interface names of largers sizes
            continue;
        }
        const struct iwinfo_ops *iw = iwinfo_backend(cur_ifname);
        if(!iw) {
            continue;
        }
        // get phyname for the interface
        char phyname[32]; // no bffer size, IFNAMSIZ?, but libiwinfo uses 32
        int err = iw->phyname(cur_ifname, phyname);
        if(err != 0 || *phyname == 0) {
            log("%s: error %d getting phy name for %s\n",
                __func__, err, cur_ifname);
            continue;
        }
        // Skip phy interface names, we only use vap names
        // For QCA drivers things work good with VAP name list only.
        if (strcmp(phyname, cur_ifname) == 0) {
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
