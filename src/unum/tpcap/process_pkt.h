// (c) 2017-2018 minim.co
// captured packet processing code include file

#ifndef _PROCESS_PKT_H
#define _PROCESS_PKT_H


// Max entries in the packet processing table
#define MAX_PKT_PROC_ENTRIES 48

// Forward declaration of the _PKT_PROC_ENTRY structure
struct _PKT_PROC_ENTRY;

// Forward declarations to avoid useless warnings
struct tpacket2_hdr;
struct iphdr;
struct ethhdr;

// Packet processing function types
typedef void (*ETH_PROC_FUNC_t)(TPCAP_IF_t *tpif,
                                struct _PKT_PROC_ENTRY *proc_entry,
                                struct tpacket2_hdr *thdr,
                                struct ethhdr *ehdr);
typedef void (*IP_PROC_FUNC_t)(TPCAP_IF_t *tpif,
                               struct _PKT_PROC_ENTRY *proc_entry,
                               struct tpacket2_hdr *thdr,
                               struct iphdr *iph);
typedef void (*STATS_FUNC_t)(TPCAP_IF_STATS_t *st);

// Structure describing packet filtering rules
typedef struct _PKT_PROC_ENTRY
{
// Flags for Ethernet header matching
#define PKT_MATCH_ETH_MAC_SRC   0x00000001 // src MAC = eth.mac
#define PKT_MATCH_ETH_MYMAC_DST 0x00000002 // dst MAC = interface MAC
#define PKT_MATCH_ETH_UCAST_SRC 0x00000004 // src MAC is unicast
#define PKT_MATCH_ETH_MCAST_SRC 0x00000008 // src MAC is multicast
#define PKT_MATCH_ETH_MAC_DST   0x00000010 // src MAC = eth.mac
#define PKT_MATCH_ETH_UCAST_DST 0x00000020 // src MAC is unicast
#define PKT_MATCH_ETH_MCAST_DST 0x00000040 // src MAC is multicast
#define PKT_MATCH_ETH_MAC_ANY   0x00000080 // src|dst MAC = eth.mac
#define PKT_MATCH_ETH_UCAST_ANY 0x00000100 // src|dst MAC is unicast
#define PKT_MATCH_ETH_MCAST_ANY 0x00000200 // src|dst MAC is multicast
#define PKT_MATCH_ETH_TYPE      0x00000400 // ethtype == eth.proto
// Negations (make filter pass if no specified match)
#define PKT_MATCH_ETH_MAC_NEG   0x10000000 // negating the MAC match
#define PKT_MATCH_ETH_TYPE_NEG  0x20000000 // negating the type match
    unsigned int flags_eth;
    struct _PKT_PROC_ENTRY_ETH_PARAMS {
        unsigned char mac[ETH_ALEN];
        uint16_t proto; // in network byte order
    } eth; // Ethernet header matching parameters


// Flags for IP header matching
#define PKT_MATCH_ALL_IP       0x00000001 // any IP packet
#define PKT_MATCH_IP_A1_SRC    0x00000002 // src IP = ip.a1
#define PKT_MATCH_IP_A1_DST    0x00000004 // dst IP = ip.a1
#define PKT_MATCH_IP_A1_ANY    0x00000008 // src|dst IP = ip.a1
#define PKT_MATCH_IP_A2_SRC    0x00000010 // src IP = ip.a2
#define PKT_MATCH_IP_A2_DST    0x00000020 // dst IP = ip.a2
#define PKT_MATCH_IP_A2_ANY    0x00000040 // src|dst IP = ip.a1
#define PKT_MATCH_IP_MY_DST    0x00000080 // dst IP = interface IP
#define PKT_MATCH_IP_NET_SRC   0x00000100 // src IP in ip.a1/ip.a2 subnet
#define PKT_MATCH_IP_NET_DST   0x00000200 // dst IP in ip.a1/ip.a2 subnet
#define PKT_MATCH_IP_NET_ANY   0x00000400 // dst IP in ip.a1/ip.a2 subnet
#define PKT_MATCH_IP_RNG_SRC   0x00000800 // src IP in ip.a1-ip.a2 range
#define PKT_MATCH_IP_RNG_DST   0x00001000 // dst IP in ip.a1-ip.a2 range
#define PKT_MATCH_IP_RNG_ANY   0x00002000 // dst IP in ip.a1-ip.a2 range
#define PKT_MATCH_IP_PROTO     0x00004000 // proto = ip.proto
// Negations (make filter pass if no specified match)
#define PKT_MATCH_IP_ADDR_NEG  0x10000000 // negating the addresses match
#define PKT_MATCH_IP_PROTO_NEG 0x20000000 // negating the IP proto match
    unsigned int flags_ip;
    struct _PKT_PROC_ENTRY_IP_PARAMS {
        union {
            unsigned char b[4];
            unsigned int  i; // in network byte order
        } a1;
        union {
            unsigned char b[4];
            unsigned int  i; // in network byte order
        } a2;
        uint8_t proto;
    } ip; // IPv4 protocol matching parameters

// Flags for TCP/UDP matching
#define PKT_MATCH_TCPUDP_P1_SRC   0x00000001 // src port = tcpudp.p1
#define PKT_MATCH_TCPUDP_P1_DST   0x00000002 // dst port = tcpudp.p1
#define PKT_MATCH_TCPUDP_P1_ANY   0x00000004 // src|dst port = tcpudp.p1
#define PKT_MATCH_TCPUDP_P2_SRC   0x00000008 // src port = tcpudp.p2
#define PKT_MATCH_TCPUDP_P2_DST   0x00000010 // dst port = tcpudp.p2
#define PKT_MATCH_TCPUDP_P2_ANY   0x00000020 // src|dst port = tcpudp.p2
#define PKT_MATCH_TCPUDP_RNG_SRC  0x00000040 // src port in tcpudp.p1-tcpudp.p2
#define PKT_MATCH_TCPUDP_RNG_DST  0x00000080 // dst port in tcpudp.p1-tcpudp.p2
#define PKT_MATCH_TCPUDP_RNG_ANY  0x00000100 // dst port in tcpudp.p1-tcpudp.p2
#define PKT_MATCH_TCPUDP_TCP_ONLY 0x00000200 // only match if TCP
#define PKT_MATCH_TCPUDP_UDP_ONLY 0x00000400 // only match if UDP
// Negations (make filter pass if no specified match)
#define PKT_MATCH_TCPUDP_PORT_NEG 0x10000000 // negating the port match
    unsigned int flags_tcpudp;
    struct _PKT_PROC_ENTRY_TCPUDP_PARAMS {
        uint16_t p1; // in HOST byte order
        uint16_t p2; // in HOST byte order
    } tcpudp; // TCP/UDP matching parameters

    ETH_PROC_FUNC_t eth_func; // function to call on Eth pkt capture, or NULL
    IP_PROC_FUNC_t ip_func;   // function to call on IP pkt capture, or NULL
    STATS_FUNC_t stats_func;  // function to report capture stats (per interval)
    struct _PKT_PROC_ENTRY* chain; // If not NULL examine the chained matches
    char *desc; // Optional description string
} PKT_PROC_ENTRY_t;


// Adds entry to the packet processing table
// pe - the pointer to the entry to add
//      (the entry must not be changed after it is added)
// Returns: 0 - if successful
int tpcap_add_proc_entry(PKT_PROC_ENTRY_t *pe);

// Delete entry from the packet processing table
// pe - the pointer to the entry to remove
// Returns: nothing
void tpcap_del_proc_entry(PKT_PROC_ENTRY_t *pe);

// Process a captured packet (only Ethernet II pkts are reported)
// tpif - interface info structure
// thdr - tpacket metadata header
// ehdr - ethernet header of the captured packet
// Returns: 0 if happy
int tpcap_process_packet(TPCAP_IF_t *tpif, struct tpacket2_hdr *thdr,
                         struct ethhdr *ehdr);

// Complete the capturing cycle (call stats handlers)
// Returns: 0 if successful
int tpcap_cycle_complete(void);

#endif // _PROCESS_PKT_H

