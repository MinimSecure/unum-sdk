// (c) 2018 minim.co
// ping flood tester code

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// TX ring socket, address and length
static int tx_ss = -1;
static void *tx_ring = MAP_FAILED;
static unsigned int tx_ring_len = 0;

// Accumulator for counting sent requests and received replies data
static unsigned long dot11_bytes_tx_count; // bytes tx from 802.11 counters
static unsigned long bytes_tx_count;       // bytes tx attempted
static unsigned long total_time_tms; // the time is in 10 microseconds units

// Length of the packet in the TX ring
static unsigned long  bytes_per_pkt;

// ID for using in ICMP pkts during the agent run session
static unsigned short my_icmp_id;


// Generate pingflood test report
static char *flood_test_report(unsigned char *mac, IPV4_ADDR_t *ip)
{
    char mac_str[MAC_ADDRSTRLEN];
    char ip_str[INET_ADDRSTRLEN];

    sprintf(mac_str, MAC_PRINTF_FMT_TPL, MAC_PRINTF_ARG_TPL(mac));
    sprintf(ip_str, IP_PRINTF_FMT_TPL, IP_PRINTF_ARG_TPL(ip));

    // Template test results reporting JSON object
    JSON_OBJ_TPL_t flood_test_tpl = {
      { "mac",       { .type = JSON_VAL_STR, { .s = mac_str }}},
      { "ip",        { .type = JSON_VAL_STR, { .s = ip_str }}},
      { "t_msec",    { .type = JSON_VAL_UL,  { .ul = total_time_tms / 100 }}},
      { "tx_b",      { .type = JSON_VAL_UL,  { .ul = bytes_tx_count }}},
      { "rx_b",      { .type = JSON_VAL_UL,  { .ul = 0 }}}, // no longer used
      { "dot11_tx_b",{ .type = JSON_VAL_UL,  { .ul = dot11_bytes_tx_count }}},
      { NULL }
    };

    return util_tpl_to_json_str(flood_test_tpl);
}

// Fill in Ethernet header
static int fill_eth_hdr(void *ptr, unsigned char *src, unsigned char *dst)
{
    struct ether_header *ehdr = (struct ether_header *)ptr;

    memcpy(ehdr->ether_shost, src, sizeof(ehdr->ether_shost));
    memcpy(ehdr->ether_dhost, dst, sizeof(ehdr->ether_dhost));
    ehdr->ether_type = htons(ETH_P_IP);

    return sizeof(struct ether_header);
}

// Fill in IP header
static int fill_ip_hdr(void *ptr, IPV4_ADDR_t *src, IPV4_ADDR_t *dst,
                       unsigned short payload_len)
{
    struct iphdr *ip = (struct iphdr *)ptr;

    memset(ip, 0, sizeof (*ip));
    ip->version = 4;    // IPv4
    ip->ihl = 5;        // 20 bytes header
    ip->tos = IPTOS_THROUGHPUT;
    ip->tot_len = htons(payload_len + sizeof(struct iphdr));
    ip->id = htons((unsigned short)rand()); // ID
    ip->ttl = 64;       // Max hops
    ip->protocol = 1;   // ICMP
    ip->saddr = src->i; // Src IP
    ip->daddr = dst->i; // Dst IP
    ip->frag_off |= htons(IP_DF);

    // Calculate IP checksum
    unsigned short cs = util_ip_cksum((unsigned short *)ptr,
                                      payload_len + sizeof(struct iphdr));
    ip->check = cs; // byte order independent

    return sizeof(struct iphdr);
}

// Fill in ICMP header and payload
static int fill_icmp(void *ptr)
{
    struct icmphdr *icmp = (struct icmphdr *)ptr;

    if(my_icmp_id == 0) {
        my_icmp_id = htons((short)syscall(SYS_gettid));
    }

    memset(icmp, 0, sizeof(*icmp));
    icmp->type = ICMP_ECHOREPLY;   // Sending echo reply, so the dst does
                                   // not bother to answer
    icmp->un.echo.id = my_icmp_id; // Echo request ID
    // icmp->un.echo.sequence = 0;

    // Add the payload
    unsigned int ii, *iptr = (unsigned int *)(ptr + sizeof(struct icmphdr));
    for(ii = 0; ii < PINGFLOOD_PAYLOAD_SIZE >> 4; ++ii) {
        unsigned int val = 0x6d496e49;
        iptr[ii] = val;
    }

    // Calculate the checksum
    int icmp_len = sizeof(struct icmphdr)+ (PINGFLOOD_PAYLOAD_SIZE & (~3));
    unsigned short cs = util_ip_cksum((unsigned short *)ptr, icmp_len);
    icmp->checksum = cs; // byte order independent

    return icmp_len;
}

// Find next free buffer (start searching from buffer 0 if restart is TRUE)
static struct tpacket_hdr *get_free_buffer(int restart)
{
    static int next = 0;
    int i, ii;

    if(restart) {
        ii = 0;
    } else {
        ii = next;
    }
    for(i = 0; i < PINGFLOOD_TX_PKT_NUM_BUFS; ++i)
    {
        struct tpacket_hdr *tph =
            (struct tpacket_hdr *)(tx_ring + (PINGFLOOD_TX_PKT_BUF_SIZE * ii));
        ii = (ii + 1) % PINGFLOOD_TX_PKT_NUM_BUFS;
        if((volatile int)tph->tp_status == TP_STATUS_AVAILABLE)
        {
            next = ii;
            return tph;
        }
    }

    return NULL;
}

// Fill in the tx ring w/ the ping packets
static int prepare_ring(unsigned char *smac, IPV4_ADDR_t *sip,
                        unsigned char *dmac, IPV4_ADDR_t *dip)
{
    int i;
    int sum_pkt_len = 0;
    int num_pkts = 0;

    for(i = 0; i < PINGFLOOD_TX_PKT_NUM_BUFS; i++)
    {
        struct tpacket_hdr *tph =
            (struct tpacket_hdr *)(tx_ring + (PINGFLOOD_TX_PKT_BUF_SIZE * i));
        void *pkt = (void *)tph + (TPACKET_HDRLEN - sizeof(struct sockaddr_ll));
        void *pkt_ip = pkt + sizeof(struct ether_header);
        void *pkt_icmp = pkt + sizeof(struct ether_header) +
                               sizeof(struct iphdr);
        int pkt_len = 0;
        pkt_len += fill_icmp(pkt_icmp);
        pkt_len += fill_ip_hdr(pkt_ip, sip, dip, pkt_len);
        pkt_len += fill_eth_hdr(pkt, smac, dmac);

        tph->tp_len = pkt_len;
        sum_pkt_len += pkt_len;
        ++num_pkts;
    }

    return (sum_pkt_len / num_pkts);
}

// Send packets from the ring at "byps_limit" rate for the "msec" time period.
static int do_flood(unsigned char *mac,
                    unsigned long msec, unsigned long byps_limit)
{
    int ii, err = 0;
    unsigned long cur_t, end_t, last_t;
    unsigned long pkts_sent;
    WIRELESS_COUNTERS_t wc_before = { .flags = 0 };
    WIRELESS_COUNTERS_t wc_after = { .flags = 0 };

    // Calculate token bucket credit interval and initial creadit.
    // The interval is calculated to allow 1 packet transmission,
    // it is in microseconds (therefore non 0 even for the high rates).
    // The initial credit is equal to the # of tokens for one interval.
    unsigned long tx_interval = (bytes_per_pkt * 1000000) / byps_limit;
    unsigned long bytes_tx_bucket = ((tx_interval / 10) * (byps_limit / 100)) / 1000;

    if(wireless_get_sta_counters(NULL, mac, &wc_before) != 0) {
        log("%s: failed to get start counters for " MAC_PRINTF_FMT_TPL "\n",
            __func__, MAC_PRINTF_ARG_TPL(mac));
        return -1;
    }

    pkts_sent = 0;
    cur_t = util_time(100000);
    end_t = cur_t + (msec * 100);
    while((end_t - cur_t) <= (msec * 100))
    {
        struct tpacket_hdr *tph;

        // Mark the # of bytes matching the # of tokens in the bucket
        // as ready to send.
        int pkts_pending = 0;
        while(bytes_tx_bucket >= bytes_per_pkt)
        {
            tph = get_free_buffer(!pkts_sent);
            if(!tph) {
                break;
            }
            tph->tp_status = TP_STATUS_SEND_REQUEST;
            __sync_synchronize();
            bytes_tx_bucket -= bytes_per_pkt;
            ++pkts_pending;
            ++pkts_sent;
        }

        // Try to send all buffers with TP_STATUS_SEND_REQUEST
        if(pkts_pending) {
            err = send(tx_ss, NULL, 0, MSG_DONTWAIT);
            if(err < 0 && EAGAIN != errno && EWOULDBLOCK != errno)
            {
                log("%s: send() error %s\n", __func__, strerror(errno));
                break;
            }
        }

        // Wait for the tx_interval
        usleep(tx_interval);

        // Use real time we've waited to calculate the credit
        // Note: sleep_t is in 10 microseconds units
        last_t = cur_t;
        cur_t = util_time(100000);
        unsigned long sleep_t = cur_t - last_t;
        bytes_tx_bucket += (sleep_t * (byps_limit / 1000)) / 100;
        total_time_tms += sleep_t;
    }

    if(err < 0) {
        return err;
    }

    if(wireless_get_sta_counters(NULL, mac, &wc_after) != 0) {
        log("%s: failed to get end counters for " MAC_PRINTF_FMT_TPL "\n",
            __func__, MAC_PRINTF_ARG_TPL(mac));
        return -2;
    }

    // Count not yet transmitted buffers
    for(ii = 0; ii < PINGFLOOD_TX_PKT_NUM_BUFS; ++ii)
    {
        struct tpacket_hdr *tph =
            (struct tpacket_hdr *)(tx_ring + (PINGFLOOD_TX_PKT_BUF_SIZE * ii));
        if((volatile int)tph->tp_status != TP_STATUS_AVAILABLE)
        {
            --pkts_sent;
        }
    }

    // Bytes we attempted to transmit
    bytes_tx_count = pkts_sent * bytes_per_pkt;

    // Bytes 802.11 layer was able to deliver
    if(strcmp(wc_before.ifname, wc_after.ifname) != 0) {
        log("%s: error, " MAC_PRINTF_FMT_TPL " moved from %s to %s\n",
            __func__, MAC_PRINTF_ARG_TPL(mac),
            wc_before.ifname, wc_after.ifname);
        return -3;
    }
    if((W_COUNTERS_MIF & wc_after.flags) != 0) {
        log("%s: warning, " MAC_PRINTF_FMT_TPL " \n",
            __func__, MAC_PRINTF_ARG_TPL(mac),
            wc_before.ifname, wc_after.ifname);
    }
    if((W_COUNTERS_32B & wc_after.flags) != 0) {
        // 32 bit counters
        unsigned int cnt_s = wc_before.tx_b;
        unsigned int cnt_e = wc_after.tx_b;
        dot11_bytes_tx_count = cnt_e - cnt_s;
    } else {
        // 64 bit counters
        dot11_bytes_tx_count = (wc_after.tx_b - wc_before.tx_b);
    }

#ifdef WT_PINGFLOOD_DO_NOT_USE
    dot11_bytes_tx_count = bytes_tx_count;
#endif // WT_PINGFLOOD_DO_NOT_USE

    log("%s: pkts/bytes sent %lu/%lu, 802.11 bytes delivered on %s %lu\n",
        __func__, pkts_sent, bytes_tx_count,
        wc_after.ifname, dot11_bytes_tx_count);

    return 0;
}

// Set up tx ring and socket
static int tx_ring_setup(char *ifname)
{
    int ifidx, ss, val;
    struct sockaddr_ll my_addr;

    // Make sure no ring and no socket
    if(tx_ss >= 0 || tx_ring_len > 0) {
        log("%s: error, busy\n", __func__);
        return -1;
    }

    ss = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(ss < 0)
    {
        log("%s: socket() failed, %s\n", __func__, strerror(errno));
        return -2;
    }

    // Get the index of the network interface
    struct ifreq s_ifr;
    memset(&s_ifr, 0, sizeof(s_ifr));
    strncpy(s_ifr.ifr_name, ifname, sizeof(s_ifr.ifr_name)-1);
    if(ioctl(ss, SIOCGIFINDEX, &s_ifr) < 0)
    {
        close(ss);
        log("%s: SIOCGIFINDEX failed, %s\n", __func__, strerror(errno));
        return -3;
    }
    // Update with interface index
    ifidx = s_ifr.ifr_ifindex;

    // Bind to the interface
    memset(&my_addr, 0, sizeof(struct sockaddr_ll));
    my_addr.sll_family = AF_PACKET;
    my_addr.sll_protocol = htons(ETH_P_ALL);
    my_addr.sll_ifindex = ifidx;
    if(bind(ss, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_ll)) < 0)
    {
        close(ss);
        log("%s: bind() failed, %s\n", __func__, strerror(errno));
        return -4;
    }

    // Set packet loss option
    val = 1;
    if(setsockopt(ss, SOL_PACKET, PACKET_LOSS, (char *)&val, sizeof(val)) < 0)
    {
        close(ss);
        log("%s: PACKET_LOSS failed, %s\n", __func__, strerror(errno));
        return -5;
    }

#ifdef PACKET_QDISC_BYPASS
    // Set PACKET_QDISC_BYPASS option (only available in 3.14 or later)
    val = 1;
    if(setsockopt(ss, SOL_PACKET, PACKET_QDISC_BYPASS,
                  (char *)&val, sizeof(val)) < 0)
    {
        close(ss);
        log("%s: PACKET_QDISC_BYPASS failed, %s\n", __func__, strerror(errno));
        return -6;
    }
#endif // PACKET_QDISC_BYPASS

    // Set up TX ring
    struct tpacket_req req;
    memset(&req, 0, sizeof(req));
    req.tp_block_size = getpagesize();
    req.tp_frame_size = PINGFLOOD_TX_PKT_BUF_SIZE;
    req.tp_block_nr   = PINGFLOOD_TX_PKT_NUM_BUFS /
                            (getpagesize() / PINGFLOOD_TX_PKT_BUF_SIZE);
    req.tp_frame_nr   = PINGFLOOD_TX_PKT_NUM_BUFS;

    if(setsockopt(ss, SOL_PACKET, PACKET_TX_RING,
                  (char *)&req, sizeof(req)) < 0)
    {
        close(ss);
        log("%s: PACKET_TX_RING failed, %s\n", __func__, strerror(errno));
        return -7;
    }

    // mmap Tx ring memory
    unsigned int len = req.tp_block_size * req.tp_block_nr;
    void *ring = mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED, ss, 0);
    if(ring == MAP_FAILED) {
        close(ss);
        log("%s: mmap error, %s\n", __func__, strerror(errno));
        return -8;
    }

    // Setup complete
    tx_ss = ss;
    tx_ring = ring;
    tx_ring_len = len;

    return 0;
}

// Free Tx ring resources
static void tx_ring_free(void)
{
    if(tx_ring_len > 0)
    {
        if(munmap(tx_ring, tx_ring_len) != 0)
        {
            log("%s: error unmapping memory at %p, len %u\n",
                __func__, tx_ring, tx_ring_len);
        }
        tx_ring_len = 0;
        tx_ring = MAP_FAILED;
    }
    if(tx_ss >= 0)
    {
        close(tx_ss);
        tx_ss = -1;
    }
    return;
}

// The engine of the pingflood test
static char *pingflood(char *ifname, unsigned char *mac, IPV4_ADDR_t *ip,
                       unsigned long test_msec, unsigned long byps_limit)
{
    char *jstr = NULL;
    DEV_IP_CFG_t my_ipcfg;
    unsigned char my_mac[6];

    // Get interface IP and MAC
    if(util_get_ipcfg(ifname, &my_ipcfg) != 0 ||
       util_get_mac(ifname, my_mac) != 0)
    {
        log("%s: failed to get IP and/or MAC for %s\n", __func__, ifname);
        return NULL;
    }
    // Set up tx ring
    if(tx_ring_setup(ifname) < 0) {
        log("%s: %s tx ring setup has failed, aborting\n", __func__, ifname);
        return NULL;
    }

    // Fill in the ring w/ the packets
    bytes_per_pkt = prepare_ring(my_mac, &my_ipcfg.ipv4, mac, ip);

    // Set up counters
    bytes_tx_count = dot11_bytes_tx_count = 0;
    total_time_tms = 0;

    // Send packets from the ring at up to the specified rate
    if(do_flood(mac, test_msec, byps_limit) == 0)
    {
        // Generate the report
        jstr = flood_test_report(mac, ip);
    }

    // Free tx ring resources
    tx_ring_free();

    return jstr;
}

// Performs device connection speed test using ping flood
// Note: use for local wireless devices only
// tgt - target device IP address
// Returns: JSON string w/ results (must be freed w/ util_free_json_str())
//          or NULL if fails
char *do_pingflood_test(char *target)
{
    char tspec[256];
    char mac_str[MAC_ADDRSTRLEN];
    char ip_str[INET_ADDRSTRLEN];
    char param[3][16];
    unsigned int val[3];
    int  params, ii;

    for(ii = 0; ii < sizeof(tspec) - 1 && *(target + ii) != 0; ++ii) {
        tspec[ii] = *(target + ii);
        if(tspec[ii] == '/' || tspec[ii] == '?' ||
           tspec[ii] == '&' || tspec[ii] == '=')
        {
            tspec[ii] = ' ';
        }
    }
    tspec[ii] = 0;
    int ret = sscanf(tspec, "%17s %15s %15s %u %15s %u %15s %u",
                     mac_str, ip_str,
                     param[0], &val[0], param[1], &val[1], param[2], &val[2]);
    if(ret < 2 || (ret > 2 && (ret & 1) != 0) || ret > 8) {
        log("%s: invalid target <%s>\n", __func__, target);
        return NULL;
    }
    params = (ret - 2) >> 1;

    // limit test time to # of millisec
    unsigned long test_msec = PINGFLOOD_TEST_TIME_SEC * 1000;
    // limit sending to bytes/sec
    unsigned long byps_limit = PINGFLOOD_SEND_BYPS_LIMIT;

    for(ii = 0; ii < params; ++ii) {
        if(strcmp(param[ii], "test_sec") == 0) {
            test_msec = val[ii] * 1000;
        } else if(strcmp(param[ii], "byps_limit") == 0) {
            byps_limit = val[ii];
        } else {
            log("%s: invalid parameter <%s>\n", __func__, param[ii]);
        }
    }

    log("%s: MAC:%s IP:%s test_sec:%lu byps_limit:%lu\n",
        __func__, mac_str, ip_str, test_msec / 1000, byps_limit);
    if(test_msec < PINGFLOOD_MIN_TEST_TIME ||
       test_msec > PINGFLOOD_MAX_TEST_TIME)
    {
        log("%s: test time is not in the %d-%d sec range\n",
            __func__, PINGFLOOD_MIN_TEST_TIME, PINGFLOOD_MAX_TEST_TIME);
        return NULL;
    }
    if(byps_limit < PINGFLOOD_MIN_RATE_LIMIT ||
       byps_limit > PINGFLOOD_MAX_RATE_LIMIT)
    {
        log("%s: rate limit is not in the %d-%d Bps range\n",
            __func__, PINGFLOOD_MIN_RATE_LIMIT, PINGFLOOD_MAX_RATE_LIMIT);
        return NULL;
    }

    // Convert MAC string to binary form
    unsigned char mac[6];
    unsigned int m[6];
    if(sscanf(mac_str, MAC_PRINTF_FMT_TPL,
              &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6)
    {
        log("%s: invalid MAC address <%s>\n", __func__, mac_str);
        return NULL;
    }
    for(ii = 0; ii < 6; ++ii) {
        mac[ii] = (unsigned char)m[ii];
    }

    // Convert IP string to binary
    IPV4_ADDR_t ip;
    struct in_addr addr;
    if(!inet_aton(ip_str, &addr)) {
        log("%s: invalid IP address <%s>\n", __func__, ip_str);
        return NULL;
    }
    ip.i = addr.s_addr;

    // figure out what interface to use
    char ifname[IFNAMSIZ] = "";
    ii = util_find_if_by_ip(&ip, ifname);
    if(ii < 0) {
        log("%s: no interface match for %s\n", __func__, ip_str);
        return NULL;
    }
    if(ii > 0) {
        log("%s: more than one interface match for %s\n", __func__, ip_str);
        return NULL;
    }

    return pingflood(ifname, mac, &ip, test_msec, byps_limit);
}

#ifdef DEBUG
void test_pingflood(void)
{
    char target[80];

    printf("Testing 'pingflood', please provide destination info.\n");
    printf("Format <MAC>/<IP>[?test_sec=<VAL>[&byps_limit=<VAL>]...]\n");
    scanf("%79s", target);

    printf("Starting TPCAP...");
    if(tpcap_init(INIT_LEVEL_TPCAP) != 0) {
        printf("error\n");
        return;
    } else {
        printf("ok\n");
    }

    set_activate_event();
    sleep(5);

    printf("Executing ping flood test...\n");
    char *rsp = do_pingflood_test(target);
    printf("results: \n---\n%s\n---\n", rsp);

    abort();
    return;
}
#endif // DEBUG
