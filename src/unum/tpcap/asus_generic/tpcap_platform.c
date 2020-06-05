// (c) 2019 minim.co
// platform specific code for packet capturing

#include "unum.h"

// The include is needed only for platforms supporting hardware timestamp
#include <linux/net_tstamp.h>


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE

