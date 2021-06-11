// (c) 2017-2019 minim.co
// fPort Scanning subsystem

#include "unum.h"

/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE

#ifdef DEBUG
#define DPRINTF(args...) ((get_test_num() == U_TEST_PORT_SCAN) ? \
                          (printf(args)) : 0)
#else  // DEBUG
#define DPRINTF(args...) /* Nothing */
#endif // DEBUG

// URL to send the port scan data, parameters:
// - the URL prefix
// - MAC addr of gateway (in xx:xx:xx:xx:xx:xx format)
// - MAC addr of device scanned
#define POST_SCAN_RSP_PATH "/v3/unums/%s/devices/%s/port_info"


// Forward declaration for the packet capturing rule
static void scan_ack_rcv_cb(TPCAP_IF_t *tpif,
                            PKT_PROC_ENTRY_t *pe,
                            struct tpacket2_hdr *thdr,
                            struct iphdr *iph);

// Flag indicating the port scanning thread is running
static int g_port_scan_in_progress = FALSE;

// IP address queue list head
static UTIL_QI_t g_queue = { &g_queue, &g_queue };

// IP address queue
static PORT_SCAN_DEVICE_t g_devices_a[PORT_SCAN_QUEUE_LIMIT];

// Mutex for protecting fetch URLs globals and the queue
static UTIL_MUTEX_t g_port_scan_mutex = UTIL_MUTEX_INITIALIZER;

// ACK to syn capturing rule
static PKT_PROC_ENTRY_t tcp_ack_pkt_proc = {
    0,
    {},
    PKT_MATCH_IP_A1_SRC |
    PKT_MATCH_IP_MY_DST, // only receive responses (unicast) to our scan
    { .a1 = { .i = 0 }}, // from the IP we are scanning (will be set later)
    PKT_MATCH_TCPUDP_TCP_ONLY | PKT_MATCH_TCPUDP_P1_DST,
    { .p1 = 0 },         // the port will be set by the scanner later
    NULL, scan_ack_rcv_cb, NULL, NULL,
    "ACK for security TCP port scanner"
};

// Global scan data (set by scanner before tcp_ack_pkt_proc
// and the sender are activated)
static struct {
    int s;            // socket the sender uses for transmitting SYNs
    IPV4_ADDR_t ipv4_src; // source IP address for transmitting
    unsigned short sport; // source port for sending SYNs (net byte order)
    IPV4_ADDR_t ipv4_dst; // destination IP address for transmitting
    PORT_RANGE_MAP_t *to_scan;   // port range w/ ports to scan
    PORT_RANGE_MAP_t *from_scan; // port range to map open ports in
    unsigned int seq; // sequence # to use in SYNs (ACKs should be +1)
} scan_params;


// TCP ACK packets receive callback for the port scanner, called by tpcap
// thread for every TCP packet to the TCP port we chose for scanning.
// Do not block or wait for anything here.
// Note: this should be the only thread updating the port ranges
//       during the device scan phase
static void scan_ack_rcv_cb(TPCAP_IF_t *tpif,
                            PKT_PROC_ENTRY_t *pe,
                            struct tpacket2_hdr *thdr,
                            struct iphdr *iph)
{
    //struct ethhdr *ehdr = (struct ethhdr *)((void *)thdr + thdr->tp_mac);
    struct tcphdr *tcph = ((void *)iph) + sizeof(struct iphdr);

    // Set the port as open in the port map
    unsigned short port = ntohs(tcph->source);

    // We should have SYN and ACK bits set in the response if the port
    // is open. If RST is set instead of SYN, then the port is closed.
    if(tcph->ack && tcph->syn) {
        UTIL_PORT_RANGE_SET(scan_params.from_scan, port);
        UTIL_PORT_RANGE_CLEAR(scan_params.to_scan, port);

        DPRINTF("%s: IP:" IP_PRINTF_FMT_TPL "->" IP_PRINTF_FMT_TPL
                " ports:%d->%d SYN:%d ACK:%d\n", __func__,
                IP_PRINTF_ARG_TPL(&iph->saddr), IP_PRINTF_ARG_TPL(&iph->daddr),
                ntohs(tcph->source), ntohs(tcph->dest),
                tcph->syn, tcph->ack);

    } else if(tcph->ack && tcph->rst) {
        UTIL_PORT_RANGE_CLEAR(scan_params.to_scan, port);
    }

    return;
}

// Send the port scan back up to the server
// Returns: 0 if successful, error code otherwise
static int report_device_scan(json_t *device, const char *mac,
                              PORT_RANGE_MAP_t *from_scan)
{
    int ret = 0;
    char url[256];
    char *my_mac = util_device_mac();

    // Check that we have MAC address
    if(!my_mac) {
        log("%s: cannot get LAN MAC\n", __func__);
        return -1;
    }

    // Post the downloaded data to the server
    util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API, url, sizeof(url),
                   POST_SCAN_RSP_PATH, my_mac, mac);

    json_t *open_port_array = NULL;
    char *json = NULL;
    http_rsp *rsp = NULL;

    for(;;) {
        open_port_array = util_port_range_to_json(from_scan);
        if(!open_port_array) {
            log("%s: cannot create port array\n", __func__);
            break;
        }

        if(json_object_set_new(device, "open", open_port_array) != 0) {
            log("%s: cannot append open port array\n", __func__);
            break;
        }
        open_port_array = NULL;

        json = util_json_obj_to_str(device);
        if(!json) {
            log("%s: cannot allocate JSON response\n", __func__);
            break;
        }

#if DEBUG
        if(get_test_num() == U_TEST_PORT_SCAN)
        {
            printf("%s: JSON for <%s>:\n%s\n", __func__, url, json);
            break;
        }
#endif // DEBUG

        http_rsp *rsp = http_post(url, NULL, json, strlen(json));
        if(rsp == NULL) {
            ret = -2;
        } else {
            if((rsp->code / 100) != 2) {
                ret = -3;
            }
        }

        break;
    }

    if(rsp) {
        free_rsp(rsp);
        rsp = NULL;
    }
    if(json) {
        util_free_json_str(json);
        json = NULL;
    }
    if(open_port_array) {
        json_decref(open_port_array);
        open_port_array = NULL;
    }

    return ret;
}

// Probe port on a IPv4 address (by sendig SYN pkt)
//  scan_delay - msecs to wait between each send
//  wait_time - msecs to wait after each send cycle (all probed ports)
//  retries - how many scan retries to make
// Returns: 0 - if successful, negative if fails
// Note: The to_scan port range is updated by the receiver on the fly
//       (no locking) to clear the ports we no longer need to scan.
//       This should work as long as we do not try to change it as well.
static int probe_ports(unsigned long scan_delay, unsigned long wait_time,
                       unsigned long retries)
{
    int i, ii, ret = 0;
    int s = scan_params.s;                    // socket
    unsigned short sport = scan_params.sport; // src port (net order)
    unsigned int seq = scan_params.seq;       // sequence #
    PORT_RANGE_MAP_t *to_scan = scan_params.to_scan; // ports to scan

    struct pseudo_header {
        unsigned int source_address;
        unsigned int dest_address;
        unsigned char placeholder;
        unsigned char protocol;
        unsigned short tcp_length;
        struct tcphdr tcp;
    } *psh; // pseudoheader for TCP checksum
    char pkt[(sizeof(struct pseudo_header) + 15) & (~0xf)]; // data buf

    // Set pointers to the pseudo and TCP header within the buffer
    psh = (struct pseudo_header *)pkt;
    struct tcphdr *tcph = &(psh->tcp);

    // Prepare the data preset for all sends
    memset(pkt, 0, sizeof(pkt));

    // TCP Header
    tcph->source = sport;
    //tcph->dest = 0;
    tcph->seq = seq;
    //tcph->ack_seq = 0;
    tcph->doff = sizeof(struct tcphdr) / 4;
    //tcph->fin=0;
    tcph->syn=1;
    //tcph->rst=0;
    //tcph->psh=0;
    //tcph->ack=0;
    //tcph->urg=0;
    tcph->window = htons(14600);
    //tcph->check = 0;
    //tcph->urg_ptr = 0;

    // Prepare fields in the pseudoheader
    psh->source_address = scan_params.ipv4_src.i;
    psh->dest_address = scan_params.ipv4_dst.i;
    //psh->placeholder = 0;
    psh->protocol = IPPROTO_TCP;
    psh->tcp_length = htons(sizeof(struct tcphdr));

    // Initial checksum (for dport == 0)
    tcph->check = util_ip_cksum((unsigned short *)psh, sizeof(*psh));

    // Send the SYN pkts
    for(ii = 0; ii <= retries; ++ii)
    {
        unsigned long sum;
        int count, count_failed;
        unsigned short port, port_old;

        DPRINTF("Scan pass %d, send delay %lu\n", ii + 1, scan_delay);

        // Send the SYN pkts updating the dport and checksum
        // only for each send
        count = count_failed = 0;
        for(i = 0; i < to_scan->len; i++) {
            port = to_scan->start + i;
            if(!UTIL_PORT_RANGE_GET(to_scan, port)) {
                continue;
            }
            ++count;

            // Update checksum (RFC1141)
            port_old = ntohs(tcph->dest);
            tcph->dest = htons(port);
            sum = port_old + (~port & 0xffff);
            sum += ntohs(tcph->check);
            sum = (sum & 0xffff) + (sum >> 16);
            tcph->check = htons(sum + (sum >> 16));

            if(send(s, tcph, sizeof(struct tcphdr), 0) < 0) {
                ++count_failed;
            }

            util_msleep(scan_delay);
        }
        if(count_failed > 0) {
            ret = -1;
        }

        DPRINTF("Scan pass %d complete, sent %d, failed %d, waiting %lu\n",
                ii + 1, count, count_failed, wait_time);

        // Wait for the results
        util_msleep(wait_time);
    }

    return ret;
}

// Interface IP check callback for scan_device(). It returns
// non-0 value if the interface IP matches to the IPv4 addr passed in
// through the 'ipv4' pointer. The util_enum_ifs() then returns
// the number of times a non-0 valie was reported by the callback and we
// will examine that value to find out if the 'ipv4' matched to any of our
// LAN interface addresses.
static int check_if_ip(char *ifname, void *ipv4)
{
    DEV_IP_CFG_t ipcfg;
    if(util_get_ipcfg(ifname, &ipcfg) != 0) {
        // can't get IP config for the interface, so no match
        return 0;
    }
    if(ipcfg.ipv4.i == *(unsigned int*)ipv4) {
        // interface IP matches
        return 1;
    }
    return 0;
}

// Try to handle the server's request to scan an ip address
// Returns: 0 if successful, error code otherwise
static int scan_device(PORT_SCAN_DEVICE_t *port_scan_device)
{
    int s, ret = -1;
    unsigned long scan_delay, wait_time, retries;

    // Set up socket for sending SYN pkts
    s = socket(AF_INET, SOCK_RAW , IPPROTO_TCP);
    if(s < 0) {
        log("%s: failed to create socket, error: %s\n",
            __func__, strerror(errno));
        return ret;
    }

    PORT_RANGE_MAP_t *to_scan = NULL;
    PORT_RANGE_MAP_t *from_scan = NULL;

    for(;;) {
        log("%s: scanning device\n", __func__);

        json_t *mac_obj = json_object_get(port_scan_device->device, "mac");
        if(!mac_obj) {
            log("%s: missing MAC address in scan device JSON\n", __func__);
            break;
        }
        size_t mac_len = json_string_length(mac_obj);
        const char *mac = json_string_value(mac_obj);
        if(mac_len <= 0 || !mac) {
            log("%s: invalid MAC string in scan device JSON\n", __func__);
            break;
        }

        json_t *ipv4_obj = json_object_get(port_scan_device->device, "ipv4");
        if(!ipv4_obj) {
            log("%s: missing IPv4 address in scan device JSON\n", __func__);
            break;
        }
        IPV4_ADDR_t ipv4;
        const char *ipv4_str = json_string_value(ipv4_obj);
        if(!ipv4_str || (ipv4.i = inet_addr(ipv4_str)) == INADDR_NONE) {
            log("%s: invalid IPv4 address in scan device JSON\n", __func__);
            break;
        }

        // Get optional scan time value (in sec)
        json_t *scan_delay_obj =
            json_object_get(port_scan_device->device, "scan_delay");
        if(!scan_delay_obj || !json_is_integer(scan_delay_obj))
        {
            scan_delay = PORT_SCANNER_DEFAULT_SCAN_DELAY;
        } else {
            scan_delay = json_integer_value(scan_delay_obj);
        }

        // Get optional scan results wait time value (in sec)
        json_t *wait_time_obj =
            json_object_get(port_scan_device->device, "wait_time");
        if(!wait_time_obj || !json_is_integer(wait_time_obj))
        {
            wait_time = PORT_SCANNER_DEFAULT_WAIT_TIME;
        } else {
            wait_time = json_integer_value(wait_time_obj);
        }

        // How many times to retry the scan (0 - scan only once)
        json_t *retries_obj =
            json_object_get(port_scan_device->device, "retries");
        if(!retries_obj || !json_is_integer(retries_obj))
        {
            retries = PORT_SCANNER_DEFAULT_SYN_RETRIES;
        } else {
            retries = json_integer_value(retries_obj);
        }

        json_t *ports_obj = json_object_get(port_scan_device->device, "ports");
        if(!ports_obj || (to_scan = util_json_to_port_range(ports_obj)) == NULL)
        {
            log("%s: unable to process ports un scan device JSON\n", __func__);
            break;
        }

        // We need to know source IP address the system will use for
        // calcualting the TCP header checksum, try to figure it out
        // by attempting connect on the socket we will use.
        struct sockaddr_in saddr;
        struct sockaddr_in daddr;
        memset(&daddr, 0, sizeof(daddr));
        socklen_t addrlen = sizeof(saddr);
        daddr.sin_family = AF_INET;
        daddr.sin_addr.s_addr = ipv4.i;
        if(connect(s, (struct sockaddr *)&daddr, sizeof(daddr)) != 0 ||
           getsockname(s, (struct sockaddr *)&saddr, &addrlen) != 0)
        {
            log("%s: unable to connect to %s\n",
                __func__, inet_ntoa(daddr.sin_addr));
            break;
        }
        log_dbg("%s: scanning from src IP: %s\n",
                __func__, inet_ntoa(saddr.sin_addr));
        // Make sure it is one of our LAN IPs
        if(util_enum_ifs(UTIL_IF_ENUM_RTR_LAN,
                         check_if_ip, &saddr.sin_addr) <= 0)
        {
            log("%s: no source IP %s is configured on LAN interfaces\n",
                __func__, inet_ntoa(saddr.sin_addr));
            break;
        }

        // Prepare the port range map the receiver will use
        from_scan = UTIL_PORT_RANGE_ALLOC(to_scan->start, to_scan->len);
        UTIL_PORT_RANGE_RESET(from_scan);

        // Pick random source port (if we inadvertently pick one that is in use)
        // it will mean a performance hit, but should not cause any major issue.
        unsigned short sport_h = 0x7fff + (rand() & 0x7fff);
        unsigned short sport = htons(sport_h);

        log_dbg("%s: scanning " IP_PRINTF_FMT_TPL " from port %d\n",
                __func__, IP_PRINTF_ARG_TPL(ipv4.b), sport_h);

        // Prepare the scan parameters structure
        scan_params.s = s;
        scan_params.ipv4_src.i = saddr.sin_addr.s_addr;
        scan_params.sport = sport;
        scan_params.ipv4_dst.i = ipv4.i;
        scan_params.to_scan = to_scan;
        scan_params.from_scan = from_scan;
        scan_params.seq = (rand() << 16) ^ rand();

        // Prepare and activate the receiving callback handling the
        // scan responses
        tcp_ack_pkt_proc.ip.a1.i = ipv4.i;
        tcp_ack_pkt_proc.tcpudp.p1 = sport_h;
        if(tpcap_add_proc_entry(&tcp_ack_pkt_proc) != 0)
        {
            log("%s: tpcap_add_proc_entry() failed for: %s\n",
                __func__, tcp_ack_pkt_proc.desc);
            break;
        }

        // Scan the ports
        probe_ports(scan_delay, wait_time, retries);

        // Stop the receiver
        tpcap_del_proc_entry(&tcp_ack_pkt_proc);
        memset(&scan_params, 0, sizeof(scan_params));

        // No longer need the to_scan range
        UTIL_PORT_RANGE_FREE(to_scan);
        to_scan = NULL;

        // Report back the collected results
        report_device_scan(port_scan_device->device, mac, from_scan);

        ret = 0;
        break;
    }

    // Remove packet capturing filter (no harm if not set)
    tpcap_del_proc_entry(&tcp_ack_pkt_proc);

    // Cleanup the scan params structure
    memset(&scan_params, 0, sizeof(scan_params));

    // Done w/ the socket
    if(s >= 0) {
        close(s);
        s = -1;
    }

    // Free the port ranges if still allocated
    if(to_scan != NULL) {
        UTIL_PORT_RANGE_FREE(to_scan);
        to_scan = NULL;
    }
    if(from_scan != NULL) {
        UTIL_PORT_RANGE_FREE(from_scan);
        from_scan = NULL;
    }

    return ret;
}

// Port scan thread entry point
static void scan_devices(THRD_PARAM_t *p)
{
    PORT_SCAN_DEVICE_t item, *p_item;

    for(;;)
    {
        // Pull item out from the scan queue, make a local copy and free
        // the cell it was using
        UTIL_MUTEX_TAKE(&g_port_scan_mutex);
        UTIL_Q_REM(&g_queue, p_item);
        if(p_item == NULL) {
            // Done with all the queued items, can terminate the thread
            UTIL_MUTEX_GIVE(&g_port_scan_mutex);
            break;
        }
        // Store the info in local data structure
        memcpy(&item, p_item, sizeof(item));
        // Free the cell
        p_item->devices = p_item->device = NULL;
        UTIL_MUTEX_GIVE(&g_port_scan_mutex);

        // Make the request
        scan_device(&item);

        // Decrement the reference count
        json_decref(item.devices);
    }

    g_port_scan_in_progress = FALSE;
}

// Add request to the fetch queue and increment root JSON refcount.
// Note: this function call should be protected by "port_scan_mutex" mutex!
// devices - root JSON object
// device - device entry
// Returns: 0 if ok, negative error code if fails, positive if duplicate
static int add_device(json_t *devices, json_t *device) {
    int ret;

    // If parameters are not valid back off immediately
    if(!devices || !device) {
        return -1;
    }

    for(;;)
    {
        // Pick a cell (start at random spot)
        int idx, ii;
        idx = rand() % PORT_SCAN_QUEUE_LIMIT;
        for(ii = 0; ii < PORT_SCAN_QUEUE_LIMIT; ii++) {
            if(!g_devices_a[idx].device) {
                break;
            }
            ++idx;
            idx %= PORT_SCAN_QUEUE_LIMIT;
        }
        if(g_devices_a[idx].device) {
            log("%s: error, no free cell\n", __func__);
            ret = -2;
            break;
        }

        // Start the port scan thread if it is not running, we are
        // under mutex, so it will wait until we populate the queue
        ret = 0;
        if(!g_port_scan_in_progress) {
            g_port_scan_in_progress = TRUE;
            if(util_start_thrd("port_scan", scan_devices, NULL, NULL) != 0) {
                ret = -3;
                break;
            }
        }

        // Set up the port scan structure and increment refcount for the request
        // NOTE: the refcount is incremented for each queued request here and
        //       decremented in the fetch thread for each request processed
        g_devices_a[idx].devices = json_incref(devices);
        g_devices_a[idx].device = device;

        // Add the item to the end of the linked list
        UTIL_Q_ADD(&g_queue, &(g_devices_a[idx]));

        ret = 0;
        break;
    }

    return ret;
}

// The "port_scan" command processor.
int cmd_port_scan(char *cmd, char *s, int s_len)
{
    json_t *devices = NULL;
    json_error_t jerr;
    int err = -1;
    int i;

    log("%s: processing Port Scan command\n", __func__);

    for(;;)
    {
        int device_count;

        if(!s || s_len <= 0) {
            log("%s: error, data: %s, len: %d\n", __func__, s, s_len);
            break;
        }

        // Process the incoming JSON. It should contain an array of ojects representing
        // the requested port scans. It is of this form:
        // [{ "mac": "c4:84:66:96:f5:3d", "ipv4": "192.168.1.11", "ports": ["80"] },
        //  { "mac": "24:f0:94:af:37:2c", "ipv4": "192.168.1.13", "ports": ["1-1024", "5353"] }]
        devices = json_loads(s, 0, &jerr);
        if(!devices) {
            log("%s: error at l:%d c:%d parsing device port scan list: '%s'\n",
                __func__, jerr.line, jerr.column, jerr.text);
            break;
        }

        device_count = json_array_size(devices);
        if(device_count <= 0) {
            log("%s: malformed or empty device array (count: %d)\n",
                    __func__, device_count);
            break;
        }

        // Loop through the array of devices. The whole loop is protected by
        // mutex since we want to queue all the items before processing them
        // (otherwise we might try to free a part of this json object while
        //  still in the loop)
        UTIL_MUTEX_TAKE(&g_port_scan_mutex);
        for(i = 0; i < device_count; i++) {
            json_t *device = json_array_get(devices, i);
            if(!device) {
                log("%s: error getting device\n", __func__);
                break;
            }

            if(add_device(devices, device) != 0) {
                log("%s: failed to enqueue device\n", __func__);
            }
        }
        UTIL_MUTEX_GIVE(&g_port_scan_mutex);

        err = 0;
        break;
    }

    if(devices) {
        json_decref(devices);
        devices = NULL;
    }

    return err;
}

#ifdef DEBUG
void test_port_scan(void)
{
    char mac1[20], mac2[20];
    char ip1[20], ip2[20];
    char jstr[512];

    printf("Please enter MAC for scan destination one:\n");
    scanf("%18s", mac1);
    printf("Please enter IP for scan destination one:\n");
    scanf("%16s", ip1);
    printf("Please enter MAC for scan destination two:\n");
    scanf("%18s", mac2);
    printf("Please enter IP for scan destination two:\n");
    scanf("%16s", ip2);
    
    sprintf(jstr, 
            "[{\"mac\":\"%s\",\"ipv4\":\"%s\","
            "\"ports\":[\"80\", \"20-22\",\"443\"],"
            "\"scan_delay\":200, \"wait_time\":1000},"
            "{\"mac\":\"%s\",\"ipv4\":\"%s\","
            "\"ports\":[\"1-65535\"],\"retries\":0}]",
            mac1, ip1, mac2, ip2);

    printf("Testing 'port_scan' command, JSON payload:\n%s\n", jstr);
    printf("Starting TPCAP...");
    if(tpcap_init(INIT_LEVEL_TPCAP) != 0) {
        printf("error\n");
        return;
    } else {
        printf("ok\n");
    }
    set_activate_event();
    sleep(5);
    printf("Executing command, press Ctrl+C to terminate...\n");
    int ret = cmd_port_scan("port_scan", jstr, strlen(jstr));
    printf("cmd_port_scan() returned %d\n", ret);

    sleep(10 * 60);
    printf("Press Ctrl-C to terminate.\n");
    return;
}
#endif // DEBUG
