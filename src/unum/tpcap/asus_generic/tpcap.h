// (c) 2019 minim.co
// packet capturing subsystem platform include file

#ifndef _TPCAP_H
#define _TPCAP_H

// Pull in the common include
#include "../tpcap_common.h"
// Pull in include for packet processing code
#include "../process_pkt.h"


// Max # of interfaces we monitor (Asus has two - br0, brg0)
#define TPCAP_IF_MAX 2

// Override the number of packets per ring. Since Asus Routers  only
// needs one ring it can be bigger than where multiple rings are needed.
#undef TPCAP_NUM_PACKETS
#define TPCAP_NUM_PACKETS 1024


// Set up packet capturing socket to use hardware timestamps
// (for platforms with no support make it empty inline returning 0)
static inline int set_sockopt_hwtimestamp(int sock, char *dev) { return 0; }

#endif // _TPCAP_H

