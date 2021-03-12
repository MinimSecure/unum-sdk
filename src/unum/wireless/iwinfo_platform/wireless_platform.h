// (c) 2019 minim.co
// unum local platform specific include file for code that
// handles local platform specific stuff, it is included by
// .c files explicitly (not through common headers), it should
// only be included by files in this folder


#ifndef _WIRELESS_PLATFORM_H
#define _WIRELESS_PLATFORM_H

#ifdef FEATURE_MAC80211_LIBNL
#include "../wireless_nl80211.h"
#endif

// Capture the STAs info for an interface
// Returns: # of STAs - if successful, negative error otherwise
int wt_rt_get_stas_info(int radio_num, char *ifname);

#endif // _WIRELESS_PLATFORM_H
