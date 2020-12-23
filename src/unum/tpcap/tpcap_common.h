// (c) 2017-2019 minim.co
// packet capturing subsystem common include file

#ifndef _TPCAP_COMMON_H
#define _TPCAP_COMMON_H

// Traffic counters and packet capturing time slot interval, in sec.
#define TPCAP_TIME_SLICE 30

// Interface discovery intervals (the TPCAP_TIME_SLICE intervals)
#define TPCAP_IF_DISCOVERY_NUM_SLICES 1


// Packet capturing ring defaults (a ring is created for each
// interface we are capturing on). It is the best to choose
// such ring size that it can be achieved with the least number of
// (PAGE_SIZE << n1) + ((PAGE_SIZE << n2) + ... etc segments.
// The n# here could be in the 0-MAX_ORDER (11 for 2.6 kernels) range.

// Number of packets in the ring
#define TPCAP_NUM_PACKETS 512

// Packet slot size in the ring
// It has to be big enough for capturing all DNS and similar info
// we are extracting from the captured packets.
#define TPCAP_SNAP_LEN 1024

// Ethernet II types min ID (we will ignore any ethtype less)
#define TPCAP_ETHTYPE_MIN 1536

// The total number and index of the WAN interface in the TPCAP_IF_STATS table
#ifndef FEATURE_LAN_ONLY // are we running on a router?
#  define TPCAP_STAT_IF_MAX (TPCAP_IF_MAX + 1)
#else  // !FEATURE_LAN_ONLY
#  define TPCAP_STAT_IF_MAX TPCAP_IF_MAX
#endif // !FEATURE_LAN_ONLY
#define TPCAP_WAN_STATS_IDX TPCAP_IF_MAX

// Structure describing monitored interface
typedef struct _TPCAP_IF {
#define TPCAP_IF_VALID    0x0001 // intrface info is populated
#define TPCAP_IF_FD_READY 0x0002 // descriptor (fd) ready for capturing
    int flags; // interface flags as described above
    int if_type; // type of the interface, refer to if_type from
                 // IF_ENUM_CB_EXT_DATA_t
    char name[IFNAMSIZ];  // interface name
    unsigned char mac[6]; // interface MAC address
    DEV_IP_CFG_t ipcfg;   // IP configuration of the device
    int ifidx; // interface index
    int fd; // packet capturing socket descriptor
    unsigned char *ring;   // packet capturing ring address
    unsigned int ring_len; // ring size in bytes
    unsigned int idx;      // last read packet index in the ring
    unsigned long long proc_pkt_count; // processed packets counter
} TPCAP_IF_t;

// Structure for tracking tpcap and interface counters/stats
typedef struct _TPCAP_IF_STATS {
    char name[IFNAMSIZ];    // interface name
    struct timespec rt;     // CLOCK_REALTIME when the counters were captured
    unsigned long len_t;    // the length of time (in msec) the conters are reported for
    unsigned int bytes_in;  // bytes in during the time slot
    unsigned int bytes_out; // bytes out during the time slot
    unsigned int proc_pkts; // processed packets during the time slot (not WAN)
    unsigned int tp_packets;// tpcap packets captured (if not WAN)
    unsigned int tp_drops;  // tpcap packets dropped  (if not WAN)
    // Values that are needed for calculating changes
    unsigned long last_t;   // uptime in msec the last counters were caprured
    unsigned long long last_in;  // last bytes-in counter for the interface
    unsigned long long last_out; // last bytes-out counter for the interface
    unsigned long long last_proc;// last tpcap processed pkts counter (not WAN)
} TPCAP_IF_STATS_t;


// Get the pointer to the interface stats table
// Should only be used in the the tpcap thread
TPCAP_IF_STATS_t *tpcap_get_if_stats(void);

// Get the pointer to the interface table
// Should only be used in the the tpcap thread
TPCAP_IF_t *tpcap_get_if_table(void);

// Add interface to the tp_ifs array
// Note: for use in tpcap thread only
// Returns: 0 - if the interface is added, error code otherwise
int tpcap_add_if(char *ifname, void *_unused);

// Subsystem init function
int tpcap_init(int level);


// TPCAP debugging extras
#ifdef DEBUG

// TPCAP test IDs
#define TPCAP_TEST_BASIC   1 // basic packet capturing test
#define TPCAP_TEST_FILTERS 2 // test packet filters and receive hooks
#define TPCAP_TEST_DNS     3 // test DNS info collection
#define TPCAP_TEST_IFSTATS 4 // test interface tpcap stats & counters
#define TPCAP_TEST_DT      5 // test devices & connection info collection
#define TPCAP_TEST_DT_JSON 6 // test building JSON from devices telemetry data
#define TPCAP_TEST_FP_JSON 7 // test fingerprinting info collection and JSON

// TPCAP test invocation macro
#define TPCAP_RUN_TEST(_id) (     \
  printf("Starting: %s\n", #_id), \
  tpcap_test(_id))

// The packet capturing testing parameter(s)
extern THRD_PARAM_t tpcap_test_param;

// Packet capturing entry point for tests, pass in the test type
int tpcap_test(int test_mode);

#endif // DEBUG

#endif // _TPCAP_COMMON_H
