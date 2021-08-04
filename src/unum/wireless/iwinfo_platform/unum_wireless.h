// (c) 2019 minim.co
// unum platform specific include file for wireless

#ifndef _UNUM_WIRELESS_H
#define _UNUM_WIRELESS_H

#include <iwinfo.h>


// Max number of radios we can have for the platform (used for platforms
// where wireless code could be shared between hardare with potentially
// different number of radios, for now iwinfo and wl)
// For QCA platforms, VAP interface is used instead of Radio Interface
// There are three VAPs each on the first two radios and 4 VAPs on third radio,
#define WIRELESS_MAX_RADIOS 3

// Max VAPs per radio we can report (used in wireless_common.h)
// For some platforms we support these many
#define WIRELESS_MAX_VAPS 8


// Common header shared across all platforms
#include "../wireless_common.h"

// Header shared across platforms using wireless extensions
#include "../wireless_iwinfo.h"


// Max time we might potentially block the thread while doing wireless
// fiunctions calls (used to set watchdog).
#define WIRELESS_MAX_DELAY (60 * 5)


// Define that tells pingflood to ignore wireless counters
// (it still calls wireless_get_sta_counters() and checks if
// the MAC is on WiFi)
//#define WT_PINGFLOOD_DO_NOT_USE


#endif // _UNUM_WIRELESS_H
