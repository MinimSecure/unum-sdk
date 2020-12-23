// (c) 2019 minim.co
// fingerprinting HTTP UserAgent information collector

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

// Forward declarations
static void useragent_rcv_cb(TPCAP_IF_t *tpif,
                             PKT_PROC_ENTRY_t *pe,
                             struct tpacket2_hdr *thdr,
                             struct iphdr *iph);

// HTTP packets processing entry for fingerprinting info collector
static PKT_PROC_ENTRY_t fp_useragent_pkt_proc = {
    PKT_MATCH_ETH_UCAST_ANY,
    {},
    PKT_MATCH_IP_NET_SRC,
    {},
    PKT_MATCH_TCPUDP_P1_DST|PKT_MATCH_TCPUDP_TCP_ONLY,
    { .p1 = 80,},
    NULL, useragent_rcv_cb, NULL, NULL,
    "HTTP Traffic for UserAgent fingerprinting"
};

// UserAgent fingerprinting info table (uses pointers)
static FP_USERAGENT_t *useragent_tbl;

// Structures tracking UserAgent info table stats
static FP_TABLE_STATS_t useragent_tbl_stats;

// Returns pointer to the UserAgent info table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the returned data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
FP_TABLE_STATS_t *fp_useragent_tbl_stats(int reset)
{
    static FP_TABLE_STATS_t tbl_stats;
    memcpy(&tbl_stats, &useragent_tbl_stats, sizeof(tbl_stats));
    if(reset) {
        memset(&useragent_tbl_stats, 0, sizeof(useragent_tbl_stats));
    }
    return &tbl_stats;
}

// Get the pointer to the UserAgent info table
// The table should only be accessed from the tpcap thread
FP_USERAGENT_t *fp_get_useragent_tbl(void)
{
    return useragent_tbl;
}

// Reset all UserAgent tables (including the table usage stats)
// Has to work (not crash) even if the subsystem is not initialized
// (otherwise it will break devices telemetry tests)
void fp_reset_useragent_tables(void)
{
    int ii;

    // If we are not initialized, nothing to do
    if(!useragent_tbl) {
        return;
    }

    // Reset useragent table (keeping already allocated items memory)
    for(ii = 0; ii < FP_MAX_DEV; ii++) {
        useragent_tbl[ii].busy = FALSE;
    }
    // Reset stats
    fp_useragent_tbl_stats(TRUE);

    return;
}

// Add MAC to UserAgent info table
// Returns a pointer to the UserAgent info table record that
// can be used for the MAC UserAgent info or NULL
// (the entry is not assumed busy till blob data length is
//  assigned to it)
FP_USERAGENT_t *add_useragent(unsigned char *mac)
{
    int ii, idx;
    FP_USERAGENT_t *ret = NULL;

    // Total # of add requests
    ++(useragent_tbl_stats.add_all);

    // Generate hash-based index
    idx = util_hash(mac, 6) % FP_MAX_DEV;

    for(ii = 0; ii < (FP_MAX_DEV / FP_SEARCH_LIMITER); ii++) {
        // If we hit a non-busy entry then the name we are looking for
        // is not in the table (we NEVER remove inidividual entries)
        if(useragent_tbl[idx].busy == FALSE) {
            ret = &useragent_tbl[idx];
            ret->busy = TRUE;
            ret->index = 0;
            memcpy(ret->mac, mac, 6);
            ++(useragent_tbl_stats.add_new);
            break;
        }
        // Check if this is the same MAC
        else if(memcmp(useragent_tbl[idx].mac, mac, 6) == 0) {
            ret = &useragent_tbl[idx];
            ++(useragent_tbl_stats.add_found);
            break;
        }
        // Collision, try the next index
        idx = (idx + 1) % FP_MAX_DEV;
    }

    if(!ret) {
        ++(useragent_tbl_stats.add_busy);
    } else if(ii < 10) {
        ++(useragent_tbl_stats.add_10);
    }

    return ret;
}

#define HTTP_FIELD_SEPARATOR "\r\n"
#define HTTP_END             "\r\n\r\n"
#define HTTP_STRING "HTTP"
#define USER_AGENT_STRING "User-Agent: "
#define NON_ASCII_REPLACEMENT_CHAR ' '
#define MIN_HTTP_SIZE 16

// UserAgent packets receive callback, called by tpcap thread for every
// HTTP packet sent client -> server it captures.
// Do not block or wait for anything here.
// Do not log anything from here.
static void useragent_rcv_cb(TPCAP_IF_t *tpif,
                             PKT_PROC_ENTRY_t *pe,
                             struct tpacket2_hdr *thdr,
                             struct iphdr *iph)
{
    struct ethhdr *ehdr = (struct ethhdr *)((void *)thdr + thdr->tp_mac);
    struct tcphdr *tcphdr = ((void *)iph + sizeof(struct iphdr));
    int header_total = thdr->tp_net - thdr->tp_mac + sizeof(struct iphdr) +
                       tcphdr->doff * 4;
    char *tcp = ((void *)iph) + sizeof(struct iphdr) + (tcphdr->doff * 4);

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

    // Calculate Payload Size (includes offset in tcp header, which could be wrong)
    int payload_size = thdr->tp_snaplen - header_total;

    // Calculate End of Packet (this is guaranteed to be accurate)
    char *end_of_packet = (char*)ehdr + thdr->tp_snaplen;

    // If there is a mismatch, use the smaller of the two
    if(payload_size > (end_of_packet - tcp)) {
        payload_size = (end_of_packet - tcp);
    }

    // If payload is less than a conservative min for an HTTP packet, return
    if(payload_size < MIN_HTTP_SIZE) {
        return;
    }

    // If payload doesn't start w/ GET or POST, return
    if(!(tcp[0] == 'G' && tcp[1] == 'E' && tcp[2] == 'T') &&
       !(tcp[0] == 'P' && tcp[1] == 'O' && tcp[2] == 'S' && tcp[3] == 'T'))
    {
        return;
    }

    // Set Current Pointer to Start of Payload
    char *curr = tcp;

    // Find end of HTTP
    char *end_of_http = memmem(curr, payload_size, HTTP_END, sizeof(HTTP_END) - 1);
    if(end_of_http) {
        payload_size = end_of_http - tcp + sizeof(HTTP_END) - 1;
    }

    // Find End of First Record -- If not found, return
    char *end_of_record = memmem(curr, payload_size, HTTP_FIELD_SEPARATOR,
                                 sizeof(HTTP_FIELD_SEPARATOR) - 1);
    if(!end_of_record) {
        DPRINTF("%s: Cannot find end of first field\n", __func__);
        return;
    }

    // Look for HTTP in First Record -- If not found, return
    char *http = memmem(curr, (int)(end_of_record - curr), HTTP_STRING,
                        sizeof(HTTP_STRING) - 1);
    if(!http) {
        DPRINTF("%s: String 'HTTP' not found in first header field\n", __func__);
        return;
    }

    // advance pointer to next record
    curr = end_of_record + sizeof(HTTP_FIELD_SEPARATOR) - 1;
    char *agent_string = NULL;

    // Look Through Packet Until We Are At The End
    while(curr < end_of_packet) {
        end_of_record = memmem(curr, (int)(end_of_packet - curr),
                               HTTP_FIELD_SEPARATOR,
                               sizeof(HTTP_FIELD_SEPARATOR) - 1);
        if(!end_of_record) {
            DPRINTF("%s: record continues beyond end of capture\n", __func__);
            return;
        }

        agent_string = memmem(curr, (int)(end_of_record - curr),
                              USER_AGENT_STRING,
                              sizeof(USER_AGENT_STRING) - 1);
        if(agent_string) {
            agent_string += sizeof(USER_AGENT_STRING) - 1;
            break;
        }
        curr = end_of_record + sizeof(HTTP_FIELD_SEPARATOR) - 1;
    }

    if(!agent_string) {
        DPRINTF("%s: UserAgent field not found in HTTP header\n", __func__);
        return;
    }

    // Compute string length taking into account max length
    int agent_string_length = end_of_record - agent_string;
    if(agent_string_length >= FP_MAX_USERAGENT_LEN) {
        agent_string_length = FP_MAX_USERAGENT_LEN - 1;
    }

    // Look any non ASCII characters and return if found
    int jj;
    for(jj=0; jj<agent_string_length; jj++) {
        if((unsigned char)agent_string[jj] > 127) {
            DPRINTF("%s: UserAgent field contains non-ASCII characters\n",
                    __func__);
            return;
        }
    }

    // Add/find the device by MAC address
    FP_USERAGENT_t *fpu = add_useragent(ehdr->h_source);
    if(!fpu) {
        // Can't add this entry to (or find it in) the table
        DPRINTF("%s: failed to add " MAC_PRINTF_FMT_TPL " to the table\n",
                  __func__, MAC_PRINTF_ARG_TPL(ehdr->h_source));
        return;
    }

    // Compute hash of UserAgent String
    unsigned int hash = util_hash(agent_string, agent_string_length);

    // Find a duplicate entry (by hash) if it exists
    for(jj = 0; jj < fpu->index && jj < FP_MAX_USERAGENT_COUNT; jj++) {
        if(fpu->ua[jj] != NULL && fpu->ua[jj]->hash == hash) {
            return;
        }
    }

    // If we have already used up all entries, we're done
    unsigned char index = fpu->index;
    if(index >= FP_MAX_USERAGENT_COUNT) {
        return;
    }

    // Allocate memory for the string if it hasn't been allocated prior
    if(fpu->ua[index] == NULL) {
        FP_USERAGENT_DATA_t *fpud = UTIL_MALLOC(sizeof(FP_USERAGENT_DATA_t));
        if(!fpud) {
            DPRINTF("%s: failed to allocate memory for UserAgent string\n ",
                    __func__);
            return;
        }
        fpu->ua[index] = fpud;
    }

    // Copy String From Packet to UserAgent Data and NULL termiante
    memcpy(fpu->ua[index]->data, agent_string, agent_string_length);
    fpu->ua[index]->data[agent_string_length] = '\0';

    // Update Hash
    fpu->ua[index]->hash = hash;

    // Update index
    fpu->index++;

    return;
}

// Init UserAgent fingerprinting
// Returns: 0 - if successful
int fp_useragent_init(void)
{
    PKT_PROC_ENTRY_t *pe;

#if !defined(FEATURE_LAN_ONLY) && !defined(FEATURE_GUEST_NAT)
    // No AP mode
    if(IS_OPM(UNUM_OPM_AP)) {
        return 0;
    }
#endif // !FEATURE_LAN_ONLY && !FEATURE_GUEST_NAT
    // Allocate memory for the UserAgent info table (never freed).
    useragent_tbl = UTIL_CALLOC(FP_MAX_DEV, sizeof(*useragent_tbl));
    if(!useragent_tbl) {
        log("%s: failed to allocate memory for data tables\n", __func__);
        return -1;
    }

    // Get Address Information
    DEV_IP_CFG_t dic;
    if(util_get_ipcfg(GET_MAIN_LAN_NET_DEV(), &dic)) {
        log("%s: Unable to retreive IP configuration\n", __func__);
        return -1;
    }

    // Configure Filter w/ Address Information
    memcpy(&fp_useragent_pkt_proc.ip.a1.b, &dic.ipv4.b,
           sizeof(fp_useragent_pkt_proc.ip.a1.b));
    memcpy(&fp_useragent_pkt_proc.ip.a2.b, &dic.ipv4mask.b,
           sizeof(fp_useragent_pkt_proc.ip.a2.b));

    // Add the collector packet processing entries.
    pe = &fp_useragent_pkt_proc;
    if(tpcap_add_proc_entry(pe) != 0)
    {
        log("%s: tpcap_add_proc_entry() failed for: %s\n",
            __func__, pe->desc);
        return -1;
    }

    // Reset stats (more for consistency since they are in BSS
    // segment anyway)
    memset(&useragent_tbl_stats, 0, sizeof(useragent_tbl_stats));

    return 0;
}
