// (c) 2019 minim.co
// unum local platform specific include file for code that
// handles local platform specific stuff, it is included by
// .c files explicitly (not through common headers), it should
// only be included by files in this folder


#ifndef _WIRELESS_PLATFORM_H
#define _WIRELESS_PLATFORM_H

// How often to query and report BRCM radio stats
// (in seconds), the output is a text about 500 bytes long
#define RADIO_STATS_PERIOD 300

// How long to wait after issuing the scan request till
// collecting the results (in sec)
#define WT_SCAN_LIST_WAIT_TIME 15

// Capture the STAs info for an interface
// Returns: # of STAs - if successful, negative error otherwise
int __attribute__((weak)) wt_rt_get_stas_info(int radio_num, char *ifname);

// Capture the association(s) (normally 1) info for an interface
// Returns: # of asociations - if successful, negative error otherwise
int __attribute__((weak)) wt_rt_get_assoc_info(int radio_num, char *ifname);

#endif // _WIRELESS_PLATFORM_H
