// (c) 2017-2018 minim.co
// fingerprinting DHCP information collector

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


#ifdef DEBUG
#define DPRINTF(args...) ((tpcap_test_param.int_val == TPCAP_TEST_FP_JSON) ? \
                          (printf(args)) : 0)
#else  // DEBUG
#define DPRINTF(args...) /* Nothing */
#endif // DEBUG


// DHCP packet header (followed by options)
struct dhcp_pkt {
    u_int8_t  type;           // packet type
    u_int8_t  haddrtype;      // type of hardware address (Ethernet, etc)
    u_int8_t  haddrlen;       // length of hardware address
    u_int8_t  hops;           // hops
    u_int32_t sid;            // random transaction id number
    u_int16_t sec;            // seconds used in timing
    u_int16_t flags;          // flags
    struct in_addr caddr;     // IP address of this machine (if available)
    struct in_addr oaddr;     // IP address of this machine (if offered)
    struct in_addr saddr;     // IP address of DHCP server
    struct in_addr raddr;     // IP address of DHCP relay
    unsigned char haddr[16];  // hardware address of this machine
    char sname[64];           // DHCP server name
    char file[128];           // boot file name
    char cookie[4];           // cookie 0x63,0x82,0x53,0x63 for DHCP
    unsigned char options[];  // variable length options (up to 308 bytes)
} __attribute__((packed));


// Forward declarations
static void dhcp_rcv_cb(TPCAP_IF_t *tpif,
                        PKT_PROC_ENTRY_t *pe,
                        struct tpacket2_hdr *thdr,
                        struct iphdr *iph);


// DHCP packets processing entry for fingerprinting info collector
static PKT_PROC_ENTRY_t fp_dhcp_pkt_proc = {
    0,
    {},
    0,
    {},
    PKT_MATCH_TCPUDP_P1_SRC|PKT_MATCH_TCPUDP_P2_DST|PKT_MATCH_TCPUDP_UDP_ONLY,
    { .p1 = 68, .p2 = 67 },
    NULL, dhcp_rcv_cb, NULL, NULL,
    "UDP from port 68 to 67, for DHCP fingerprinting"
};

// DHCP fingerprinting info table (uses pointers)
static FP_DHCP_t **dhcp_tbl;

// Structures tracking DHCP info table stats
static FP_TABLE_STATS_t dhcp_tbl_stats;


// Returns pointer to the DHCP info table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the returned data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
FP_TABLE_STATS_t *fp_dhcp_tbl_stats(int reset)
{
    static FP_TABLE_STATS_t tbl_stats;
    memcpy(&tbl_stats, &dhcp_tbl_stats, sizeof(tbl_stats));
    if(reset) {
        memset(&dhcp_tbl_stats, 0, sizeof(dhcp_tbl_stats));
    }
    return &tbl_stats;
}

// Get the pointer to the DHCP info table
// The table should only be accessed from the tpcap thread
FP_DHCP_t **fp_get_dhcp_tbl(void)
{
    return dhcp_tbl;
}

// Reset all DHCP tables (including the table usage stats)
// Has to work (not crash) even if the subsystem is not initialized
// (otherwise it will break devices telemetry tests)
void fp_reset_dhcp_tables(void)
{
    int ii;

    // If we are not initialized, nothing to do
    if(!dhcp_tbl) {
        return;
    }
    // Reset DHCP table (keeping already allocated items memory)
    for(ii = 0; ii < FP_MAX_DEV; ii++) {
        FP_DHCP_t *item = dhcp_tbl[ii];
        if(item) {
            // blob_len == 0 indicates unused entry
            item->blob_len = 0;
        }
    }
    // Reset stats
    fp_dhcp_tbl_stats(TRUE);

    return;
}

// Add MAC to DHCP info table
// Returns a pointer to the DHCP info table record that
// can be used for the MAC DHCP info or NULL
// (the entry is not assumed busy till blob data length is
//  assigned to it)
static FP_DHCP_t *add_dhcp(unsigned char *mac)
{
    int ii, idx;
    FP_DHCP_t *ret = NULL;

    // Total # of add requests
    ++(dhcp_tbl_stats.add_all);

    // Generate hash-based index
    idx = util_hash(mac, 6) % FP_MAX_DEV;

    for(ii = 0; ii < (FP_MAX_DEV / FP_SEARCH_LIMITER); ii++) {
        // If we hit an empty entry then the name we are looking for
        // is not in the table (we NEVER remove inidividual entries)
        if(!dhcp_tbl[idx]) {
            ret = malloc(sizeof(FP_DHCP_t));
            if(!ret) { // Run out of memory
                ++(dhcp_tbl_stats.add_limit);
                return NULL;
            }
            ret->blob_len = 0;
            dhcp_tbl[idx] = ret;
        }
        if(dhcp_tbl[idx]->blob_len == 0) {
            ret = dhcp_tbl[idx];
            memcpy(ret->mac, mac, 6);
            ++(dhcp_tbl_stats.add_new);
            break;
        }
        // Check if this is the same MAC
        if(memcmp(dhcp_tbl[idx]->mac, mac, 6) == 0) {
            ret = dhcp_tbl[idx];
            ++(dhcp_tbl_stats.add_found);
            break;
        }
        // Collision, try the next index
        idx = (idx + 1) % FP_MAX_DEV;
    }

    if(!ret) {
        ++(dhcp_tbl_stats.add_busy);
    } else if(ii < 10) {
        ++(dhcp_tbl_stats.add_10);
    }

    return ret;
}

// DHCP packets receive callback, called by tpcap thread for every
// UDP DHCP packet sent client -> server it captures.
// Do not block or wait for anything here.
// Do not log anything from here.
static void dhcp_rcv_cb(TPCAP_IF_t *tpif,
                        PKT_PROC_ENTRY_t *pe,
                        struct tpacket2_hdr *thdr,
                        struct iphdr *iph)
{
    struct ethhdr *ehdr = (struct ethhdr *)((void *)thdr + thdr->tp_mac);
    struct udphdr *udph = ((void *)iph) + sizeof(struct iphdr);
    struct dhcp_pkt *dhdr = ((void *)udph) + sizeof(struct udphdr);

    if(IS_OPM(UNUM_OPM_AP)) {
#ifdef FEATURE_GUEST_NAT
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

    // We are guaranteed to have the IP & UDP headers, check
    // that we have complete DNS header w/ at least 1 byte of options.
    int remains = thdr->tp_snaplen;
    remains -= (thdr->tp_net - thdr->tp_mac) +
               sizeof(struct iphdr) + sizeof(struct udphdr);
    if(remains < sizeof(struct dhcp_pkt) + 1) {
        DPRINTF("%s: incomplete DHCP header, %d bytes remains, need %d\n",
                   __func__, remains, sizeof(struct dhcp_pkt) + 1);
        return;
    }
    // We only interested in DHCP request packets
    if(dhdr->type != 1) {
        return;
    }
    // Verify DHCP cookie
    if(memcmp("\x63\x82\x53\x63", dhdr->cookie, 4) != 0) {
        DPRINTF("%s: invalid DHCP packet cookie %02x,%02x,%02x,%02x\n",
                __func__, (unsigned char)dhdr->cookie[0],
                (unsigned char)dhdr->cookie[1],
                (unsigned char)dhdr->cookie[2],
                (unsigned char)dhdr->cookie[3]);
        return;
    }
    // Make sure it is from a directly connected Ethernet client
    if(dhdr->haddrtype != 1 || dhdr->haddrlen != 6 ||
       memcmp(ehdr->h_source, dhdr->haddr, 6) != 0)
    {
        DPRINTF("%s: DHCP request client HW addr/type/value mismatch\n",
                __func__);
        return;
    }
    // Parse TLVs and make sure we have valid options data
    remains -= sizeof(struct dhcp_pkt);
    unsigned char *ptr = dhdr->options;
    unsigned int size = 0;
    while(size < remains && *ptr != 255) {
        if(*ptr == 0) {
            ++ptr;
            ++size;
            continue;
        }
        ++ptr;
        size += 2 + *ptr;
        ptr += 1 + *ptr;
    }
    if(size <= 0 || size > remains || *ptr != 255) {
        DPRINTF("%s: corrupt DHCP optins, at byte %d of %d, val: %d\n",
                __func__, size, remains,
                ((size <= remains) ? *ptr : 0));
        return;
    }
    if(size > FP_MAX_DHCP_OPTIONS) {
        DPRINTF("%s: too many DHCP options, %d of max %d, truncating\n",
                __func__, size, FP_MAX_DHCP_OPTIONS);
        size = FP_MAX_DHCP_OPTIONS;
    }
    // Add/find the device and update its DHCP request info
    FP_DHCP_t *fpd = add_dhcp(ehdr->h_source);
    if(!fpd) {
        // Can't add this entry to (or find it in) the table
        DPRINTF("%s: failed to add " MAC_PRINTF_FMT_TPL " to the table\n",
                  __func__, MAC_PRINTF_ARG_TPL(ehdr->h_source));
        return;
    }
    memcpy(fpd->blob, dhdr->options, size);
    fpd->blob_len = size;

    return;
}

// Init DHCP fingerprinting
// Returns: 0 - if successful
int fp_dhcp_init(void)
{
    PKT_PROC_ENTRY_t *pe;

    // Allocate memory for the DHCP info table (never freed).
    // The memory for each element is allocated when required
    // and stores the pointer in the array. Once allocated the
    // entries are never freed (reused when needed).
    dhcp_tbl = calloc(FP_MAX_DEV, sizeof(FP_DHCP_t *));
    if(!dhcp_tbl) {
        log("%s: failed to allocate memory for data tables\n", __func__);
        return -1;
    }

    // Add the collector packet processing entries.
    pe = &fp_dhcp_pkt_proc;
    if(tpcap_add_proc_entry(pe) != 0)
    {
        log("%s: tpcap_add_proc_entry() failed for: %s\n",
            __func__, pe->desc);
        return -1;
    }

    // Reset stats (more for consistency since they are in BSS
    // segment anyway)
    memset(&dhcp_tbl_stats, 0, sizeof(dhcp_tbl_stats));

    return 0;
}
