// (c) 2018 minim.co
// forwarding engine stats subsystem hashtable definitions


// Stores information about ARP entries we detect. We only store it for IPs
// matching the IP networks configured on the monitored LAN interfaces.
// It's used to find out MAC addresses of the devices.
struct _FE_ARP {
    IPV4_ADDR_t   ipv4;   // device IPv4 address
    unsigned int  uptime; // uptime when the entry was last seen
    unsigned char mac[6]; // device MAC
    unsigned char no_mac; // flag indicating we need to discover the MAC
    unsigned char pad;    // unused yet
} __attribute__((packed));
typedef struct _FE_ARP FE_ARP_t;

// Store unchanging information about a connection in a header.
// This is required because we need to be able to hash the same
// connection consistently, even if its other info changes.
struct _FE_CONN_HDR {
    IPV4_ADDR_t    dev_ipv4;  // device IPv4 address
    IPV4_ADDR_t    peer_ipv4; // peer IPv4 address
    unsigned short dev_port;  // device TCP/UDP port
    unsigned short peer_port; // peer TCP/UDP port
    char ifname[IFNAMSIZ];    // interface name the device is seen on
                              // Note: potential 12 bytes memory saver
#if ((IFNAMSIZ % 4) != 0)
#  error IFNAMSIZ should be divisible by 4
#endif
    unsigned char  proto;     // IP protocol #
    unsigned char  rev;       // TRUE if connection was from peer to dev
} __attribute__((packed));
typedef struct _FE_CONN_HDR FE_CONN_HDR_t;

// Byte counter that contains 2 fields, one is the current value,
// the other is the last read value (so we can calculate the change
// when adding the stats to the device telemetry). 
struct _FE_COUNTER {
    unsigned long long bytes_read; // reported bytes counter
    unsigned long long bytes;      // current bytes counter
} __attribute__((packed));
typedef struct _FE_COUNTER FE_COUNTER_t;

// Connection table entry
struct _FE_CONN {
    FE_CONN_HDR_t  hdr;   // Unchanging header, used as the hash index
    unsigned short off;   // Entry offset from its hash match position
    FE_COUNTER_t   in;    // Bytes to LAN counter
    FE_COUNTER_t   out;   // Bytes from LAN counter
    unsigned int   uid;   // Platform id var that might be necessary
                          // for detecting when connection is reopened
                          // in between counter polls
    unsigned char  mac[6];// device MAC
#define FE_CONN_BUSY    0x0001 // busy, do not touch
#define FE_CONN_UPDATED 0x0002 // updated, but not yet read
#define FE_CONN_HAS_MAC 0x0004 // MAC address is known
    unsigned short flags; // Common code connection entry flags
} __attribute__((packed));
typedef struct _FE_CONN FE_CONN_t;

// Structure tracking statistics of the table operations
// This has to be compatible with DT_TABLE_STATS_t
struct _DT_TABLE_STATS; // Forward declaration
typedef struct _DT_TABLE_STATS FE_TABLE_STATS_t;
