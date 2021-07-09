// (c) 2017-2019 minim.co
// devices telemetry data tables include file

#ifndef _DT_DATA_TABLES_H
#define _DT_DATA_TABLES_H

// The values are placed in the tables based on their unique keys
// hash. This works well while the tables are mostly empty.
// When they fill up the number of collisions grows and the time needed to
// find or add an entry increases. In order to keep it fast we
// limit how much collisions the table functions will examine
// before giving up (to add or find an item). The number is a fraction
// of the table max size. The define below determines what fraction
// that is. For example:
// 1 - do not give up until the whole table is examined.
// 2 - examine 1/2 of the table before giving up
// 4 - examine 1/4 of the table before giving up
#define DT_SEARCH_LIMITER 2

// Forward declarations
struct _DT_CONN;
struct _DT_DEVICE;
struct _DT_DNS_IP;
struct _DT_DNS_NAME;
struct _DT_COUNTERS;

// Note: The structures are packed to minimize memory usage, so
//       watch for the fields alignment when changing them.
//       Some of those structures form arrays, for those make
//       sure the total size of the struct makes the elements
//       past the first position properly aligned as well.

// Connection information structure header
struct _DT_CONN_HDR {
    IPV4_ADDR_t ipv4;    // Peer's IPv4 address
    unsigned char proto; // Connection IP protocol (0 - unrecognized)
    unsigned char rev;   // TRUE if reporting reverse TCP/UDP port
    uint16_t port; // Peer's TCP/UDP port (device port if rev is TRUE)
    struct _DT_DEVICE *dev; // Ptr to the device owning the connection
} __attribute__((packed));
typedef struct _DT_CONN_HDR DT_CONN_HDR_t;

// Discovered device connection information structure for connections table
struct _DT_CONN {
    DT_CONN_HDR_t hdr; // Connection info entry header
#ifdef DT_CONN_MAP_IP_TO_DNS
    struct _DT_DNS_IP *dns_ip; // Ptr to the peer DNS name table entry
#endif // DT_CONN_MAP_IP_TO_DNS
    struct _DT_CONN   *next;   // Ptr to the device's next connection
    uint16_t syn_tcp_win_size; // Syn pkt TCP window size (if captured)
    uint16_t cur_tcp_win_size; // Current TCP window size
    uint32_t upd_total;  // Number of updates to the connection stats
    uint32_t bytes_to[DEVTELEMETRY_NUM_SLICES];   // to device
    uint32_t bytes_from[DEVTELEMETRY_NUM_SLICES]; // from device
} __attribute__((packed));
typedef struct _DT_CONN DT_CONN_t;

// Discovered device information structure for the device data table
struct _DT_DEVICE {
    unsigned char mac[6];  // device MAC address
    uint16_t rating;       // 0->free, 1->few out, 2->few in, 3->in+out or 3+
    unsigned long t_add;   // 1/10 sec uptime when the device was added
    IPV4_ADDR_t ipv4;      // Device IPv4 address
    char ifname[IFNAMSIZ]; // Interface name the device last seen on
#if (IFNAMSIZ % 4) != 0
#  error IFNAMSIZ should be divisible by 4
#endif
    // Subsystems can add fields and store their device data here
    // in this structure (have to add their tpcap handlers after
    // the dt collector ones to assure the device is added first)
    // Note: if using pointers here beware that the entries might be
    //       wiped clean and replaced if higher rated device is discovered,
    //       therefore if you need your pointer to be preserved bump up
    //       the entry rating to prevent dt telemetry ip_pkt_rcv_cb()
    //       from overriding it.

    DT_CONN_t *last_conn; // Pointer to the last added connection
    DT_CONN_t conn; // Device's first connection entry, combined with the
                    // device structure for efficiency and to allow easy
                    // replacement when the device table fills in with
                    // potentially fake (i.e. spoofed MAC) devices.

} __attribute__((packed));
typedef struct _DT_DEVICE DT_DEVICE_t;

// DNS table IP address entry
struct _DT_DNS_IP {
    IPV4_ADDR_t ipv4;         // IP address the name is discovered for
    struct _DT_DNS_NAME *dns; // DNS name entry pointer for the IP
    unsigned short refs;      // Number of references to this entry
    unsigned short pad;       // Padding, unused
};
typedef struct _DT_DNS_IP DT_DNS_IP_t;

// DNS table name entry
struct _DT_DNS_NAME {
// Max length of the DNS names (253 is the max, but we should
// be good with 127), plus the null-termination
#define DEVTELEMETRY_MAX_DNS 128
    char name[DEVTELEMETRY_MAX_DNS]; // DNS name string
    struct _DT_DNS_NAME *cname; // Ptr to entry we are the CNAME of
} __attribute__((packed));
typedef struct _DT_DNS_NAME DT_DNS_NAME_t;


// Interface counters table entry (for single interface)
// This table is small and does not require packing
typedef struct _DT_IF_STATS {
    char name[IFNAMSIZ];     // interface name
    unsigned long sec;       // sec since Epoch
    unsigned long msec;      // msec (in addition to the secons above)
    unsigned long len_t;     // time length (msec) the counters are reported for
    unsigned long bytes_in;  // bytes in during the time slot
    unsigned long bytes_out; // bytes out during the time slot
    unsigned long proc_pkts; // processed packets during the time slot (not WAN)
    unsigned long tp_packets;// tpcap packets captured (if not WAN)
    unsigned long tp_drops;  // tpcap packets dropped  (if not WAN)
    int slice;               // telemetry time slice # (0 - based)
    int wan;                 // TRUE if wan interface
    int kind;                // Interface kind
    DEV_IP_CFG_t ipcfg;      // IP configuration of the interface
    unsigned char mac[6];    // interface MAC address
} DT_IF_STATS_t;

// Structure tracking statistics of the table operations
// (same layout for all the tables)
typedef struct _DT_TABLE_STATS {
    unsigned long add_all;   // All attempts to add an item
    unsigned long add_limit; // Items not added due to values being over limit
    unsigned long add_busy;  // Items not added due to no free rows
    unsigned long add_10;    // Items placed to <10 offset from the hash-row
    unsigned long add_found; // Items being added that are already in the table
    unsigned long add_repl;  // Items being replaced (devices table only)
    unsigned long find_all;  // All attempts to find an entry
    unsigned long find_fails;// Failed attempts to find an entry
    unsigned long find_10;   // searches completed after examining <10 rows
} DT_TABLE_STATS_t;

#endif // _DT_DATA_TABLES_H
