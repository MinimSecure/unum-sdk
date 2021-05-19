// (c) 2017-2019 minim.co
// unum helper net utils include file

#ifndef _UTIL_NET_H
#define _UTIL_NET_H

// Length of the MAC address string (do we have a standard define for that?)
#define MAC_ADDRSTRLEN 18

// Format string and argument macros for printing MAC addresses
// from printf style functions
#define MAC_PRINTF_FMT_TPL "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_PRINTF_ARG_TPL(a)  \
  *((unsigned char *)(a)),     \
  *((unsigned char *)(a) + 1), \
  *((unsigned char *)(a) + 2), \
  *((unsigned char *)(a) + 3), \
  *((unsigned char *)(a) + 4), \
  *((unsigned char *)(a) + 5)

// Format string and argument macros for reading MAC addresses
// with sscanf style functions
#define MAC_SSCANF_FMT_TPL "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx"
#define MAC_SSCANF_ARG_TPL(a)  &a[0],&a[1],&a[2],&a[3],&a[4],&a[5]

// Format string and argument macros for printing IP addresses
// from printf style functions (network byte order is expected)
// The argument is the pointer to the IP address (32 bit value)
#define IP_PRINTF_FMT_TPL "%u.%u.%u.%u"
#define IP_PRINTF_ARG_TPL(a)	\
  *((unsigned char *)(a)),     \
  *((unsigned char *)(a) + 1), \
  *((unsigned char *)(a) + 2), \
  *((unsigned char *)(a) + 3)

// Format string and argument  macros for reading IP addresses
// with sscanf style functions
#define IP_SSCANF_FMT_TPL "%hhu.%hhu.%hhu.%hhu"
#define IP_SSCANF_ARG_TPL(a)  &a[0],&a[1],&a[2],&a[3]

// Static declaration of a port range
// _name - name of the PORT_RANGE_MAP_t structure to declare
// _s - start port
// _l - number of ports in the range
// UTIL_PORT_RANGE_DECLARE(port_range, 1, 1024);
// UTIL_PORT_RANGE_RESET(&port_range);
#define UTIL_PORT_RANGE_DECLARE(_name, _s, _l)              \
    struct {                                                \
      PORT_RANGE_MAP_t range;                               \
      int data[((((unsigned short)(_l)) + 7) / 8 + 3) / 3]; \
    } _name = {{ _s, _l}, {}}
// Malloc and free a port range.
// The allocator returns the pointer to PORT_RANGE_MAP_t
#define UTIL_PORT_RANGE_ALLOC(_s, _l) util_port_range_alloc(_s, _l)
#define UTIL_PORT_RANGE_FREE(_r) UTIL_FREE(_r);
// Set port '_p' in the port range pointed by '_r'
#define UTIL_PORT_RANGE_SET(_r, _p) util_port_range_op((_r), (_p), TRUE)
// Clear port '_p' in the port range pointed by '_r'
#define UTIL_PORT_RANGE_CLEAR(_r, _p) util_port_range_op((_r), (_p), FALSE)
// Set get value for port '_p' in the port range pointed by '_r'
#define UTIL_PORT_RANGE_GET(_r, _p) util_port_range_op((_r), (_p), -1)
// Reset (clear all ports) the port range pointed by '_r'
#define UTIL_PORT_RANGE_RESET(_r)                     \
    memset(((PORT_RANGE_MAP_t *)(_r))->data, 0,       \
           (((PORT_RANGE_MAP_t *)(_r))->len + 7) / 8)

// Macros for getting main LAN and WAN interface names. The names
// are retrieved from the config (if specified as the options) or
// control is passed over to the platform macros.
#define GET_MAIN_WAN_NET_DEV() (                                      \
    (unum_config.wan_ifcount > 0) ? (unum_config.wan_ifname) :        \
                                    (PLATFORM_GET_MAIN_WAN_NET_DEV()) \
)
#define GET_MAIN_LAN_NET_DEV() (                                      \
    (unum_config.lan_ifcount > 0) ? (unum_config.lan_ifname[0]) :     \
                                    (PLATFORM_GET_MAIN_LAN_NET_DEV()) \
)

// Port range map header data structure
typedef struct _PORT_RANGE_MAP {
    unsigned short start;// port range start port #
    unsigned short len;  // port range bits per port
    unsigned int data[]; // bitmap array
} PORT_RANGE_MAP_t;

// Helper inline for port range allocation
static inline PORT_RANGE_MAP_t *util_port_range_alloc(unsigned short start,
                                                      unsigned short len)
{
    PORT_RANGE_MAP_t *r_ptr;
    unsigned int map_size = ((len + 7) / 8 + 3) & (~3);
    r_ptr = UTIL_MALLOC(sizeof(PORT_RANGE_MAP_t) + map_size);
    if(r_ptr != NULL) {
        r_ptr->start = start;
        r_ptr->len = len;
    }
    return r_ptr;
};

// Helper inline for port range operations (if val < 0 indicates get operation)
// It returns TRUE, FLASE or a negative value if the port is out of range.
static inline int util_port_range_op(PORT_RANGE_MAP_t *r_ptr,
                                     unsigned short port, int val)
{
    if(port < r_ptr->start || port >= r_ptr->start + r_ptr->len) {
        return -1;
    }
    int off = (port - r_ptr->start) / (sizeof(int) * 8);
    int bit = (port - r_ptr->start) - (off * (sizeof(int) * 8));
    if(val == TRUE) {
        r_ptr->data[off] |= (1 << bit);
        return 0;
    }
    if(val == FALSE) {
        r_ptr->data[off] &= ~(1 << bit);
        return 0;
    }
    return (r_ptr->data[off] >> bit) & 1;
};

// IPv4 internal address data type
typedef union {
    unsigned char b[4];
    unsigned int  i;
} IPV4_ADDR_t;

// IP configuration structure describing device IP settings
typedef struct _DEV_IP_CFG {
    IPV4_ADDR_t ipv4; // byte order is the same as in struct sockaddr
    IPV4_ADDR_t ipv4mask; // byte order is the same as in struct sockaddr
} DEV_IP_CFG_t;

// Netlink socket structure
typedef struct _NL_SOCK {
    int s;            // socket descriptor
    unsigned int pid; // ID the kernel assigned to this netlink connection
} NL_SOCK_t;

// Network device stats
typedef struct _NET_DEV_STATS {
    unsigned long long rx_packets;  // packets received
    unsigned long long tx_packets;  // packets transmitted
    unsigned long long rx_bytes;    // bytes received
    unsigned long long tx_bytes;    // bytes transmitted
    unsigned long rx_errors;        // bad packets received
    unsigned long tx_errors;        // packet transmit problems
    unsigned long rx_dropped;       // no space in linux buffers
    unsigned long tx_dropped;       // no space available in linux
    unsigned long rx_multicast;     // multicast packets received
    unsigned long rx_compressed;    // compressed packets received
    unsigned long tx_compressed;    // compressed packets sent
    unsigned long collisions;       // collisions detected
    unsigned long rx_fifo_errors;   // recv'r fifo overrun
    unsigned long tx_fifo_errors;   // tx fifo overrun
    unsigned long rx_frame_errors;  // recv'd frame alignment error
    unsigned long tx_carrier_errors;// no carrier errors
} NET_DEV_STATS_t;

typedef struct _UDP_PAYLOAD {
    char *dip;
    int sport;
    int dport;
    char *data;
    int len;
} UDP_PAYLOAD_t;

// Data structure passed to UTIL_IF_ENUM_CB_t callback when interface
// enumeration function is called with UTIL_IF_ENUM_EXT_DATA flag
typedef struct _IF_ENUM_CB_EXT_DATA {
    int flags;          // flags describing the data
#define IF_ENUM_CB_ED_FLAGS_WAN 0x00000001 // the data is for WAN intrface
#define IF_ENUM_CB_ED_FLAGS_IFT 0x00000002 // if_type field present
    void *user_payload; // ptr passed by user to util_enum_ifs()
    int if_type;        // IF_ENUM_CB_ED_IFT_... type bitmap
#define IF_ENUM_CB_ED_IFT_PRIMARY 0x00000001 // primary LAN interface
#define IF_ENUM_CB_ED_IFT_GUEST   0x00000002 // guest LAN interface
//#define IF_ENUM_CB_ED_IFT_BRIDGE  0x00000004 // bridge interface
//#define IF_ENUM_CB_ED_IFT_ENET    0x00000008 // Ethernet interface
//#define IF_ENUM_CB_ED_IFT_DOCSIS  0x00000008 // DOCSIS interface
} IF_ENUM_CB_EXT_DATA_t;

// Checks if the interface is administratively UP
// ifname - the interface name
// Returns: TRUE - interface is up, FALSE - down or error
int util_net_dev_is_up(char *ifname);

// Checks if the interface reports link UP
// ifname - the interface name
// Returns: TRUE - interface link is up, FALSE - down or error
int util_net_dev_link_is_up(char *ifname);

// Enumerite the list of the IP interfaces. For each interface a caller's
// callback is invoked until all the interfaces are enumerated.
// Returns: 0 - success, number of times the callback has failed
// flags - flags controlling the enumerator behavior
// f - callback function to invoke per interface
// payload - payload to pass to the callback function
#define UTIL_IF_ENUM_RTR_LAN  0x00000001 // all IP routing LAN interfaces
#define UTIL_IF_ENUM_RTR_WAN  0x00000002 // IP routing WAN interface
#define UTIL_IF_ENUM_EXT_DATA 0x00000004 // see IF_ENUM_CB_EXT_DATA_t above
typedef int (*UTIL_IF_ENUM_CB_t)(char *, void *); // Callback function type
int util_enum_ifs(int flags, UTIL_IF_ENUM_CB_t f, void *payload);

// The platform might provide its own interface enumerator that is used
// in the util_enum_ifs() for enumerating LAN and/or WAN interfaces
// unless they are explicitly specified on the command line. If the
// platform does not provide its own implementation the common stub
// that uses required PLATFORM_GET_MAIN_WAN_NET_DEV() and
// PLATFORM_GET_MAIN_LAN_NET_DEV() macros is called.
// Note: When util_enum_ifs() is called with UTIL_IF_ENUM_EXT_DATA
//       flag it pre-fills IF_ENUM_CB_EXT_DATA_t structure and passes
//       its pointer as the payload. See the util_enum_ifs() to
//       find out what is pre-filled. The old platforms that do not
//       need UTIL_IF_ENUM_EXT_DATA can continue to act as simple
//       pass-through for the payload pointer. The new platforms should
//       complete filling in the IF_ENUM_CB_EXT_DATA_t structure before
//       passing it over to the interface enumerator callback.
int util_platform_enum_ifs(int flags, UTIL_IF_ENUM_CB_t f, void *payload);

// Get the MAC address (binary) of a network device.
// The mac buffer length should be at least 6 bytes.
// If mac is NULL just executes ioctl and reports success or error.
// Returns 0 if successful, error code if fails.
int util_get_mac(char *dev, unsigned char *mac);

// Get the IPv4 configuration of a network device.
// Returns 0 if successful, error code if fails.
int util_get_ipcfg(char *dev, DEV_IP_CFG_t *ipcfg);

// Get device base MAC (format xx:xx:xx:xx:xx:xx, stored at a static location)
char *util_device_mac();

// Get the IPv4 address (as a string) of a network device.
// The buf length should be at least INET_ADDRSTRLEN bytes
// If buf is NULL it is not used.
// Returns 0 if successful, error code if fails.
int util_get_ipv4(char *dev, char *buf);

// Using getaddrinfo we do a name lookup and then
// *res will be set to the first ip4 sockaddr (or ignored if NULL)
// return negative number on error
int util_get_ip4_addr(char *name, struct sockaddr *res);

// Sends out an ICMP packet to a host and waits for a reply
// If the timeout is set to 0, we do not wait for the reply
// Returns the time it took in milliseconds, or -1 if error
//
// Bug: Currently does not work with ipv6
int util_ping(struct sockaddr *s_addr, int timeout_in_sec);

// Get network device statistics/counters.
// Returns 0 if successful, error code if fails.
int util_get_dev_stats(char *dev, NET_DEV_STATS_t *st);

// Send ARP query (just sends the packet)
int util_send_arp_query(char *ifname, IPV4_ADDR_t *tgt);

// Extract a record name from a DNS/mDNS packet
int extract_dns_name(void *pkt, unsigned char *ptr, int max,
                     char *name, int name_len, int level, int to_lower);

// Send UDP packet
// Returns: 0 - if successful, or error code
int send_udp_packet(char *ifname, UDP_PAYLOAD_t *payload);

// Calculates the checksum to be used with TCP & IP packets
unsigned short util_ip_cksum(unsigned short *addr, int len);

// Get default gateway IPv4 address
// Returns: 0 - if gw_ip is populated w/ the gateway IP address,
//          negative error code otherwise
int util_get_ipv4_gw(IPV4_ADDR_t *gw_ip);

// Get default route outbound interface name
// ifnam - ptr to the buffer that receives the interface name
//         the buffer has to be at least IFNAMSIZ long
// Returns: 0 - if gw_ip is populated w/ the gateway IP address,
//          negative error code otherwise
int util_get_ipv4_gw_oif(char *ifnam);

// Find a monitored LAN interface for sending traffic to the specified IP
// address. Parameters:
// ip - pointer to IP address to lookup a match for
// ifn_buf - at least IFNAMSIZ long buffer to return the interface name in
// Returns: 0 - ok, negative - none found, positive - more than 1 found
int util_find_if_by_ip(IPV4_ADDR_t *ip, char *ifn_buf);

#endif // _UTIL_NET_H
