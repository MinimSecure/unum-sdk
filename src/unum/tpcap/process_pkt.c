// (c) 2017-2018 minim.co
// captured packet processing code

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Magic pointer replacing ptbl entry pointer while it is in use
PKT_PROC_ENTRY_t null_entry = {};
#define PTBL_IN_USE_MAGIC (&null_entry)

// Packet processing table
static PKT_PROC_ENTRY_t *ptbl[MAX_PKT_PROC_ENTRIES];


// Adds entry to the packet processing table
// Note: no check for adding the entry multiple times!
// pe - the pointer to the entry to add
//      (the entry must not be changed after it is added)
// Returns: 0 - if successful
int tpcap_add_proc_entry(PKT_PROC_ENTRY_t *pe)
{
    int ii, ret = -1;

    // Try to add the entry
    for(ii = 0; ii < MAX_PKT_PROC_ENTRIES; ii++)
    {
        if(__sync_bool_compare_and_swap(&(ptbl[ii]), NULL, pe))
        {
            ret = 0;
            break;
        }
    }

    return ret;
}

// Delete entry from the packet processing table
// pe - the pointer to the entry to remove
// Returns: nothing
void tpcap_del_proc_entry(PKT_PROC_ENTRY_t *pe)
{
    int ii;
    int magic_found;
    int entry_found;

    for(;;)
    {
        magic_found = FALSE;
        entry_found = FALSE;
        for(ii = 0; ii < MAX_PKT_PROC_ENTRIES; ii++)
        {
            if(__sync_bool_compare_and_swap(&(ptbl[ii]), pe, NULL))
            {
                entry_found = TRUE;
                break;
            }
            if(ptbl[ii] == PTBL_IN_USE_MAGIC) {
                magic_found = TRUE;
            }
        }
        // Continue trying until we either remove the entry or for
        // sure cannot find (not in the table and no entries are in use)
        if(entry_found || ! magic_found) {
            break;
        }
        // Sleep a little bit and try again
        util_msleep(100);
    }

    return;
}

// Check packet against the pkt proc table entry
// Returns: TRUE if match (processing function(s) called),
//          FALSE otherwise
static int tpcap_match_packet(TPCAP_IF_t *tpif, struct tpacket2_hdr *thdr,
                              struct ethhdr *ehdr, PKT_PROC_ENTRY_t *pe)
{
    unsigned int ef, ipf, tuf;
    int match, m;

    // Store flags for reasy access
    ef = pe->flags_eth;
    ipf = pe->flags_ip;
    tuf = pe->flags_tcpudp;

    // Start with match TRUE if any of the flags are set
    match = ((ef | ipf | tuf) != 0);
    if(!match) {
        return FALSE;
    }

    // We are guaranteed to have the Ethernet II header, no need to check
    // snap length to be sure all the Ethernet header data is there.

    // Match the Ethernet header
    if(ef != 0)
    {
        m = TRUE;
        // Match addressses
        m &= ((ef & PKT_MATCH_ETH_MAC_SRC) == 0 ||
              memcmp(ehdr->h_source, pe->eth.mac, ETH_ALEN) == 0);
        m &= ((ef & PKT_MATCH_ETH_MYMAC_DST) == 0 ||
              memcmp(ehdr->h_dest, tpif->mac, ETH_ALEN) == 0);
        m &= ((ef & PKT_MATCH_ETH_MAC_DST) == 0 ||
              memcmp(ehdr->h_dest, pe->eth.mac, ETH_ALEN) == 0);
        m &= ((ef & PKT_MATCH_ETH_MAC_ANY) == 0 ||
              memcmp(ehdr->h_source, pe->eth.mac, ETH_ALEN) == 0 ||
              memcmp(ehdr->h_dest, pe->eth.mac, ETH_ALEN) == 0);
        m &= ((ef & PKT_MATCH_ETH_UCAST_SRC) == 0 ||
              (*(ehdr->h_source) & 1) == 0);
        m &= ((ef & PKT_MATCH_ETH_UCAST_DST) == 0 ||
              (*(ehdr->h_dest) & 1) == 0);
        m &= ((ef & PKT_MATCH_ETH_UCAST_ANY) == 0 ||
              ((*(ehdr->h_source) & *(ehdr->h_dest)) & 1) == 0);
        m &= ((ef & PKT_MATCH_ETH_MCAST_SRC) == 0 ||
              (*(ehdr->h_source) & 1) != 0);
        m &= ((ef & PKT_MATCH_ETH_MCAST_DST) == 0 ||
              (*(ehdr->h_dest) & 1) != 0);
        m &= ((ef & PKT_MATCH_ETH_MCAST_ANY) == 0 ||
              ((*(ehdr->h_source) | *(ehdr->h_dest)) & 1) != 0);
        // Negate addr match
        match &= ((ef & PKT_MATCH_ETH_MAC_NEG) == 0) ? (m) : (!m);
        // Match the ethertype
        m = ((ef & PKT_MATCH_ETH_TYPE) == 0 || ehdr->h_proto == pe->eth.proto);
        // Negate ethtype match
        match &= ((ef & PKT_MATCH_ETH_TYPE_NEG) == 0) ? (m) : (!m);
        // return if matching has failed
        if(!match) {
            return FALSE;
        }
    }

    // Matching IP header
    struct iphdr *iph = (void *)thdr + thdr->tp_net;

    // No IP header if snap len is too short
    if(thdr->tp_snaplen - (thdr->tp_net-thdr->tp_mac) < sizeof(struct iphdr) ||
       ehdr->h_proto != htons(ETH_P_IP))
    {
        iph = NULL;
    }
    // Make sure we have IP header if checking it or TCP/UDP
    if((ipf | tuf) != 0 && !iph)
    {
        return FALSE;
    }

    if(ipf != 0)
    {
        uint32_t sa = iph->saddr;
        uint32_t da = iph->daddr;
        uint32_t sah = ntohl(iph->saddr);
        uint32_t dah = ntohl(iph->daddr);
        m = TRUE;
        m &= ((ipf & PKT_MATCH_IP_A1_SRC) == 0 || sa == pe->ip.a1.i);
        m &= ((ipf & PKT_MATCH_IP_A1_DST) == 0 || da == pe->ip.a1.i);
        m &= ((ipf & PKT_MATCH_IP_A1_ANY) == 0 ||
              sa == pe->ip.a1.i || da == pe->ip.a1.i);
        m &= ((ipf & PKT_MATCH_IP_A2_SRC) == 0 || sa == pe->ip.a2.i);
        m &= ((ipf & PKT_MATCH_IP_A2_DST) == 0 || da == pe->ip.a2.i);
        m &= ((ipf & PKT_MATCH_IP_A2_ANY) == 0 ||
              sa == pe->ip.a2.i || da == pe->ip.a2.i);
        m &= ((ipf & PKT_MATCH_IP_MY_DST) == 0 || da == tpif->ipcfg.ipv4.i);
        m &= ((ipf & PKT_MATCH_IP_NET_SRC) == 0 ||
              (sa & pe->ip.a2.i) == (pe->ip.a1.i & pe->ip.a2.i));
        m &= ((ipf & PKT_MATCH_IP_NET_DST) == 0 ||
              (da & pe->ip.a2.i) == (pe->ip.a1.i & pe->ip.a2.i));
        m &= ((ipf & PKT_MATCH_IP_NET_ANY) == 0 ||
              (da & pe->ip.a2.i) == (pe->ip.a1.i & pe->ip.a2.i) ||
              (sa & pe->ip.a2.i) == (pe->ip.a1.i & pe->ip.a2.i));
        m &= ((ipf & PKT_MATCH_IP_RNG_SRC) == 0 ||
              (sah >= ntohl(pe->ip.a1.i) && sah <= ntohl(pe->ip.a2.i)));
        m &= ((ipf & PKT_MATCH_IP_RNG_DST) == 0 ||
              (dah >= ntohl(pe->ip.a1.i) && dah <= ntohl(pe->ip.a2.i)));
        m &= ((ipf & PKT_MATCH_IP_RNG_ANY) == 0 ||
              (sah >= ntohl(pe->ip.a1.i) && sah <= ntohl(pe->ip.a2.i)) ||
              (dah >= ntohl(pe->ip.a1.i) && dah <= ntohl(pe->ip.a2.i)));
        // Negate addr match
        match &= ((ipf & PKT_MATCH_IP_ADDR_NEG) == 0) ? (m) : (!m);
        // Match the IP protocol field
        m = ((ipf & PKT_MATCH_IP_PROTO) == 0 || iph->protocol == pe->ip.proto);
        // Negate the protocol match
        match &= ((ipf & PKT_MATCH_IP_PROTO_NEG) == 0) ? (m) : (!m);
        // return if matching has failed
        if(!match) {
            return FALSE;
        }
    }

    struct tcphdr* tcph = ((void *)iph) + sizeof(struct iphdr);
    struct udphdr* udph = ((void *)iph) + sizeof(struct iphdr);

    // Match TCP/UDP header
    if(tuf != 0)
    {
        int minlen;
        // Fail immediately if IP packet is not TCP or UDP
        if(iph->protocol == 6) {
            minlen = sizeof(struct iphdr) + sizeof(*tcph);
        } else if(iph->protocol == 17) {
            minlen = sizeof(struct iphdr) + sizeof(*udph);
        } else {
            return FALSE;
        }
        // Make sure we have full header worth of data
        if(thdr->tp_snaplen - (thdr->tp_net - thdr->tp_mac) < minlen)
        {
            return FALSE;
        }

        // TCP & UDP store ports in the same place. We use host
        // byte order for specifying ports, so will be doing all the
        // checking in the host byte order
        uint16_t sp = ntohs(udph->source);
        uint16_t dp = ntohs(udph->dest);

        m = TRUE;
        m &= ((tuf & PKT_MATCH_TCPUDP_P1_SRC) == 0 || sp == pe->tcpudp.p1);
        m &= ((tuf & PKT_MATCH_TCPUDP_P1_DST) == 0 || dp == pe->tcpudp.p1);
        m &= ((tuf & PKT_MATCH_TCPUDP_P1_ANY) == 0 ||
              sp == pe->tcpudp.p1 || dp == pe->tcpudp.p1);
        m &= ((tuf & PKT_MATCH_TCPUDP_P2_SRC) == 0 || sp == pe->tcpudp.p2);
        m &= ((tuf & PKT_MATCH_TCPUDP_P2_DST) == 0 || dp == pe->tcpudp.p2);
        m &= ((tuf & PKT_MATCH_TCPUDP_P2_ANY) == 0 ||
              sp == pe->tcpudp.p2 || dp == pe->tcpudp.p2);
        m &= ((tuf & PKT_MATCH_TCPUDP_RNG_SRC) == 0 ||
              (sp >= pe->tcpudp.p1 && sp <= pe->tcpudp.p2));
        m &= ((tuf & PKT_MATCH_TCPUDP_RNG_DST) == 0 ||
              (dp >= pe->tcpudp.p1 && dp <= pe->tcpudp.p2));
        m &= ((tuf & PKT_MATCH_TCPUDP_RNG_ANY) == 0 ||
              (sp >= pe->tcpudp.p1 && sp <= pe->tcpudp.p2) ||
              (dp >= pe->tcpudp.p1 && dp <= pe->tcpudp.p2));
        // Negate the port match
        match &= ((tuf & PKT_MATCH_TCPUDP_PORT_NEG) == 0) ? (m) : (!m);
        // Filter specific TCP or UDP
        match &= (!(tuf & PKT_MATCH_TCPUDP_TCP_ONLY) || iph->protocol == 6);
        match &= (!(tuf & PKT_MATCH_TCPUDP_UDP_ONLY) || iph->protocol == 17);
        // return if matching has failed
        if(!match) {
            return FALSE;
        }
    }

    // The match must be TRUE, call the functions and/or
    // follow to the chained processing structure.
    if(pe->eth_func) {
        pe->eth_func(tpif, pe, thdr, ehdr);
    }
    if(pe->ip_func && iph) {
        pe->ip_func(tpif, pe, thdr, iph);
    }
    if(pe->chain) {
        return tpcap_match_packet(tpif, thdr, ehdr, pe->chain);
    }

    return match;
}

// Process a captured packet
// tpif - interface info structure
// thdr - tpacket metadata header
// ehdr - ethernet header of the captured packet
// e_proto - ethertype of the captured packet
//           (only Ethernet II pkts are reported)
// Returns: 0 if happy
int tpcap_process_packet(TPCAP_IF_t *tpif, struct tpacket2_hdr *thdr,
                         struct ethhdr *ehdr)
{
    int ii;

#ifdef DEBUG
    // Test 1, print captured packets info
    if(tpcap_test_param.int_val == TPCAP_TEST_BASIC)
    {
        tpkt_print_pkt_info(thdr, ehdr, NULL);
    }
#endif // DEBUG

    for(ii = 0; ii < MAX_PKT_PROC_ENTRIES; ii++)
    {
        PKT_PROC_ENTRY_t *pe = ptbl[ii];

        if(!pe) {
            continue;
        }
        if(!__sync_bool_compare_and_swap(&(ptbl[ii]),
                                         pe, PTBL_IN_USE_MAGIC))
        {
            continue;
        }
        tpcap_match_packet(tpif, thdr, ehdr, pe);
        __sync_synchronize();
        ptbl[ii] = pe;
    }

    return 0;
}

// Complete the capturing cycle (call stats handlers)
// Returns: 0 if successful
int tpcap_cycle_complete()
{
    int ii;

    for(ii = 0; ii < MAX_PKT_PROC_ENTRIES; ii++)
    {
        PKT_PROC_ENTRY_t *pe = ptbl[ii];

        if(!pe) {
            continue;
        }
        if(!__sync_bool_compare_and_swap(&(ptbl[ii]),
                                         pe, PTBL_IN_USE_MAGIC))
        {
            continue;
        }
        if(pe->stats_func) {
            pe->stats_func(tpcap_get_if_stats());
        }
        __sync_synchronize();
        ptbl[ii] = pe;
    }

    return 0;
}
