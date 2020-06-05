// (c) 2017-2018 minim.co
// packet capturing subsystem platform include file

#ifndef _TPCAP_H
#define _TPCAP_H

// Pull in the common include
#include "../tpcap_common.h"
// Pull in include for packet processing code
#include "../process_pkt.h"


// Max # of interfaces we monitor. For LEDE it will be possible to monitor
// multiple interfaces (in addition to the standard "lan" interface).
// Assuming here that if that happens max we will need is the main LAN subnet,
// guest network subnet and IoT/isolated devices subnet, +1 (just in case).
#define TPCAP_IF_MAX 4

// We are using 1024B/Pkt. The 2048 pkts per interface willl require
// 2MB per interface. In the majority of the setups it is all we will need.
#undef TPCAP_NUM_PACKETS
#define TPCAP_NUM_PACKETS 2048


// Set up packet capturing socket to use hardware timestamps
// (for platforms with no support make it empty inline returning 0).
// For generic LEDE builds we do not know what hardware to expect.
static __inline__ int set_sockopt_hwtimestamp(int sock, char *dev) {
    return 0;
}

#endif // _TPCAP_H
