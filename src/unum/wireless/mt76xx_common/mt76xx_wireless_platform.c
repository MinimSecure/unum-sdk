// (c) 2018 minim.co
// unum platform wireless helper routines that do not fit into
// a specific 'radios', 'STAs' and/or 'scan' category

#include "unum.h"

// Currently we only need this .h here for klogctl()
#include <sys/klog.h>

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

// Get wireless counters for a station.
// Declared as weak to allow platfroms override.
// Parameters:
// ifname - The ifname to look up the STA on (could be NULL, search all)
// mac_str - STA MAC address (binary)
// wc - ptr to WIRELESS_COUNTERS_t buffer to return the data in
// Returns:
// 0 - ok, negative - unable to get the counters for the given MAC address
int __attribute__((weak)) wireless_get_sta_counters(char *ifname,
                                                    unsigned char* mac,
                                                    WIRELESS_COUNTERS_t *wc)
{
#define MAX_STA_LINE_SIZE 80 // Approximate single STA stats max line length
#define MAX_STA_LINES     33 // Max STAs we can possibly see (per radio)
    int ii, len, ret, using_klog;
    char *cur_ifname;
    char buf[MAX_STA_LINE_SIZE * (MAX_STA_LINES + 1)]; // +header
    char hdr_str[] =
        "MAC                AID TxPackets   RxPackets   TxBytes     RxBytes";

    // Unless we are given specific interface, try all of them
    memset(wc, 0, sizeof(WIRELESS_COUNTERS_t));
    ret = -1;
    for(ii = 0; ii < WIRELESS_MAX_RADIOS; ++ii)
    {
        cur_ifname = wt_get_radio_name(ii);
        if(!cur_ifname || *cur_ifname == 0) {
            continue;
        }
        if(ifname && strcmp(ifname, cur_ifname) != 0) {
            continue;
        }
        if(if_nametoindex(cur_ifname) <= 0) {
            continue;
        }
        // The radios behave differently. The 5GHz radio IOCTL prints the
        // STA counters into the kernel log buffer. The 2.4GHz prints into the
        // passed in buffer. We issue the request here, then immediately capture
        // the kernel log and parse for the output from the bottom up until
        // either the buffer top is reached or the STA table header is detected.
        klogctl(6 /* SYSLOG_ACTION_CONSOLE_OFF */, 0, 0);
        len = wlext_priv_cmd(-1, cur_ifname,
                             "show\0stacountinfo\0\0", buf, sizeof(buf));
        if(len < 0)
        {
            // Failure
            klogctl(7 /* SYSLOG_ACTION_CONSOLE_ON */, 0, 0);
            continue;
        }
        // Success
        klogctl(7 /* SYSLOG_ACTION_CONSOLE_ON */, 0, 0);
        // If IOCTL did not return anything in the buffer try the log
        using_klog = FALSE;
        if(len < sizeof(hdr_str)) {
            // Capture the output from the kernel log.
            len = klogctl(3 /* SYSLOG_ACTION_READ_ALL */, buf, sizeof(buf));
            if(len < 0) {
                log_dbg("%s: klogctl() has failed: %s\n",
                        __func__, strerror(errno));
                continue;
            }
            using_klog = TRUE;
        }
        if(len == 0 || len > sizeof(buf)) {
            log_dbg("%s: invalid data length of %d bytes\n", __func__, len);
            continue;
        }
        // Parse the kernel log looking for lines matching our format string
        char *str = buf + (len - 1); // this must be '\n', but unwind if not
        while(str >= buf && *str != '\n') --str;
        // Loop through all the lines from bottom up looking for the
        // counters for the specified MAC adddress
        unsigned long tx_p, rx_p, tx_b, rx_b;
        int found = 0;
        while(str >= buf) {
            if(*str == '\n') {
                *str = 0;
            }
            char *str_end = str;
            while(str >= buf && *str != '\n') --str;
            // See if we can skip klog severety which is for our data "<4>"
            char *line = str + 1 + (using_klog ? 3 : 0);
            if(line >= str_end) {
                continue;
            }
            // If reached the table header line, we are done
            if(strncmp(hdr_str, line, sizeof(hdr_str) - 1) == 0) {
                break;
            }
            // Try to scan the line
            unsigned char cur_mac[ETHER_ADDR_LEN];
            unsigned int a[ETHER_ADDR_LEN];
            int num;
            char tmp;
            num = sscanf(line,
                         "%02x:%02x:%02x:%02x:%02x:%02x %*d %lu %lu %lu %lu %c",
                         &a[0], &a[1], &a[2], &a[3], &a[4], &a[5],
                         &tx_p, &rx_p, &tx_b, &rx_b, &tmp);
            if(num != 10) {
                // Not a counters line, skip
                continue;
            }
            for(num = 0; num < ETHER_ADDR_LEN; ++num) {
                cur_mac[num] = a[num];
            }
            if(memcmp(cur_mac, mac, ETHER_ADDR_LEN) == 0) {
                ++found;
                break;
            }
        }
        if(found == 0) {
            // MAC address is not found on the radio
            continue;
        }
        // Found matching MAC address
        if(ret == 0) { // saw MAC on the other radio too
            wc->flags |= W_COUNTERS_MIF;
        }
        wc->flags |= W_COUNTERS_32B;
        wc->rx_p = rx_p;
        wc->tx_p = tx_p;
        wc->rx_b = rx_b;
        wc->tx_b = tx_b;
        strncpy(wc->ifname, cur_ifname, sizeof(wc->ifname) - 1);
        wc->ifname[sizeof(wc->ifname) - 1] = 0;
        ret = 0;
    }
    return ret;
}

// Init function for the platform specific wireless APIs (if needed)
// It is called at WIRELESS_ITERATE_PERIOD until TRUE is returned.
// Until it returns TRUE no other wireless APIs are called and no wireless
// telemetry is sent to the cloud. After it returns TRUE the platform init
// is assumed to be completed and it is no longer called.
int wt_platform_init(void)
{
    return (wt_wlext_init() >= 0);
}
