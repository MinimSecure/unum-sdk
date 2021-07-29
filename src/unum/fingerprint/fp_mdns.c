// (c) 2017-2018 minim.co
// fingerprinting mDNS information collector

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


// mDNS response header
struct mdns_rsp {
   /*************************************************
    *              THE HEADER                       *
    *************************************************
     0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                      ID                       |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    QDCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    ANCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    NSCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    ARCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    *************************************************/

    uint16_t id;      // Bits 0-15 are the query identifier
    uint8_t flags1;   // 0      : Bit one Query or response
                      // 1 - 4  : OPcode, set 0 for standard
                      // 5      : AA, set to 1 for response 0 for query
                      // 6      : TC, truncated
                      // 7      : RD, Recursion desired
    uint8_t flags2;   // 0      : RA, recursion available
                      // 2-3    : set to zero
                      // 4-7    : Rcode, set 0's for client
    uint16_t qdcount; // how many queries
    uint16_t ancount; // answers count
    uint16_t nscount; // authorized nameservers count
    uint16_t arcount; // additional records count
} __attribute__((packed));

struct mdns_answer {
    uint16_t type;  // answer type
    uint16_t class; // answers class
    uint32_t ttl;   // time to live
    uint16_t dlen;  // length of the data (following the answer header)
} __attribute__((packed));

// Forward declarations
static void mdns_rcv_cb(TPCAP_IF_t *tpif,
                        PKT_PROC_ENTRY_t *pe,
                        struct tpacket2_hdr *thdr,
                        struct iphdr *iph);


// mDNS packets processing entry for device info collector
static PKT_PROC_ENTRY_t fp_mdns_pkt_proc = {
    0,
    {},
    0,
    {},
    PKT_MATCH_TCPUDP_P1_ANY | PKT_MATCH_TCPUDP_UDP_ONLY,
    { .p1 = 5353 },
    NULL, mdns_rcv_cb, NULL, NULL,
    "UDP from port 5353, for mDNS fingerprinting"
};

// mDNS fingerprinting info table (uses pointers)
static FP_MDNS_t **mdns_tbl;

// Structures tracking mDNS info table stats
static FP_TABLE_STATS_t mdns_tbl_stats;


// Returns pointer to the mDNS info table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the returned data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
FP_TABLE_STATS_t *fp_mdns_tbl_stats(int reset)
{
    static FP_TABLE_STATS_t tbl_stats;
    memcpy(&tbl_stats, &mdns_tbl_stats, sizeof(tbl_stats));
    if(reset) {
        memset(&mdns_tbl_stats, 0, sizeof(mdns_tbl_stats));
    }
    return &tbl_stats;
}

// Get the pointer to the mDNS info table
// The table should only be accessed from the tpcap thread
FP_MDNS_t **fp_get_mdns_tbl(void)
{
    return mdns_tbl;
}

// Reset all mDNS tables (including the table usage stats)
// Has to work (not crash) even if the subsystem is not initialized
// (otherwise it will break devices telemetry tests)
void fp_reset_mdns_tables(void)
{
    int ii;

    // If we are not initialized, nothing to do
    if(!mdns_tbl) {
        return;
    }
    // Reset mDNS table (keeping already allocated items memory)
    for(ii = 0; ii < FP_MAX_DEV; ii++) {
        FP_MDNS_t *item = mdns_tbl[ii];
        if(item) {
            // blob_len == 0 indicates unused entry
            item->blob_len = 0;
        }
    }
    // Reset stats
    fp_mdns_tbl_stats(TRUE);

    return;
}

// Add MAC to mDNS info table
// Returns a pointer to the mDNS info table record that
// can be used for the MAC mDNS info or NULL
// (the entry is not assumed busy till blob data length is
//  assigned to it)
static FP_MDNS_t *add_mdns(unsigned char *mac, char *name, int name_len)
{
    int ii, idx;
    FP_MDNS_t *ret = NULL;

    // Total # of add requests
    ++(mdns_tbl_stats.add_all);

    // Generate hash-based index
    idx = (util_hash(mac, 6) + util_hash(name, name_len)) % FP_MAX_DEV;

    for(ii = 0; ii < (FP_MAX_DEV / FP_SEARCH_LIMITER); ii++) {
        // If we hit an empty entry then the name we are looking for
        // is not in the table (we NEVER remove individual entries)
        if(!mdns_tbl[idx]) {
            ret = malloc(sizeof(FP_MDNS_t));
            if(!ret) { // Run out of memory
                ++(mdns_tbl_stats.add_limit);
                return NULL;
            }
            ret->blob_len = 0;
            ret->port = 0;
            mdns_tbl[idx] = ret;
        }
        if(mdns_tbl[idx]->blob_len == 0) {
            ret = mdns_tbl[idx];
            memcpy(ret->mac, mac, 6);
            memcpy(ret->name, name, name_len);
            ++(mdns_tbl_stats.add_new);
            break;
        }
        // Check if this is the same MAC and name
        if((memcmp(mdns_tbl[idx]->mac, mac, 6) == 0) &&
           (strncmp((char *)mdns_tbl[idx]->name, name,
                    FP_MAX_MDNS_NAME) == 0)) {
            ret = mdns_tbl[idx];
            ++(mdns_tbl_stats.add_found);
            break;
        }
        // Collision, try the next index
        idx = (idx + 1) % FP_MAX_DEV;
    }

    if(!ret) {
        ++(mdns_tbl_stats.add_busy);
    } else if(ii < 10) {
        ++(mdns_tbl_stats.add_10);
    }

    return ret;
}

// mDNS packets receive callback, called by tpcap thread for every
// UDP mDNS packet broadcast from client it captures.
// Do not block or wait for anything here.
// Do not log anything from here.
static void mdns_rcv_cb(TPCAP_IF_t *tpif,
                        PKT_PROC_ENTRY_t *pe,
                        struct tpacket2_hdr *thdr,
                        struct iphdr *iph)
{
  struct ethhdr *ehdr = (struct ethhdr *)((void *)thdr + thdr->tp_mac);
  struct udphdr *udph = ((void *)iph) + sizeof(struct iphdr);
  struct mdns_rsp *dnsh = ((void *)udph) + sizeof(struct udphdr);

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

  // We are guaranteed to have the IP header, but have to keep checking
  // the mDNS data being examined is actually captured.
  int remains = thdr->tp_snaplen;
  remains -= (thdr->tp_net - thdr->tp_mac) +
             sizeof(struct iphdr) + sizeof(struct udphdr);
  // If we do not have full mDNS header ignore the packet
  if(remains < sizeof(struct mdns_rsp)) {
      DPRINTF("%s: incomplete mDNS header, only %d bytes remains\n",
              __func__, remains);
      return;
  }
  remains -= sizeof(struct mdns_rsp);
  // Check if it is a standard mDNS response
  if((dnsh->flags1 & 0x80) != 0x80) {
      return;
  }
  int qcount = ntohs(dnsh->qdcount);
  unsigned char *ptr = ((void *)dnsh) + sizeof(struct mdns_rsp);
  char name[FP_MAX_MDNS_NAME];
  // Skip the query section
  while(qcount > 0 && remains > 0) {
      // Pull out the query name
      int val = extract_dns_name(dnsh, ptr, remains, NULL, 0, 0, FALSE);
      if(val < 0) {
          // Corrupt or incomplete query data
          DPRINTF("%s: (%d) failed to get qname, offset %d, len %d\n",
                  __func__, val, (void *)ptr - (void *)dnsh, remains);
          return;
      }
      // Skip the query (name plus 2 bytes of type and 2 bytes of class)
      int len = (val >> 16) + 2 + 2;
      remains -= len;
      ptr += len;
      --qcount;
  }
  // Add info from TXT type answers, authorities, and additionals to the tables
  while(remains > 0)
  {
      int val = extract_dns_name(dnsh, ptr, remains, name, sizeof(name),
                                 0, FALSE);
      if(val < 0) {
          // Corrupt or incomplete answer data
          DPRINTF("%s: (%d) failed to get aname, offset %d, len %d\n",
                  __func__, val, (void *)ptr - (void *)dnsh, remains);
          return;
      }
      if((val & 0xffff) == 0) {
          return;
      }
      // Go past the name, type(2), class(2), TTL(4) and data_len(2)
      int len = (val >> 16) + 2 + 2 + 4 + 2;
      remains -= len;
      if(remains <= 0) {
          // Corrupt or no data
          DPRINTF("%s: incomplete answer, offset %d, len %d\n",
                  __func__, (void *)ptr - (void *)dnsh, remains);
          return;
      }
      struct mdns_answer *ahdr = (struct mdns_answer *)(ptr + (val >> 16));
      ptr += len;
      // We are now past the header, get the answer resource data length
      len = ntohs(ahdr->dlen);
      if(remains < len) {
          // answer data is incomplete or corrupt packet
          DPRINTF("%s: incomplete adata (%d), offset %d, len %d\n",
                  __func__, len, (void *)ptr - (void *)dnsh, remains);
          return;
      }

      for(;;) {
          if((ntohs(ahdr->type) == 16) && ((ntohs(ahdr->class) & 0x7FFF) == 1)) { // TXT IN record
              FP_MDNS_t *fp = add_mdns(ehdr->h_source, name, (val & 0xffff));
              if(!fp) {
                  // Can't add this entry to (or find it in) the table
                  DPRINTF("%s: failed to add <%s> to the table\n",
                          __func__, name);
                  break;
              }
              if(len > FP_MAX_MDNS_TXT) {
                  DPRINTF("%s: mDNS blob too large, %d of max %d, truncating\n",
                          __func__, len, FP_MAX_MDNS_TXT);
                  len = FP_MAX_MDNS_TXT;
              }
              memcpy(fp->blob, ptr, len);
              fp->blob_len = len;
          }
          if((ntohs(ahdr->type) == 33) && ((ntohs(ahdr->class) & 0x7FFF) == 1)) { // SRV IN record
              FP_MDNS_t *fp = add_mdns(ehdr->h_source, name, (val & 0xffff));
              if(!fp) {
                  // Can't add this entry to (or find it in) the table
                  DPRINTF("%s: failed to add <%s> to the table\n",
                          __func__, name);
                  break;
              }
              // advance past priority and weight to get the two port bytes
              fp->port = ntohs(*((unsigned short*)(ptr + 4)));
          }

          break;
      }
      remains -= len;
      ptr += len;
  }

  return;
}

// The "do_mdns_discovery" command processor.
int cmd_mdns_discovery(char *cmd, char *s, int s_len)
{
    json_t *array = NULL;
    size_t array_length;
    json_error_t jerr;
    int err = -1;
    char queries[FP_MAX_MDNS_TXT];
    UDP_PAYLOAD_t payload = { .len = 0 };
    int remainder = FP_MAX_MDNS_TXT;

    log("%s: processing mDNS discovery command\n", __func__);

    for(;;)
    {
        if(!s || s_len <= 0) {
            log("%s: error, data: %s, len: %d\n", __func__, s, s_len);
            break;
        }

        // Process the incoming JSON. It should contain an array of strings
        // of the form ["._device-info._tcp.local", "._ssh._tcp.local", ...]
        array = json_loads(s, 0, &jerr);
        if(!array) {
            log("%s: error at l:%d c:%d parsing mDNS service list: '%s'\n",
                __func__, jerr.line, jerr.column, jerr.text);
            break;
        }

        // Make sure we have an array with something inside
        array_length = json_array_size(array);
        if(array_length > 0) {
            int i;
            payload.len = 12; // DNS header length
            memset(queries, 0, payload.len);
            remainder -= payload.len;

            for(i = 0; i < array_length; i++)
            {
                json_t *service_obj = json_array_get(array, i);
                if(!service_obj) {
                    log("%s: error getting service_obj\n", __func__);
                    break;
                }

                size_t len = json_string_length(service_obj);
                const char *service = json_string_value(service_obj);
                if(len <= 0 || !service) {
                    log("%s: error getting service\n", __func__);
                    break;
                }

                // We need 5 extra chars; one for the string null terminator,
                // two for the type, and two for the class
                if(len + 5 <= remainder) {
                    int index = payload.len;
                    int ptr = 0;
                    while (ptr < len) {
                        if(service[ptr] == '.') {
                            index = payload.len + ptr;
                            queries[index] = 0;
                        } else {
                            queries[payload.len + ptr] = service[ptr];
                            queries[index] += 1;
                        }
                        ptr++;
                    }
                    // Append DNS query footer
                    queries[payload.len + len] = 0x00;     // NULL terminator
                    queries[payload.len + len + 1] = 0x00; // PTR 1
                    queries[payload.len + len + 2] = 0x0C; // PTR 2
                    queries[payload.len + len + 3] = 0x00; // Class 1 - Prefer Multicast
                    queries[payload.len + len + 4] = 0x01; // Class 2
                    // End DNS query footer

                    payload.len += len + 5;
                    queries[5] += 1; // Query count
                    remainder -= len + 5;
                }
            }
        } else {
            log("%s: malformed or empty mDNS service array (count: %d)\n",
                    __func__, array_length);
        }

        json_decref(array);

        err = 0;
        break;
    }

    if(!err) {
        if(payload.len > 12) {
            payload.data = queries;
            payload.dip = "224.0.0.251";
            payload.dport = 5353;
            payload.sport = 5353;
            util_enum_ifs(UTIL_IF_ENUM_RTR_LAN,
                          (UTIL_IF_ENUM_CB_t)send_udp_packet, &payload);
            log("%s: mDNS discovery successfully run\n", __func__);
        } else {
            log("%s: mDNS discovery did no run due to a lack of data!\n", __func__);
        }
    }

    return err;
}

// Init mDNS fingerprinting
// Returns: 0 - if successful
int fp_mdns_init(void)
{
    PKT_PROC_ENTRY_t *pe;

    // Allocate memory for the mDNS info table (never freed).
    // The memory for each element is allocated when required
    // and stores the pointer in the array. Once allocated the
    // entries are never freed (reused when needed).
    mdns_tbl = calloc(FP_MAX_DEV, sizeof(FP_MDNS_t *));
    if(!mdns_tbl) {
        DPRINTF("%s: failed to allocate memory for data tables\n", __func__);
        return -1;
    }

    // Add the collector packet processing entries.
    pe = &fp_mdns_pkt_proc;
    if(tpcap_add_proc_entry(pe) != 0)
    {
        DPRINTF("%s: tpcap_add_proc_entry() failed for: %s\n",
                __func__, pe->desc);
        return -1;
    }

    // Reset stats (more for consistency since they are in BSS
    // segment anyway)
    memset(&mdns_tbl_stats, 0, sizeof(mdns_tbl_stats));

    return 0;
}
