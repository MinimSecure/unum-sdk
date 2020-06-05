// (c) 2017-2018 minim.co
// device telemetry DNS information collector

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


#ifdef DEBUG
#define DNSTPRINTF(args...) ((tpcap_test_param.int_val == TPCAP_TEST_DNS) ? \
                             (printf(args)) : 0)
#else  // DEBUG
#define DNSTPRINTF(args...) /* Nothing */
#endif // DEBUG


// DNS response header
struct dns_rsp {
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

struct dns_answer {
    uint16_t type;  // answer type
    uint16_t class; // answers class
    uint32_t ttl;   // time to live
    uint16_t dlen;  // length of the data (following the answer header)
} __attribute__((packed));


// Forward declarations
static void dns_udp_rcv_cb(TPCAP_IF_t *tpif,
                           PKT_PROC_ENTRY_t *pe,
                           struct tpacket2_hdr *thdr,
                           struct iphdr *iph);


// DNS packets processing entry for device info collector
static PKT_PROC_ENTRY_t collector_dns_udp = {
    0,
    {},
    0,
    {},
    PKT_MATCH_TCPUDP_P1_ANY | PKT_MATCH_TCPUDP_UDP_ONLY,
    { .p1 = 53 },
    NULL, dns_udp_rcv_cb, NULL, NULL,
    "UDP src/dst port 53 for UDP DNS names resolution tracking"
};


// DNS IPs table (direct placement)
static DT_DNS_IP_t *ip_tbl;
// DNS names table (uses pointers)
static DT_DNS_NAME_t **name_tbl;

// Structures tracking DNS name and IP tables stats
static DT_TABLE_STATS_t name_tbl_stats;
static DT_TABLE_STATS_t ip_tbl_stats;


// Returns pointer to the DNS name table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
DT_TABLE_STATS_t *dt_dns_name_tbl_stats(int reset)
{
    static DT_TABLE_STATS_t tbl_stats;
    memcpy(&tbl_stats, &name_tbl_stats, sizeof(tbl_stats));
    if(reset) {
        memset(&name_tbl_stats, 0, sizeof(name_tbl_stats));
    }
    return &tbl_stats;
}

// Get the pointer to the DNS ip addresses table
// The table should only be accessed from the tpcap thread
DT_DNS_IP_t *dt_get_dns_ip_tbl(void)
{
    return ip_tbl;
}

// Returns pointer to the DNS IP table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
DT_TABLE_STATS_t *dt_dns_ip_tbl_stats(int reset)
{
    static DT_TABLE_STATS_t tbl_stats;
    memcpy(&tbl_stats, &ip_tbl_stats, sizeof(tbl_stats));
    if(reset) {
        memset(&ip_tbl_stats, 0, sizeof(ip_tbl_stats));
    }
    return &tbl_stats;
}

// Reset all DNS tables (including the table usage stats)
void dt_reset_dns_tables(void)
{
    int ii;

    // Reset IP table (it is direct placement, just memset)
    memset(ip_tbl, 0, DTEL_MAX_DNS_IPS * sizeof(DT_DNS_IP_t));
    // Reset name table (keeping already allocated items memory)
    for(ii = 0; ii < DTEL_MAX_DNS_NAMES; ii++) {
        DT_DNS_NAME_t *item = name_tbl[ii];
        if(item) {
            *(item->name) = 0;
            item->cname = NULL;
        }
    }
    // Reset stats
    dt_dns_ip_tbl_stats(TRUE);
    dt_dns_name_tbl_stats(TRUE);
    return;
}

// Find DNS IP enty in the IP table
// Returns a pointer to the IP table entry of NULL if not found
// Call only from the TPCAP thread/handlers
DT_DNS_IP_t *dt_find_dns_ip(IPV4_ADDR_t *ip)
{
    int ii, idx;
    DT_DNS_IP_t *ret = NULL;

    // Total # of find requests
    ++(ip_tbl_stats.find_all);

    // Generate hash-based index
    idx = util_hash(ip, sizeof(*ip)) % DTEL_MAX_DNS_IPS;

    for(ii = 0; ii < (DTEL_MAX_DNS_IPS / DT_SEARCH_LIMITER); ii++) {
        // If we hit an empty entry then the IP we are looking for
        // is not in the table (we NEVER remove inidividual entries)
        if(ip_tbl[idx].ipv4.i == 0) {
            break;
        }
        if(ip_tbl[idx].ipv4.i == ip->i) {
            ret = &(ip_tbl[idx]);
            break;
        }
        // Collision, try the next index
        idx = (idx + 1) % DTEL_MAX_DNS_IPS;
    }

    if(ii < 10) {
        ++(ip_tbl_stats.find_10);
    }
    if(!ret) {
        ++(ip_tbl_stats.find_fails);
    }

    return ret;
}

// Add to or update DNS IP->name mapping in the IP table
// Returns a pointer to the IP table entry added/updated or NULL
static DT_DNS_IP_t *add_dns_ip(IPV4_ADDR_t *ip, DT_DNS_NAME_t *dname)
{
    int ii, idx;
    DT_DNS_IP_t *ret = NULL;

    // Total # of add requests
    ++(ip_tbl_stats.add_all);

    // Generate hash-based index
    idx = util_hash(ip, sizeof(*ip)) % DTEL_MAX_DNS_IPS;

    for(ii = 0; ii < (DTEL_MAX_DNS_IPS / DT_SEARCH_LIMITER); ii++) {
        // If we hit an empty entry then the IP we are looking for
        // is not in the table (we NEVER remove inidividual entries)
        if(ip_tbl[idx].ipv4.i == 0) {
            ret = &(ip_tbl[idx]);
            ret->ipv4.i = ip->i;
            ret->refs = 0;
        } else if(ip_tbl[idx].ipv4.i != ip->i) {
            // Collision, try the next index
            idx = (idx + 1) % DTEL_MAX_DNS_IPS;
            continue;
        } else {
            // The IP is already listed
            ++(ip_tbl_stats.add_found);
        }
        // Update the DNS name
        ip_tbl[idx].dns = dname;
        ret = &(ip_tbl[idx]);
        break;
    }

    if(ii < 10) {
        ++(ip_tbl_stats.add_10);
    }
    if(!ret) {
        ++(ip_tbl_stats.add_busy);
    }

    return ret;
}

// Add DNS name to the name table
// Returns a pointer to the DNS name table record or NULL
static DT_DNS_NAME_t *add_dns_name(char *name)
{
    int ii, idx;
    DT_DNS_NAME_t *ret = NULL;
    int len = strlen(name);

    // Total # of add requests
    ++(name_tbl_stats.add_all);

    // Make sure the name is short enough to fit
    if(len >= DEVTELEMETRY_MAX_DNS) {
        ++(name_tbl_stats.add_limit);
        return NULL;
    }

    // Generate hash-based index
    idx = util_hash(name, len) % DTEL_MAX_DNS_NAMES;

    for(ii = 0; ii < (DTEL_MAX_DNS_NAMES / DT_SEARCH_LIMITER); ii++) {
        // If we hit an empty entry then the name we are looking for
        // is not in the table (we NEVER remove inidividual entries)
        if(!name_tbl[idx]) {
            ret = malloc(sizeof(DT_DNS_NAME_t));
            if(!ret) { // Run out of memory
                ++(name_tbl_stats.add_limit);
                return NULL;
            }
            *(ret->name) = 0;
            ret->cname = NULL;
            name_tbl[idx] = ret;
        }
        if(*(name_tbl[idx]->name) == 0) {
            ret = name_tbl[idx];
            strcpy(ret->name, name);
            break;
        }
        // Check if this is the same name
        if(strcmp(name_tbl[idx]->name, name) == 0) {
            ret = name_tbl[idx];
            ++(name_tbl_stats.add_found);
            break;
        }
        // Collision, try the next index
        idx = (idx + 1) % DTEL_MAX_DNS_NAMES;
    }

    if(!ret) {
        ++(name_tbl_stats.add_busy);
    } else if(ii < 10) {
        ++(name_tbl_stats.add_10);
    }

    return ret;
}

// DNS packet receive callback, called by tpcap thread for every
// UDP DNS packet it captures.
// Do not block or wait for anything here.
// Do not log anything from here.
static void dns_udp_rcv_cb(TPCAP_IF_t *tpif,
                           PKT_PROC_ENTRY_t *pe,
                           struct tpacket2_hdr *thdr,
                           struct iphdr *iph)
{
    struct udphdr *udph = ((void *)iph) + sizeof(struct iphdr);
    struct dns_rsp *dnsh = ((void *)udph) + sizeof(struct udphdr);

    // We are guaranteed to have the IP header, but have to keep checking
    // the DNS data being examined is actually captured.
    int remains = thdr->tp_snaplen;
    remains -= (thdr->tp_net - thdr->tp_mac) +
               sizeof(struct iphdr) + sizeof(struct udphdr);
    // If we do not have full DNS header ignore the packet
    if(remains < sizeof(struct dns_rsp)) {
        DNSTPRINTF("%s: incomplete DNS header, only %d bytes remains\n",
                   __func__, remains);
        return;
    }
    remains -= sizeof(struct dns_rsp);
    // Check if it is a standard DNS response
    if((dnsh->flags1 & 0x80) == 0 || (dnsh->flags1 & 0x78) != 0) {
        return;
    }
    // Make sure it is a no-error reply
    if((dnsh->flags2 & 0x0f) != 0) {
        return;
    }
    int qcount = ntohs(dnsh->qdcount);
    int acount = ntohs(dnsh->ancount);
    unsigned char *ptr = ((void *)dnsh) + sizeof(struct dns_rsp);
    char name[256];
    // Skip the query section
    while(qcount > 0 && remains > 0) {
        // Pull out the query name
        int val = extract_dns_name(dnsh, ptr, remains, NULL, 0, 0, TRUE);
        if(val < 0) {
            // Corrupt or incomplete query data
            DNSTPRINTF("%s: (%d) failed to get qname, offset %d, len %d\n",
                       __func__, val, (void *)ptr - (void *)dnsh, remains);
            return;
        }
        // Skip the query (name plus 2 bytes of type and 2 bytes of class)
        int len = (val >> 16) + 2 + 2;
        remains -= len;
        ptr += len;
        --qcount;
    }
    // Add info from CNAME and A type answers to the tables
    while(acount > 0 && remains > 0)
    {
        int val = extract_dns_name(dnsh, ptr, remains, name, sizeof(name), 0, TRUE);
        if(val < 0 || (val & 0xffff) == 0) {
            // Corrupt or incomplete answer data
            DNSTPRINTF("%s: (%d) failed to get aname, offset %d, len %d\n",
                       __func__, val, (void *)ptr - (void *)dnsh, remains);
            return;
        }
        // Go past the name, type(2), class(2), TTL(4) and data_len(2)
        int len = (val >> 16) + 2 + 2 + 4 + 2;
        remains -= len;
        if(remains <= 0) {
            // Corrupt or no data
            DNSTPRINTF("%s: incomplete answer, offset %d, len %d\n",
                       __func__, (void *)ptr - (void *)dnsh, remains);
            return;
        }
        struct dns_answer *ahdr = (struct dns_answer *)(ptr + (val >> 16));
        ptr += len;
        // We are now past the header, get the answer resource data length
        len = ntohs(ahdr->dlen);
        if(remains < len) {
            // answer data is incomplete or corrupt packet
            DNSTPRINTF("%s: incomplete adata (%d), offset %d, len %d\n",
                       __func__, len, (void *)ptr - (void *)dnsh, remains);
            return;
        }
        for(;;) {
            if(ntohs(ahdr->class) != 1) {
                // Not an IN (Internet) class answer, skip
                break;
            }
            if(ntohs(ahdr->type) == 5) { // CNAME record answer
                DT_DNS_NAME_t *qn = add_dns_name(name);
                if(!qn) {
                    // Can't add this entry to (or find it in) the table
                    DNSTPRINTF("%s: failed to add <%s> to the table\n",
                              __func__, name);
                    break;
                }
                int val = extract_dns_name(dnsh, ptr, len,
                                           name, sizeof(name), 0, TRUE);
                if(val < 0 || (val & 0xffff) == 0) {
                    // Corrupt or incomplete cname data field
                    DNSTPRINTF("%s: (%d) failed to get cname, offset %d\n",
                       __func__, val, (void *)ptr - (void *)dnsh);
                    break;
                }
                DT_DNS_NAME_t *cn = add_dns_name(name);
                if(!cn) {
                    // Can't add this entry to (or find it in) the table
                    DNSTPRINTF("%s: failed to add cname <%s> to the table\n",
                              __func__, name);
                    break;
                }
                // Record the last query name for this CNAME
                // Note: if there are multiple requests for different DNS
                //       names pointing to the same CNAME we only record
                //       the last request and will assume all those have been
                //       originated by the same original name, moreover all
                //       the requests asking directly for the CNAME will be
                //       assumed to be for that original name too.
                // Note: only record the CNAME if the chain ends at the
                //       next or after-next reference and there is no loop
                if((!qn->cname || !qn->cname->cname) &&
                   qn != cn && qn->cname != cn)
                {
                    cn->cname = qn;
                }
            }
            if(ntohs(ahdr->type) == 1 && len >= sizeof(IPV4_ADDR_t)) { // A record answer
                DT_DNS_NAME_t *qn = add_dns_name(name);
                if(!qn) {
                    // Can't add/find this entry to/in the table
                    DNSTPRINTF("%s: failed to add <%s> to the table\n",
                              __func__, name);
                    break;
                }
                if(!add_dns_ip((IPV4_ADDR_t *)ptr, qn)) {
                    DNSTPRINTF("%s: failed to add IP <" IP_PRINTF_FMT_TPL
                               "> to the table\n",
                               __func__, IP_PRINTF_ARG_TPL(ptr));
                }
            }

            break;
        }
        remains -= len;
        ptr += len;
        --acount;
    }

    return;
}

// DNS info collector init function
// Returns: 0 - if successful
int dt_dns_collector_init(void)
{
    PKT_PROC_ENTRY_t *pe;

    // Allocate memory for the DNS info tables (never freed).
    // The names table allocates memory for each element
    // when required and stores the pointer in the array.
    // Once allocated the DNS entries are never freed (reused
    // when needed).
    ip_tbl = calloc(DTEL_MAX_DNS_IPS, sizeof(DT_DNS_IP_t));
    name_tbl = calloc(DTEL_MAX_DNS_NAMES, sizeof(DT_DNS_NAME_t *));
    if(!name_tbl || !ip_tbl) {
        log("%s: failed to allocate memory for data tables\n", __func__);
        return -1;
    }

    // Add the collector packet processing entries.
    pe = &collector_dns_udp;
    if(tpcap_add_proc_entry(pe) != 0)
    {
        log("%s: tpcap_add_proc_entry() failed for: %s\n",
            __func__, pe->desc);
        return -1;
    }

    // Reset stats (more for consistency since they are in BSS
    // segment anyway)
    memset(&name_tbl_stats, 0, sizeof(name_tbl_stats));
    memset(&ip_tbl_stats, 0, sizeof(ip_tbl_stats));

    return 0;
}

#ifdef DEBUG
void dt_dns_tbls_test(void)
{
    // Dump the tables
    int ii, count = 0, ecount = 0;
    printf("IP->DNS tables dump:\n");
    for(ii = 0; ii < DTEL_MAX_DNS_IPS; ii++) {
        DT_DNS_IP_t *ip_item = &(ip_tbl[ii]);
        if(ip_item->ipv4.i != 0) {
            count++;
            printf("  [%04d] " IP_PRINTF_FMT_TPL,
                   ip_item->refs, IP_PRINTF_ARG_TPL(ip_item->ipv4.b));
            DT_DNS_NAME_t *n_item = ip_item->dns;
            if(!n_item) {
                printf(" -> No Name Error\n");
                ++ecount;
                continue;
            }
            while(n_item) {
                printf(" -> <%s>", n_item->name);
                n_item = n_item->cname;
            }
            if(dt_find_dns_ip(&ip_item->ipv4) != ip_item) {
                printf(" [search error!]\n");
                ++ecount;
            }
            printf("\n");
        }
    }
    printf("  -----------------------------\n");
    printf("  Total: %d Errors: %d\n\n", count, ecount);

    // Test find for an address we should not have
    IPV4_ADDR_t lip;
    lip.i = inet_addr("127.1.1.1");
    if(dt_find_dns_ip(&lip) != NULL) {
        printf("Error, search for an unknown address did not fail!\n\n");
    } else {
        printf("Verified no result searching for unknown address\n\n");
    }

    // Dump the stats
    DT_TABLE_STATS_t *name_tbl_st = dt_dns_name_tbl_stats(FALSE);
    DT_TABLE_STATS_t *ip_tbl_st = dt_dns_ip_tbl_stats(FALSE);
    printf("DNS name table stats:\n");
    printf("  add_all = %lu\n", name_tbl_st->add_all);
    printf("  add_limit = %lu\n", name_tbl_st->add_limit);
    printf("  add_busy = %lu\n", name_tbl_st->add_busy);
    printf("  add_10 = %lu\n", name_tbl_st->add_10);
    printf("  add_found = %lu\n", name_tbl_st->add_found);
    printf("IP table stats:\n");
    printf("  add_all = %lu\n", ip_tbl_st->add_all);
    printf("  add_limit = %lu\n", ip_tbl_st->add_limit);
    printf("  add_busy = %lu\n", ip_tbl_st->add_busy);
    printf("  add_10 = %lu\n", ip_tbl_st->add_10);
    printf("  add_found = %lu\n", ip_tbl_st->add_found);
    printf("  find_all = %lu\n", ip_tbl_st->find_all);
    printf("  find_fails = %lu\n", ip_tbl_st->find_fails);
    printf("  find_10 = %lu\n", ip_tbl_st->find_10);

    printf("\n");
}
#endif // DEBUG
