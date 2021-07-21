// (c) 2017-2019 minim.co
// device telemetry information collector

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


#ifdef DEBUG
#define DPRINTF(args...) ((tpcap_test_param.int_val == TPCAP_TEST_DT) ? \
                          (printf(args)) : 0)
#else  // DEBUG
#define DPRINTF(args...) /* Nothing */
#endif // DEBUG


// Forward declarations
static void stats_ready_cb(TPCAP_IF_STATS_t *st);
static void ip_pkt_rcv_cb(TPCAP_IF_t *tpif,
                          PKT_PROC_ENTRY_t *pe,
                          struct tpacket2_hdr *thdr,
                          struct iphdr *iph);

// All (from or to unicast MAC) IP packets processing entry for
// device & connection tracking
static PKT_PROC_ENTRY_t collector_all_ip = {
    PKT_MATCH_ETH_UCAST_ANY,
    {},
    PKT_MATCH_ALL_IP,
    {},
    0,
    {},
    NULL, ip_pkt_rcv_cb, stats_ready_cb, NULL,
    "Captures IP packets for device and connection tracking, "
    "finalizes devices telemetery and stats collection"
};

#ifdef FEATURE_FESTATS_ONLY
// When festats_only feature flag is enabled, the packets from tpcap shouldn't
// be used for connections tracking. However, everything else including
// collecting and reporting various statistics at the end of the capturing
// interval should continue working as before.
static PKT_PROC_ENTRY_t collector_stats_cb_only = {
    0, {}, 0, {}, 0, {},  // Don't match any packets
    NULL, NULL, stats_ready_cb, NULL,
    "finalizes devices telemetery and stats collection"
};
#endif // FEATURE_FESTATS_ONLY

// Capturing interval counter. Incremented at the end of the
// capturing when stats are reported to the devtelemetry subsystem.
static int cap_iteration = 0;

// Discovered device table (uses pointers)
static DT_DEVICE_t **dev_tbl;
// Connections table (uses pointers)
static DT_CONN_t **conn_tbl;
// Interface counters and capturing stats (direct placement, static allocation)
static DT_IF_STATS_t stats_tbl[DEVTELEMETRY_NUM_SLICES][TPCAP_STAT_IF_MAX];

// Structures tracking the device and connection tables stats
static DT_TABLE_STATS_t dev_tbl_stats;
static DT_TABLE_STATS_t conn_tbl_stats;

#ifndef FEATURE_LAN_ONLY
// 25 bits of IP mcast MAC in network order for matching mcast destinations
static uint32_t *ip_mc_mac =
    (uint32_t *)((char []){ 0x01, 0x00, 0x5E, 0x00 });
static uint32_t *ip_mc_mac_mask =
    (uint32_t *)((char []){ 0xff, 0xff, 0xff, 0x80 });
#endif // !FEATURE_LAN_ONLY


// Reset device telemetry main tables (including the table usage stats)
static void dt_reset_dev_tables(void)
{
    int ii;

    // Reset devices table (keeping already allocated items memory)
    for(ii = 0; ii < DTEL_MAX_DEV; ii++) {
        DT_DEVICE_t *item = dev_tbl[ii];
        if(item) {
            memset(item, 0, sizeof(DT_DEVICE_t));
        }
    }
    // Reset name table (keeping already allocated items memory)
    for(ii = 0; ii < DTEL_MAX_CONN; ii++) {
        DT_CONN_t *item = conn_tbl[ii];
        if(item) {
            item->hdr.dev = NULL;
        }
    }
    // Reset stats
    dt_dev_tbl_stats(TRUE);
    dt_conn_tbl_stats(TRUE);
    return;
}

// Add to the connection table or find (if already there) the matching
// one and return the pointer to the table entry (NULL if not there and
// cannot add). If the connection is added the header is copied over
// and the rest of the connection structure zeroed. The pointer to the
// last added connection item in the owner-device structure and the
// device connections chain pointer are updated.
static DT_CONN_t *add_conn(DT_CONN_HDR_t *hdr)
{
    int ii, idx;
    DT_CONN_t *ret = NULL;

    // Total # of add requests
    ++(conn_tbl_stats.add_all);

    // Generate hash-based index
    idx = util_hash(hdr, sizeof(DT_CONN_HDR_t)) % DTEL_MAX_CONN;

    for(ii = 0; ii < (DTEL_MAX_CONN / DT_SEARCH_LIMITER); ii++) {
        // If we hit an empty entry then the connection we are looking for
        // is not in the table (we NEVER remove inidividual entries)
        if(!conn_tbl[idx]) {
            ret = malloc(sizeof(DT_CONN_t));
            if(!ret) { // Run out of memory
                ++(conn_tbl_stats.add_limit);
                return NULL;
            }
            ret->hdr.dev = NULL;
            conn_tbl[idx] = ret;
        }
        if(conn_tbl[idx]->hdr.dev == NULL) {
            ret = conn_tbl[idx];
            memcpy(&(ret->hdr), hdr, sizeof(DT_CONN_HDR_t));
            memset((void *)ret + sizeof(DT_CONN_HDR_t), 0,
                   sizeof(DT_CONN_t) - sizeof(DT_CONN_HDR_t));
            ret->hdr.dev->last_conn->next = ret;
            ret->hdr.dev->last_conn = ret;
            break;
        }
        // Check if this is the same connection
        if(memcmp(&(conn_tbl[idx]->hdr), hdr, sizeof(DT_CONN_HDR_t)) == 0) {
            ret = conn_tbl[idx];
            ++(conn_tbl_stats.add_found);
            break;
        }
        // Collision, try the next index
        idx = (idx + 1) % DTEL_MAX_CONN;
    }

    if(ii < 10) {
        ++(conn_tbl_stats.add_10);
    }
    if(!ret) {
        ++(conn_tbl_stats.add_busy);
    }

    return ret;
}

// Add to the devices table or find (if already there) a MAC address
// and return the pointer to the table entry (NULL if not there and
// cannot add). The rating paramter allows to override already taken
// entry with the lower rating if no more space in the table.
// Upon return the new and replaced entries are wiped clean and
// only the new MAC and timestamp are updated then (rating is not set).
// The found  entries are returned as-is.
static DT_DEVICE_t *add_dev(unsigned char *mac, int rating)
{
    int ii, idx;
    int p_idx, p_rating;
    unsigned long p_t_add;
    unsigned long tt = util_time(10);
    DT_DEVICE_t *ret = NULL;

    // Total # of add requests
    ++(dev_tbl_stats.add_all);

    // Generate hash-based index
    idx = util_hash(mac, 6) % DTEL_MAX_DEV;

    // The device table is relatively small, so we will examine the whole
    // table (if necessary) to find an entry or a free row.
    // If we have no free space then a device with the same or lower rating
    // (based on what we've seen) will be replaced with the more recent device.
    p_idx = p_rating = -1;
    p_t_add = 0;
    for(ii = 0; ii < DTEL_MAX_DEV; ii++) {
        // If we hit a free entry then the device we are looking for
        // is not in the table (since we NEVER free entries taken).
        if(!dev_tbl[idx]) {
            ret = malloc(sizeof(DT_DEVICE_t));
            if(!ret) { // Run out of memory
                ++(dev_tbl_stats.add_limit);
                return NULL;
            }
            memset(ret, 0, sizeof(DT_DEVICE_t));
            dev_tbl[idx] = ret;
        }
        if(dev_tbl[idx]->rating == 0) {
            ret = dev_tbl[idx];
            memcpy(ret->mac, mac, sizeof(ret->mac));
            ret->t_add = tt;
            ret->last_conn = &(ret->conn);
            break;
        }
        // Check if this is our MAC address entry
        if(memcmp(dev_tbl[idx]->mac, mac, 6) == 0) {
            ret = dev_tbl[idx];
            ++(dev_tbl_stats.add_found);
            break;
        }
        // If entry has a lower rating (or the same rating but is older) then
        // it is a candidate for replacement. Remember it unless already have
        // a better candidate.
        if(dev_tbl[idx]->rating < rating ||
           (dev_tbl[idx]->rating == rating && dev_tbl[idx]->t_add < tt))
        {
            if(p_idx < 0 || dev_tbl[idx]->rating < p_rating ||
               (dev_tbl[idx]->rating==p_rating && dev_tbl[idx]->t_add<p_t_add))
            {
                p_idx = idx;
                p_rating = dev_tbl[idx]->rating;
                p_t_add = dev_tbl[idx]->t_add;
            }
        }
        // Check the next index
        idx = (idx + 1) % DTEL_MAX_DEV;
    }

    if(ii < 10) {
        ++(dev_tbl_stats.add_10);
    }
    if(!ret) {
        // Use the replacement entry if available
        if(p_idx >= 0) {
            ret = dev_tbl[p_idx];
            memset(ret, 0, sizeof(DT_DEVICE_t));
            memcpy(ret->mac, mac, sizeof(ret->mac));
            ret->t_add = tt;
            ret->last_conn = &(ret->conn);
            ++(dev_tbl_stats.add_repl);
        } else {
            ++(dev_tbl_stats.add_busy);
        }
    }

    return ret;
}

// Generic update connection function for both tpcap and festats.
// It returns the pointer to the connection if it is added or found.
static DT_CONN_t *ip_upd_conn(DT_DEVICE_t *dev,
                              DT_CONN_HDR_t *hdr)
{
    DT_CONN_t *conn = &(dev->conn);

    // See if we can use connection entry combined w/ the device
    if(conn->hdr.dev == NULL) {
        memcpy(&(conn->hdr), hdr, sizeof(DT_CONN_HDR_t));
        // The pointer to last_conn is set to combined connection
        // entry by default, the connection entry is also zeroed
        // when the device is added, so nothing else to do here.
    } else if(!memcmp(hdr, &(conn->hdr), sizeof(DT_CONN_HDR_t))) {
        // Nothing extra to do if the combined connection matches
    } else { // Try to add a new connection to the table
        conn = add_conn(hdr);
    }
    if(conn == NULL) {
        return NULL;
    }

#ifdef DT_CONN_MAP_IP_TO_DNS
    // Update the DNS info
    if(!conn->dns_ip) {
        conn->dns_ip = dt_find_dns_ip(&(conn->hdr.ipv4));
        if(conn->dns_ip) {
            conn->dns_ip->refs++;
        }
    }
#endif // DT_CONN_MAP_IP_TO_DNS

    // Update the connection counters
    ++(conn->upd_total);

    return conn;
}

// Updates connection info with the data from the provided
// packet capture headers. This helper is called from the
// IP packet receive callback (below) only.
static void ip_pkt_upd_conn(DT_DEVICE_t *dev,
                            DT_CONN_HDR_t *hdr,
                            unsigned long bytes_in,
                            unsigned long bytes_out,
                            uint16_t syn_tcp_win_size,
                            uint16_t cur_tcp_win_size)
{
    DT_CONN_t *conn = ip_upd_conn(dev, hdr);

    if(conn == NULL) {
        DPRINTF("%s: unable to add IP pkt connection "
                IP_PRINTF_FMT_TPL " <-> " IP_PRINTF_FMT_TPL " \n",
                __func__, IP_PRINTF_ARG_TPL(dev->ipv4.b),
                IP_PRINTF_ARG_TPL(hdr->ipv4.b));
        return;
    }

    if(conn->syn_tcp_win_size != 0) {
        conn->syn_tcp_win_size = syn_tcp_win_size;
    }
    if(cur_tcp_win_size != 0) {
        conn->cur_tcp_win_size = cur_tcp_win_size;
    }

    int slice_num = cap_iteration % DEVTELEMETRY_NUM_SLICES;
    conn->bytes_to[slice_num] += bytes_in;
    conn->bytes_from[slice_num] += bytes_out;

    return;
}

// IP packet receive callback, called by tpcap thread for every
// packet it captures.
// Do not block or wait for anything here.
// Do not log anything from here.
static void ip_pkt_rcv_cb(TPCAP_IF_t *tpif,
                          PKT_PROC_ENTRY_t *pe,
                          struct tpacket2_hdr *thdr,
                          struct iphdr *iph)
{
    unsigned char *dev_mac;
    IPV4_ADDR_t dev_ipv4;
    IPV4_ADDR_t peer_ipv4;
    unsigned long bytes_in, bytes_out;
    struct ethhdr *ehdr = (struct ethhdr *)((void *)thdr + thdr->tp_mac);
    int from_rtr = TRUE;
    int to_rtr = TRUE;

    if(IS_OPM(UNUM_OPM_AP)) {

#ifdef FEATURE_GUEST_NAT
        // We are interested in packets only from guest network
        // Return if it is not a guest network
        // The guest_nat feature is used for the devices that double-NAT
        // guest traffic when in AP mode.
        // The agent has to watch the traffic on the AP guest interfaces
        // to know which device it originates from.
        if((tpif->if_type & IF_ENUM_CB_ED_IFT_GUEST) == 0)
#endif // FEATURE_GUEST_NAT
        {

            return;
        }
    }
#ifndef FEATURE_LAN_ONLY
    // Is the packet from or to the router MAC address
    // If it is device->IP mcast pkt - assume to-the-router
    from_rtr = !memcmp(tpif->mac, ehdr->h_source, 6);
    void *mac_ptr = ehdr->h_dest;
    to_rtr = !memcmp(tpif->mac, ehdr->h_dest, 6) ||
                     (!from_rtr && (*((uint32_t*)mac_ptr) & *ip_mc_mac_mask) ==
                                   *ip_mc_mac);
#endif // !FEATURE_LAN_ONLY

#ifdef FEATURE_LAN_ONLY
    // Use subnet match to figure if the traffic is to or from outside.
    // For the AP-only mode to work we should use the subnet match method
    // (currently this is the only way to identify the traffic direction).
    // For the managed devices (which are also LAN only devices) we can
    // just match the MAC to src or dst, but for consistency will just use
    // the subnet match method.
    if((iph->saddr & tpif->ipcfg.ipv4mask.i) ==
       (tpif->ipcfg.ipv4.i & tpif->ipcfg.ipv4mask.i))
    {
        from_rtr = FALSE;
    }
    if((iph->daddr & tpif->ipcfg.ipv4mask.i) ==
       (tpif->ipcfg.ipv4.i & tpif->ipcfg.ipv4mask.i))
    {
        to_rtr = FALSE;
    }
    // In the AP mode we cannot see traffic of the devices that are not
    // sending through the AP's bridge interface, but we could see their
    // multicast/broadcast traffic and unless filtered would report false
    // info and pretend we are monitoring them, so kill all the local LAN
    // multicasts here.
    if(!from_rtr && (*ehdr->h_dest & 0x01) != 0) {
        //DPRINTF("%s: packet ignored, local mcast to "
        //        MAC_PRINTF_FMT_TPL "\n",
        //        __func__, MAC_PRINTF_ARG_TPL(ehdr->h_dest));
        return;
    }
#ifdef FEATURE_MANAGED_DEVICE
    // For managed devices drop all the traffic unrelated to the device itself
    if(memcmp(tpif->mac, ehdr->h_source, 6) &&
       memcmp(tpif->mac, ehdr->h_dest, 6))
    {
        //DPRINTF("%s: packet src & dst do not match the interface MAC\n",
        //        __func__);
        return;
    }
#endif //!FEATURE_MANAGED_DEVICE
#endif // FEATURE_LAN_ONLY

    // Only take packets that go from some device to the
    // outside or back (i.e. try to go through the router)
    if(!(to_rtr ^ from_rtr)) {
        DPRINTF("%s: packet ignored (to_rtr: %d, from_rtr: %d)\n",
                __func__, to_rtr, from_rtr);
        return;
    }
    // If the packet has the same src and dst MAC drop it
    if(!memcmp(ehdr->h_dest, ehdr->h_source, 6)) {
        DPRINTF("%s: packet ignored, same SRC/DST MAC"
                MAC_PRINTF_FMT_TPL "\n",
                __func__, MAC_PRINTF_ARG_TPL(ehdr->h_dest));
        return;
    }

    // We will also catch the null-MAC.

    // Collect the basic information about the packet
    if(to_rtr) {
        bytes_in = 0;
        bytes_out = thdr->tp_len;
        dev_mac = ehdr->h_source;
        dev_ipv4.i = iph->saddr;
        peer_ipv4.i = iph->daddr;
    } else {
        bytes_out = 0;
        bytes_in = thdr->tp_len;
        dev_mac = ehdr->h_dest;
        dev_ipv4.i = iph->daddr;
        peer_ipv4.i = iph->saddr;
    }

#ifndef FEATURE_LAN_ONLY
    // Drop packets that go to or come from the router/AP itself
    // This can only be seen in the router mode
    if(peer_ipv4.i == tpif->ipcfg.ipv4.i) {
        //DPRINTF("%s: packet to/from router " IP_PRINTF_FMT_TPL "\n",
        //        __func__, IP_PRINTF_ARG_TPL(peer_ipv4.b));
        return;
    }
#endif // !FEATURE_LAN_ONLY

#ifdef FEATURE_LAN_ONLY
    // Skip this check for managed devices, they must report their own traffic
#ifndef FEATURE_MANAGED_DEVICE
    // In the AP-only firmware we will only see our own traffic to outside of
    // the network (might be useful, but is inconsistent w/ the router mode)
    if(dev_ipv4.i == tpif->ipcfg.ipv4.i) {
        //DPRINTF("%s: packet to/from AP " IP_PRINTF_FMT_TPL "\n",
        //        __func__, IP_PRINTF_ARG_TPL(dev_ipv4.b));
        return;
    }
#endif //!FEATURE_MANAGED_DEVICE
#endif // FEATURE_LAN_ONLY

    // First add or make sure device is in the devices table
    uint16_t rating = ((to_rtr | (from_rtr << 1))) << 1;
    DT_DEVICE_t *dev = add_dev(dev_mac, rating);
    if(!dev) {
        DPRINTF("%s: can't add or find " MAC_PRINTF_FMT_TPL "\n",
                __func__, MAC_PRINTF_ARG_TPL(dev_mac));
        return;
    }

    // Update the interface and the device IPv4 address
    dev->ipv4.i = dev_ipv4.i;
    strncpy(dev->ifname, tpif->name, IFNAMSIZ-1);
    dev->ifname[IFNAMSIZ-1] = '\0';

    // Prepare header for the connection info table entry
    DT_CONN_HDR_t hdr;
    hdr.ipv4.i = peer_ipv4.i;
    hdr.proto = iph->protocol;
    hdr.rev = FALSE;
    hdr.port = 0;
    hdr.dev = dev;

    uint16_t syn_tcp_win_size = 0;
    uint16_t cur_tcp_win_size = 0;

    // Work out tcp/udp and what port to report. If the device port is
    // less than 1024 we report it, otherwise report peer's port.
    // If the packet is corrupt override protocol w/ 0.
    if(hdr.proto == 6 || hdr.proto == 17) {
        struct tcphdr* tcph = ((void *)iph) + sizeof(struct iphdr);
        struct udphdr* udph = ((void *)iph) + sizeof(struct iphdr);
        int have_net_len = thdr->tp_snaplen - (thdr->tp_net - thdr->tp_mac);
        int need_net_len = sizeof(struct iphdr);
        if(hdr.proto == 6) {
            need_net_len += sizeof(*tcph);
        } else if(hdr.proto == 17) {
            need_net_len += sizeof(*udph);
        }
        if(need_net_len <= have_net_len) {
            // The ports are in the same place for TCP & UDP
            uint16_t sp = ntohs(udph->source);
            uint16_t dp = ntohs(udph->dest);
            if(to_rtr) {
                hdr.rev = (sp < 1024 && dp >= 1024);
                hdr.port = hdr.rev ? sp : dp;
            } else { // pkt from the router
                hdr.rev = (dp < 1024 && sp >= 1024);
                hdr.port = hdr.rev ? dp : sp;
            }
            if(hdr.proto == 6) {
                cur_tcp_win_size = ntohs(tcph->window);
                // Only capture win size if SYN or SYN|ACK pkt from the device
                if(tcph->syn && to_rtr) {
                    syn_tcp_win_size = cur_tcp_win_size;
                }
            }
        } else {
            hdr.port = 0;
            hdr.proto = 0;
            hdr.rev = FALSE;
        }
    }

    // Update or add connection information
    ip_pkt_upd_conn(dev, &hdr, bytes_in, bytes_out,
                    syn_tcp_win_size, cur_tcp_win_size);

    // Finally update the device rating, we should have:
    // 000 - new device
    // 001 - N/A
    // 010 - pkt out (from device)
    // 011 - multiple pkts out (from device)
    // 100 - pkt in (to device)
    // 101 - multiple pkts in (to device)
    // 110 - pkt in and pkt out
    // 111 - pkts in/out
    if(dev->rating == 0) {
        // 100 - packet is in, 010 - out
        dev->rating = rating;
    } else if(dev->rating < 6) {
        // Make it at least 110 if we saw packets both in and out
        dev->rating |= rating;
        // Set bit 0 if we already saw more than one connection or
        // more than a few packets for the same connection.
        if(dev->conn.next || dev->conn.upd_total > 2) {
            dev->rating |= 1;
        }
    }

    return;
}

// This functon handles adding festats connection info to devices
// telemetry. It's unused for platforms that do not support festats.
// It's called from festats code for each tracked connection when dev
// telemetry requests the report before it builds the telemetry JSON.
// Returns: TRUE - connection info added, FALSE - unable to add
//          all or some of the info
int dt_add_fe_conn(FE_CONN_t *fe_conn)
{
    // If we see the connection then there were at least connection setup pkts
    int rating = 0x4 | 0x2 |
                 ((fe_conn->in.bytes > 0 || fe_conn->out.bytes > 0) ? 0x1 : 0);

    DPRINTF("%s: adding " MAC_PRINTF_FMT_TPL " rating %d\n",
            __func__, MAC_PRINTF_ARG_TPL(fe_conn->mac), rating);
    DT_DEVICE_t *dev = add_dev(fe_conn->mac, rating);
    if(!dev) {
        DPRINTF("%s: can't add or find " MAC_PRINTF_FMT_TPL "\n",
                __func__, MAC_PRINTF_ARG_TPL(fe_conn->mac));
        return FALSE;
    }

    // Update the device IP and the interface name where it was found
    dev->ipv4.i = fe_conn->hdr.dev_ipv4.i;
    memcpy(dev->ifname, fe_conn->hdr.ifname, sizeof(dev->ifname));

    // Prepare the dev telemetry connection header from fe connection header
    DT_CONN_HDR_t hdr;
    hdr.ipv4.i = fe_conn->hdr.peer_ipv4.i;
    hdr.proto = fe_conn->hdr.proto;
    hdr.rev = fe_conn->hdr.rev;
    hdr.port = (hdr.rev ? fe_conn->hdr.dev_port : fe_conn->hdr.peer_port);
    hdr.dev = dev;

    // Adding/updating the connection common info (i.e. the info updated
    // the same way from both festats and tpcap)
    DT_CONN_t *conn = ip_upd_conn(dev, &hdr);
    if(conn == NULL) {
        DPRINTF("%s: unable to add fe connection p:%d"
                IP_PRINTF_FMT_TPL ":%hu <-> " IP_PRINTF_FMT_TPL ":%hu \n",
                __func__, fe_conn->hdr.proto, 
                IP_PRINTF_ARG_TPL(fe_conn->hdr.dev_ipv4.b),
                fe_conn->hdr.dev_port,
                IP_PRINTF_ARG_TPL(fe_conn->hdr.peer_ipv4.b),
                fe_conn->hdr.peer_port);
        return FALSE;
    }

    // Current slice #
    int slice_num = cap_iteration % DEVTELEMETRY_NUM_SLICES;

    DPRINTF("%s: fe%s connection p:%d "
            IP_PRINTF_FMT_TPL ":%hu <-> " IP_PRINTF_FMT_TPL ":%hu "
            "cur in/from:%u out/to:%u\n",
            __func__, (fe_conn->hdr.rev ? " rev" : ""), fe_conn->hdr.proto,
            IP_PRINTF_ARG_TPL(fe_conn->hdr.dev_ipv4.b),
            fe_conn->hdr.dev_port,
            IP_PRINTF_ARG_TPL(fe_conn->hdr.peer_ipv4.b),
            fe_conn->hdr.peer_port,
            conn->bytes_from[slice_num], conn->bytes_to[slice_num]);
    DPRINTF("%s: fe reported in:%llu out:%llu, read in:%llu out:%llu\n",
            __func__, fe_conn->in.bytes, fe_conn->out.bytes,
                      fe_conn->in.bytes_read, fe_conn->out.bytes_read);

    // Calculate the updated counters. It only adds as much as we can
    // before the values overflow, the rest is not included in the
    // bytes_read, so we should be adding it later
    unsigned long long bytes_to = conn->bytes_to[slice_num];
    unsigned long long bytes_from = conn->bytes_from[slice_num];
    bytes_to += (fe_conn->in.bytes - fe_conn->in.bytes_read);
    bytes_from += (fe_conn->out.bytes - fe_conn->out.bytes_read);
    unsigned long long bytes_to_rem = 0;
    unsigned long long bytes_from_rem = 0;
    int all_accounted = TRUE;
    if(bytes_to > ULONG_MAX) {
        bytes_to_rem = bytes_to - ULONG_MAX;
        bytes_to = ULONG_MAX;
        all_accounted = FALSE;
    }
    if(bytes_from > ULONG_MAX) {
        bytes_from_rem = bytes_from - ULONG_MAX;
        bytes_from = ULONG_MAX;
        all_accounted = FALSE;
    }

    // Update the dev telemetry connection counters
    conn->bytes_to[slice_num] = bytes_to;
    conn->bytes_from[slice_num] = bytes_from;
    DPRINTF("%s: dt reporting in/to:%llu out/from:%llu\n",
            __func__, bytes_to, bytes_from);

    // Update device rating
    dev->rating |= rating;

    // Update bytes_read in the festats connection
    fe_conn->in.bytes_read = fe_conn->in.bytes - bytes_to_rem;
    fe_conn->out.bytes_read = fe_conn->out.bytes - bytes_from_rem;

    DPRINTF("%s: fe updated read in:%llu out:%llu\n",
            __func__, fe_conn->in.bytes_read, fe_conn->out.bytes_read);

    return all_accounted;
}

// Stats callback called by tpcap thread every capturing time slice
// interval.
// st -> tp_if_stats[] array from tpcap
// Do not block or wait for anything here.
// Avoid any slow operations if possible.
// No packet capturing callbacks are called while this function executes.
static void stats_ready_cb(TPCAP_IF_STATS_t *st)
{
    int ii, slice_num = cap_iteration % DEVTELEMETRY_NUM_SLICES;

    // Update the capturing interval counter global for the subsystem
    ++cap_iteration;

    // Add forwarding engine's connections to the devtelemetry tables
    fe_report_conn();

    // Capture the stats
    int st_if_num = 0;
    for(ii = 0; ii < TPCAP_STAT_IF_MAX; ii++) {
        if(st[ii].len_t == 0) {
            // Unused table entry
            continue;
        }
        DT_IF_STATS_t *dtst = &(stats_tbl[slice_num][st_if_num]);
        TPCAP_IF_STATS_t *ste = &(st[ii]);
        strncpy(dtst->name, st[ii].name, sizeof(dtst->name)-1);
        dtst->name[sizeof(dtst->name)-1] = '\0';
        dtst->sec = ste->rt.tv_sec;
        dtst->msec = ste->rt.tv_nsec / 1000000;
        dtst->len_t = ste->len_t;
        dtst->bytes_in = ste->bytes_in;
        dtst->bytes_out = ste->bytes_out;
        dtst->proc_pkts = ste->proc_pkts;
        dtst->tp_packets = ste->tp_packets;
        dtst->tp_drops = ste->tp_drops;
        dtst->slice = slice_num;
        dtst->wan = (ii == TPCAP_WAN_STATS_IDX);
        if(util_get_interface_kind != NULL) {
            dtst->kind = util_get_interface_kind(dtst->name);
        } else {
            dtst->kind = -1;
        }
        if(util_get_ipcfg(dtst->name, &dtst->ipcfg) != 0) {
            memset(&dtst->ipcfg, 0, sizeof(dtst->ipcfg));
        }
        if(util_get_mac(dtst->name, dtst->mac) != 0) {
            memset(&dtst->mac, 0, sizeof(dtst->mac));
        }
        ste->len_t = 0;
        ++st_if_num;
    }

    // Tell telemetry sender to grab data and start the transmission after
    // collecting DEVTELEMETRY_NUM_SLICES of the capturing time intervals.
    if(slice_num == (DEVTELEMETRY_NUM_SLICES - 1)) {
        // Tell sender that the device telemetry data is ready
        dt_sender_data_ready();

        // Unlock IP forwarding stats table
        fe_conn_tbl_stats(TRUE);

        // Reset the interface counters/stats table
        memset(&stats_tbl, 0, sizeof(stats_tbl));
        // Reset DNS tables
        dt_reset_dns_tables();
        // Reset main telemetry tables
        dt_reset_dev_tables();

        // Reset fingerprinting tables (since we send them with
        // the devices telemetry have to do it here)
        fp_reset_tables();
    }
}

// Returns pointer to the devices table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
DT_TABLE_STATS_t *dt_dev_tbl_stats(int reset)
{
    static DT_TABLE_STATS_t tbl_stats;
    memcpy(&tbl_stats, &dev_tbl_stats, sizeof(tbl_stats));
    if(reset) {
        memset(&dev_tbl_stats, 0, sizeof(dev_tbl_stats));
    }
    return &tbl_stats;
}

// Returns pointer to the connections table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
DT_TABLE_STATS_t *dt_conn_tbl_stats(int reset)
{
    static DT_TABLE_STATS_t tbl_stats;
    memcpy(&tbl_stats, &conn_tbl_stats, sizeof(tbl_stats));
    if(reset) {
        memset(&conn_tbl_stats, 0, sizeof(conn_tbl_stats));
    }
    return &tbl_stats;
}

// Get the interface counters and capturing stats
DT_IF_STATS_t *dt_get_if_stats(void)
{
    return &(stats_tbl[0][0]);
}

// Get the pointer to the devices table
// The table should only be accessed from the tpcap thread
DT_DEVICE_t **dt_get_dev_tbl(void)
{
    return dev_tbl;
}

// Device info collector init function
// Returns: 0 - if successful
int dt_main_collector_init(void)
{
    PKT_PROC_ENTRY_t *pe;

    // Allocate memory for the device and connection tables (never freed).
    // the device table preallocates the memory for data structs of
    // all DTEL_MAX_DEV devices, the conectioon table uses array of
    // pointers to the data structs that are allocated when needed.
    // Once the connection data structure is allocated and the pointer
    // is stored in the array it is never removed/freed.
    dev_tbl = calloc(DTEL_MAX_DEV, sizeof(DT_DEVICE_t *));
    conn_tbl = calloc(DTEL_MAX_CONN, sizeof(DT_CONN_t *));
    if(!dev_tbl || !conn_tbl) {
        log("%s: failed to allocate memory for data tables\n", __func__);
        return -1;
    }

    // Reset table stats (more for consistency since they are in BSS
    // segment)
    memset(&dev_tbl_stats, 0, sizeof(dev_tbl_stats));
    memset(&conn_tbl_stats, 0, sizeof(conn_tbl_stats));

    // Add the collector main packet processing entry.
    pe = &collector_all_ip;

#ifdef FEATURE_FESTATS_ONLY
    // For platforms having festats_only feature flag enabled override
    // collector_all_ip with collector_stats_cb_only
    pe = &collector_stats_cb_only;
#endif // FEATURE_FESTATS_ONLY

    if(tpcap_add_proc_entry(pe) != 0)
    {
        log("%s: tpcap_add_proc_entry() failed for: %s\n",
            __func__, pe->desc);
        return -2;
    }

    // Reset the interface counters/stats table (more for consistency since
    // it is BSS segment anyway)
    memset(&stats_tbl, 0, sizeof(stats_tbl));

    return 0;
}

// Subsystem init fuction. The subsystem runs a sender thread, but
// it populates data from the tpcap subsystem callbacks (run by tpcap thread).
// The callbacks collect the information in the data tables.
// The statistics callback that is called periodically (with
// TPCAP_TIME_SLICE interval) dumps all the data from the tables into JSON
// and kicks off the sender (after every DEVTELEMETRY_NUM_SLICES time slice
// intervals).
// While the sender transmits the information the callbacks populate data tables
// with the next set of data.
int devtelemetry_init(int level)
{
    if(level == INIT_LEVEL_DEVTELEMETRY)
    {
#if !defined(FEATURE_LAN_ONLY) && !defined(FEATURE_GUEST_NAT)
        // Unless it is a standalone AP device firmware we do not do device
        // telemetry in the AP operation mode (i.e. gateway firmware in the
        // AP mode).
        // But when guest nat is enabled, we need device telemetry in AP mode
        // too as the guest network is in a separate network.
        if(IS_OPM(UNUM_OPM_AP)) {
            return 0;
        }
#endif // !FEATURE_LAN_ONLY && !FEATURE_GUEST_NAT
        // Initialize the device info collector
        if(dt_main_collector_init() != 0) {
            return -1;
        }
        // Initialize the DNS info collector
        if(dt_dns_collector_init() != 0) {
            return -2;
        }
        // Start the festas subsystem if it's included in the build
        if(fe_start_subsystem != NULL && fe_start_subsystem() != 0) {
            return -3;
        }
        // Start the sender job
        if(dt_sender_start() != 0) {
            return -4;
        }
    }
    return 0;
}

#ifdef DEBUG
// Print out info for the counters test
void dt_if_counters_test(void)
{
    int ii;
    DT_IF_STATS_t *dts = dt_get_if_stats();

    // Dump the counters
    for(ii = 0; ii < (TPCAP_STAT_IF_MAX * DEVTELEMETRY_NUM_SLICES); ii++) {
        if(dts[ii].len_t <= 0) {
            continue;
        }
        time_t tt = dts[ii].sec;
        printf("%s: done at %.24s +%lu msec, duration %lu msec\n",
               dts[ii].name, ctime(&tt), dts[ii].msec, dts[ii].len_t);
        printf("  bytes_in  : %lu\n", dts[ii].bytes_in);
        printf("  bytes_out : %lu\n", dts[ii].bytes_out);
        printf("  proc_pkts : %lu\n", dts[ii].proc_pkts);
        printf("  tp_packets: %lu\n", dts[ii].tp_packets);
        printf("  tp_drops  : %lu\n", dts[ii].tp_drops);

        printf("\n");
    }
}

// A few protocol names
static char *proto_name(unsigned char proto)
{
    char *n = "unknown";
    switch(proto) {
        case 1:  n = "ICMP";     break;
        case 2:  n = "IGMP";     break;
        case 6:  n = "TCP";      break;
        case 17: n = "UDP";      break;
        case 47: n = "GRE";      break;
        case 51: n = "IPSecAH";  break;
        case 50: n = "IPSecESP"; break;
        case 8:  n = "EGP";      break;
        case 3:  n = "GGP";      break;
        case 20: n = "HMP";      break;
        case 88: n = "IGMP";     break;
        case 66: n = "RVD";      break;
        case 89: n = "OSPF";     break;
        case 12: n = "PUP";      break;
        case 27: n = "RDP";      break;
        case 46: n = "RSVP";     break;
    }
    return n;
}

// Print out connecion info for the test
static void print_test_conn_info(DT_CONN_t *conn)
{
    int ii;
#ifdef DT_CONN_MAP_IP_TO_DNS
    DT_DNS_IP_t *dns_ip = conn->dns_ip;
    DT_DNS_NAME_t *n_item = dns_ip ? dns_ip->dns : NULL;

    // Find the last DNS name in the CNAME chain
    while(n_item && n_item->cname) {
        n_item = n_item->cname;
    }
#else  // DT_CONN_MAP_IP_TO_DNS
    DT_DNS_NAME_t *n_item = NULL;
#endif // DT_CONN_MAP_IP_TO_DNS

    printf(" %3u/%.8s to " IP_PRINTF_FMT_TPL,
           conn->hdr.proto, proto_name(conn->hdr.proto),
           IP_PRINTF_ARG_TPL(conn->hdr.ipv4.b));
    if(conn->hdr.proto == 6 || conn->hdr.proto == 17) {
        printf(" p:%u(%s)", conn->hdr.port, conn->hdr.rev ? "our" : "their");
    }
    if(conn->hdr.proto == 6) {
        printf(" syn_ws:%d ws:%d",
               conn->syn_tcp_win_size, conn->cur_tcp_win_size);
    }
    if(n_item) {
        printf(" (%s)\n", n_item->name);
    } else {
        printf("\n");
    }
    printf("    upds: %u, bytes in/out: ", conn->upd_total);
    for(ii = 0; ii < DEVTELEMETRY_NUM_SLICES; ii++) {
        printf("%u/%u ", conn->bytes_to[ii], conn->bytes_from[ii]);
    }
    printf("\n");
}

// Print out the devices & connections info for the main
// info colector tests
void dt_main_tbls_test(void)
{
    unsigned long base_tt;
    int ii, count = 0, conn_count = 0;

    base_tt = util_time(10);
    // Set base time for printouts to be 30.1 sec back
    base_tt -= (DEVTELEMETRY_NUM_SLICES * unum_config.tpcap_time_slice * 10) + 1;

    printf("Devices & connections tables dump:\n");
    printf("----------------------------------\n");
    for(ii = 0; ii < DTEL_MAX_DEV; ii++) {
        DT_DEVICE_t *dev = dev_tbl[ii];
        if(dev != 0 && dev->rating != 0) {
            count++;
            printf(MAC_PRINTF_FMT_TPL " " IP_PRINTF_FMT_TPL
                   " on %s at %lu rated %u\n",
                   MAC_PRINTF_ARG_TPL(dev->mac), IP_PRINTF_ARG_TPL(dev->ipv4.b),
                   dev->ifname, dev->t_add - base_tt, dev->rating);
            DT_CONN_t *conn = &(dev->conn);
            while(conn) {
                ++conn_count;
                print_test_conn_info(conn);
                conn = conn->next;
            }
        }
    }
    printf("----------------------------------\n");
    printf("Devices: %d Connections: %d\n\n", count, conn_count);

    // Dump the stats
    DT_TABLE_STATS_t *d_tbl_st = dt_dev_tbl_stats(FALSE);
    DT_TABLE_STATS_t *c_tbl_st = dt_conn_tbl_stats(FALSE);

    printf("Devices table stats:\n");
    printf("  add_all = %lu\n", d_tbl_st->add_all);
    printf("  add_limit = %lu\n", d_tbl_st->add_limit);
    printf("  add_busy = %lu\n", d_tbl_st->add_busy);
    printf("  add_repl = %lu\n", d_tbl_st->add_repl);
    printf("  add_10 = %lu\n", d_tbl_st->add_10);
    printf("  add_found = %lu\n", d_tbl_st->add_found);
    printf("Connections table stats:\n");
    printf("  add_all = %lu\n", c_tbl_st->add_all);
    printf("  add_limit = %lu\n", c_tbl_st->add_limit);
    printf("  add_busy = %lu\n", c_tbl_st->add_busy);
    printf("  add_10 = %lu\n", c_tbl_st->add_10);
    printf("  add_found = %lu\n", c_tbl_st->add_found);

    DT_TABLE_STATS_t *fe_tbl_st = fe_conn_tbl_stats(FALSE);
    if(fe_tbl_st != NULL) {
        printf("Fast forwarding connection table stats:\n");
        printf("  add_all = %lu\n", fe_tbl_st->add_all);
        printf("  add_limit = %lu\n", fe_tbl_st->add_limit);
        printf("  add_busy = %lu\n", fe_tbl_st->add_busy);
        printf("  add_10 = %lu\n", fe_tbl_st->add_10);
        printf("  add_found = %lu\n", fe_tbl_st->add_found);
    }

    printf("\n");
}
#endif // DEBUG
