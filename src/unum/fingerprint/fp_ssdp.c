// (c) 2017-2019 minim.co
// fingerprinting SSDP information collector

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
static void ssdp_rsp_rcv_cb(TPCAP_IF_t *tpif,
                            PKT_PROC_ENTRY_t *pe,
                            struct tpacket2_hdr *thdr,
                            struct iphdr *iph);


// SSDP start timer handle. There should be only 1 SSDP start timer
// set at any time. If the timer is not set (or the handle is invalid)
// then SSDP discovery cycle is currently in progress.
static TIMER_HANDLE_t ssdp_timer = 0;

// Counts current discovery cycle attempts (up to FP_SSDP_DISCOVERY_ATTEMPTS
// for each cycle)
static int ssdp_discovery_attempt = 0;

// SSDP packets processing entry
static PKT_PROC_ENTRY_t fp_ssdp_pkt_proc = {
    0,
    {},
    PKT_MATCH_IP_MY_DST, // only receive responses (unicast) to our search
    {},
    PKT_MATCH_TCPUDP_UDP_ONLY|PKT_MATCH_TCPUDP_P1_DST,
    { .p1 = 1900 },
    NULL, ssdp_rsp_rcv_cb, NULL, NULL,
    "UDP to 1900, for SSDP fingerprinting"
};

// SSDP fingerprinting info table (uses pointers)
static FP_SSDP_t **ssdp_tbl;

// Structures tracking SSDP info table stats
static FP_TABLE_STATS_t ssdp_tbl_stats;


// Returns pointer to the SSDP info table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the returned data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
FP_TABLE_STATS_t *fp_ssdp_tbl_stats(int reset)
{
    static FP_TABLE_STATS_t tbl_stats;
    memcpy(&tbl_stats, &ssdp_tbl_stats, sizeof(tbl_stats));
    if(reset) {
        memset(&ssdp_tbl_stats, 0, sizeof(ssdp_tbl_stats));
    }
    return &tbl_stats;
}

// Get the pointer to the SSDP info table
// The table should only be accessed from the tpcap thread
FP_SSDP_t **fp_get_ssdp_tbl(void)
{
    return ssdp_tbl;
}

// Reset all SSDP tables (including the table usage stats)
// Has to work (not crash) even if the subsystem is not initialized
// (otherwise it will break devices telemetry tests)
void fp_reset_ssdp_tables(void)
{
    int ii;

    // If we are not initialized, nothing to do
    if(!ssdp_tbl) {
        return;
    }
    // Reset SSDP table (keeping already allocated items memory)
    for(ii = 0; ii < FP_MAX_DEV; ii++) {
        FP_SSDP_t *item = ssdp_tbl[ii];
        if(item) {
            item->data[0] = 0; // indicates unused entry
        }
    }
    // Reset stats
    fp_ssdp_tbl_stats(TRUE);

    return;
}

// Add MAC to SSDP info table
// Returns a pointer to the SSDP info table record that
// can be used for the MAC SSDP info or NULL
// (the entry is not assumed busy till data string is empty)
static FP_SSDP_t *add_ssdp(unsigned char *mac)
{
    int ii, idx;
    FP_SSDP_t *ret = NULL;

    // Total # of add requests
    ++(ssdp_tbl_stats.add_all);

    // Generate hash-based index
    idx = util_hash(mac, 6) % FP_MAX_DEV;

    for(ii = 0; ii < (FP_MAX_DEV / FP_SEARCH_LIMITER); ii++) {
        // If we hit an empty entry then the name we are looking for
        // is not in the table (we NEVER remove inidividual entries)
        if(!ssdp_tbl[idx]) {
            ret = malloc(sizeof(FP_SSDP_t));
            if(!ret) { // Run out of memory
                ++(ssdp_tbl_stats.add_limit);
                return NULL;
            }
            ret->data[0] = 0;
            ssdp_tbl[idx] = ret;
        }
        if(ssdp_tbl[idx]->data[0] == 0) {
            ret = ssdp_tbl[idx];
            memcpy(ret->mac, mac, 6);
            ++(ssdp_tbl_stats.add_new);
            break;
        }
        // Check if this is the same MAC
        if(memcmp(ssdp_tbl[idx]->mac, mac, 6) == 0) {
            ret = ssdp_tbl[idx];
            ++(ssdp_tbl_stats.add_found);
            break;
        }
        // Collision, try the next index
        idx = (idx + 1) % FP_MAX_DEV;
    }

    if(!ret) {
        ++(ssdp_tbl_stats.add_busy);
    } else if(ii < 10) {
        ++(ssdp_tbl_stats.add_10);
    }

    return ret;
}

// SSDP packets receive callback, called by tpcap thread for every
// UDP unicast to our IP port 1900. Do not block or wait for anything here.
static void ssdp_rsp_rcv_cb(TPCAP_IF_t *tpif,
                            PKT_PROC_ENTRY_t *pe,
                            struct tpacket2_hdr *thdr,
                            struct iphdr *iph)
{
    static char ssdp_hdr[] = "HTTP/1.1 200 OK\r\n";
    struct ethhdr *ehdr = (struct ethhdr *)((void *)thdr + thdr->tp_mac);
    struct udphdr *udph = ((void *)iph) + sizeof(struct iphdr);
    char *ptr = ((void *)udph) + sizeof(struct udphdr);

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

    // We are guaranteed to have the IP & UDP headers, the SSDP data
    // is the only packet payload. Make sure we have enough space for
    // at least "HTTP/1.1 200 OK\r\n<PAYLOAD>\r\n\r\n" (22 characters)
    int remains = thdr->tp_snaplen;
    remains -= (thdr->tp_net - thdr->tp_mac) +
               sizeof(struct iphdr) + sizeof(struct udphdr);
    if(remains < 22) {
        DPRINTF("%s: the %d bytes payload is too short for SSDP data\n",
                   __func__, remains);
        return;
    }

    // We are only interested in 200 responses
    if(strncmp(ssdp_hdr, ptr, sizeof(ssdp_hdr) - 1) != 0) {
        DPRINTF("%s: SSDP response header mismatch, got <%.*s>\n",
                __func__, sizeof(ssdp_hdr) - 1, ptr);
        return;
    }

    // Bypass the header
    ptr += sizeof(ssdp_hdr) - 1;
    remains -= sizeof(ssdp_hdr) - 1;

    // Capture the remaining payload
    int tocopy, size, slen, end_found = FALSE;
    for(tocopy = slen = size = 0;
        size + 2 <= remains && size <= FP_MAX_SSDP_DATA - 1;
        ++size)
    {
        if(ptr[size] == '\r' && ptr[size + 1] == '\n') {
            tocopy = size;
            if(slen == 0) {
                end_found = TRUE;
                break;
            }
            slen = 0;
            ++size;
            continue;
        }
        // If 0-terminator or non utf-8 charcter, terminate here
        if(ptr[size] == 0 || (ptr[size] & 0x80) != 0) {
            break;
        }
        ++slen;
    }

    // If no request end detected print a debug warning, but proceed
    // since it just might be beyond our packet capturing size limit.
    if(!end_found) {
        DPRINTF("%s: warning, at byte %d, no terminator reached\n",
                __func__, size);
    }

    // If no complete lines of data we can capture, then just drop it
    if(tocopy <= 0) {
        DPRINTF("%s: total parsed size %d, can capture %d, dropping\n",
                __func__, size, tocopy);
        return;
    }

    // Add/find the device and update its SSDP info
    FP_SSDP_t *fps = add_ssdp(ehdr->h_source);
    if(!fps) {
        // Can't add this entry to (or find it in) the table
        DPRINTF("%s: failed to add " MAC_PRINTF_FMT_TPL " to the table\n",
                  __func__, MAC_PRINTF_ARG_TPL(ehdr->h_source));
        return;
    }

    memcpy(fps->data, ptr, tocopy);
    fps->data[tocopy] = 0;
    fps->truncated = !end_found;

    return;
}

// Start SSDP discovery. The function is a fast timer handler,
// no long processing is allowed here. It verifies that the
// server is ready to receive telemetry form the agent, then
// starts SSDP discover. It only runs in the timer thread.
static void ssdp_discover(TIMER_PARAM_t *p)
{
    int end_cycle = FALSE;

    // The timer handle is no longer active, so reset it to 0
    ssdp_timer = 0;

    // Note: we are sure that command handler is not scheduling SSDP
    //       processing since it MUST successfully cancel the old SSDP
    //       timer before setting the new one (since we are running
    //       nobody cancelled it)

    unsigned long delay = FP_SSDP_DISCOVERY_INIT_DELAY;
    for(;;) {
        // If activation is not complete nothing to do
        if(!is_agent_activated()) {
            break;
        }

        // If starting the cycle set up packet capturing filter
        if(ssdp_discovery_attempt <= 0)
        {
            // Add the SSDP info collector packet processing entries.
            // If fails will try again in a few seconds (ssdp_discovery_attempt
            // is not updated, so it will continue keep trying)
            if(tpcap_add_proc_entry(&fp_ssdp_pkt_proc) != 0)
            {
                log("%s: tpcap_add_proc_entry() failed for: %s\n",
                    __func__, fp_ssdp_pkt_proc.desc);
                delay = FP_SSDP_DISCOVERY_PERIOD;
                break;
            }
        } // Just finished the cycle
        else if(ssdp_discovery_attempt >= FP_SSDP_DISCOVERY_ATTEMPTS)
        {
            // Remove packet capturing filter
            tpcap_del_proc_entry(&fp_ssdp_pkt_proc);
            // Set the delay for the long wait
            delay = FP_SSDP_DISCOVERY_CYCLE_INTERVAL;
            // There is a chance that before data from this cycle is uploaded
            // server starts a new one, there should be no harm in doing that.
            end_cycle = TRUE;
            break;
        }

        // The cycle has just started or in progress, send the SSDP discovery.
        // Ignore a failure to send, we try several times anyway.
        UDP_PAYLOAD_t payload;
        payload.dip = "239.255.255.250";
        payload.dport = 1900;
        payload.sport = 1900;
        payload.data =  "M-SEARCH * HTTP/1.1\r\n"
                        "HOST:239.255.255.250:1900\r\n"
                        "MAN:\"ssdp:discover\"\r\n"
                        "ST:upnp:rootdevice\r\n"
                        "MX:" UTIL_STR(FP_SSDP_DISCOVERY_PERIOD) "\r\n"
                        "\r\n";
        payload.len = strlen(payload.data);

        util_enum_ifs(UTIL_IF_ENUM_RTR_LAN,
                      (UTIL_IF_ENUM_CB_t)send_udp_packet, &payload);

        // Increment the attempt count and set the delay
        ++ssdp_discovery_attempt;
        delay = FP_SSDP_DISCOVERY_PERIOD;

        break;
    }

    // Set the timer to call us again (either soon when it is time for the next
    // attempt to send the discover or in a while if scheduling a new cycle)
    TIMER_HANDLE_t timer = util_timer_set(delay * 1000,
                                          "ssdp_discover", ssdp_discover,
                                          NULL, FALSE);
    if(timer == 0) {
        log("%s: unable to set timer, SSDP fingerprinting terminated\n",
            __func__);
        // Nothing we can do here, just giving up on SSDP till server
        // sends the command and we try to set up the timer again.
        tpcap_del_proc_entry(&fp_ssdp_pkt_proc);
        // Set discovery attempts to a negative value to have some indication
        // that there is no longer SSDP timer scheduled or being processed.
        ssdp_discovery_attempt = -1;
        return;
    }

    // Set the timer handle global and reset the attempt count
    // if we are done with the current cycle and successfully
    // scheduled the next one.
    if(end_cycle)
    {
        ssdp_discovery_attempt = 0;
        __sync_synchronize();
        ssdp_timer = timer;
        return;
    }

    return;
}

// The "do_ssdp_discovery" command processor. It executes
// concurrently with the SSDP timer driven routines and
// has to make sure there is only one SSDP timer/discovery
// set or being processed at any time.
void cmd_ssdp_discovery(void)
{
    log("%s: processing SSDP discovery command\n", __func__);

    // First get the current timer handle and try to cancel it
    TIMER_HANDLE_t timer = ssdp_timer;

    // If timer is not set while the ssdp_discovery_attempt has a valid
    // non-negative value then we are already doing discovery and ignore
    // the command.
    if(!timer && ssdp_discovery_attempt >= 0) {
        log("%s: SSDP discovery is already in progress\n", __func__);
        return;
    }

    // Try to cancel the timer, if not successful then the timer has
    // just fired triggering the new discovery and is not yet zeroed
    // (unlikely)
    if(timer != 0 && util_timer_cancel(timer) != 0) {
        log("%s: unable to cancel timer 0x%x, discovery must be in progress\n",
            __func__, timer);
        return;
    }

    // The old timer is cancelled successfully (or not set) and we schedule
    // new SSDP discovery to start immediately
    ssdp_timer = 0;
    ssdp_discovery_attempt = 0;
    timer = util_timer_set(0, "ssdp_discover", ssdp_discover,
                           NULL, FALSE);
    if(timer == 0) {
        log("%s: failed to set SSDP discovery timer\n", __func__);
        // Set discovery attempts to a negative value to have some indication
        // that there is no longer SSDP timer scheduled or being processed.
        ssdp_discovery_attempt = -1;
        return;
    }

    // The discovery has been scheduled and it will take care of setting
    // ssdp_timer global.
    log("%s: SSDP discovery successfully scheduled\n", __func__);
    return;
}

// Init SSDP fingerprinting
// Returns: 0 - if successful
int fp_ssdp_init(void)
{
#if !defined(FEATURE_LAN_ONLY) && !defined(FEATURE_GUEST_NAT)
    // We do not do SSDP discovery in the AP operation mode (i.e. gateway
    // firmware in the AP mode), but do it in the standalone AP firmware
    if(IS_OPM(UNUM_OPM_AP)) {
        return 0;
    }
#endif // !FEATURE_LAN_ONLY && !FEATURE_GUEST_NAT

    // Allocate memory for the SSDP info table (never freed).
    // The memory for each element is allocated when required
    // and stores the pointer in the array. Once allocated the
    // entries are never freed (reused when needed).
    ssdp_tbl = calloc(FP_MAX_DEV, sizeof(FP_SSDP_t *));
    if(!ssdp_tbl) {
        log("%s: failed to allocate memory for data tables\n", __func__);
        return -1;
    }

    // Reset stats (more for consistency since they are in BSS
    // segment anyway)
    memset(&ssdp_tbl_stats, 0, sizeof(ssdp_tbl_stats));

    // Schedule SSDP discovery start
    ssdp_timer = util_timer_set(FP_SSDP_DISCOVERY_INIT_DELAY * 1000,
                                "ssdp_discover", ssdp_discover,
                                NULL, FALSE);
    if(ssdp_timer == 0) {
        return -1;
    }

    return 0;
}
