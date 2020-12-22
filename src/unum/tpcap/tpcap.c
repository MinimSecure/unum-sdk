// (c) 2017-2019 minim.co
// unum packet capturing code

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Array of monitored interfaces
static TPCAP_IF_t tp_ifs[TPCAP_IF_MAX];

// Array of stats for monitored interfaces plus the WAN interface
static TPCAP_IF_STATS_t tp_if_stats[TPCAP_STAT_IF_MAX];

// Initialize interface stats data structure
// (stores timestamp and inital values of the counters)
// Parameters:
// ii - interface index in the stats table
// name - interface name
// Returns: 0 - if successful
static int tpcap_prep_if_stats(int ii, char *name)
{
    NET_DEV_STATS_t st;

    // Initialize the same interface entry in the stats table
    memset(&(tp_if_stats[ii]), 0, sizeof(TPCAP_IF_STATS_t));
    strncpy(tp_if_stats[ii].name, name, IFNAMSIZ);
    tp_if_stats[ii].name[IFNAMSIZ - 1] = 0;

    if(util_get_dev_stats(tp_if_stats[ii].name, &st) == 0) {
        tp_if_stats[ii].last_t = util_time(1000);
        tp_if_stats[ii].last_in = st.rx_bytes;
        tp_if_stats[ii].last_out = st.tx_bytes;
        tp_if_stats[ii].last_proc = 0;
        return 0;
    }

    return -1;
}

// Add interface to the tp_ifs array
// Note: for use in tpcap thread only
// Returns: 0 - if the interface is added, error code otherwise
int tpcap_add_if(char *ifname, void *data)
{
    int ii;
    IF_ENUM_CB_EXT_DATA_t *e_data = (IF_ENUM_CB_EXT_DATA_t *)data;
    int if_type = 0;

    // Note: old platforms do not report intrface type bitmap
    // For the new ones IF_ENUM_CB_ED_IFT_PRIMARY and IF_ENUM_CB_ED_IFT_GUEST
    // bits allow to identify primary and guest LAN intrfaces. 
    // See IF_ENUM_CB_EXT_DATA_t in util_net.h for more details.
    if((e_data->flags & IF_ENUM_CB_ED_FLAGS_IFT) != 0) {
        if_type = e_data->if_type;
    }

    for(ii = 0; ii < TPCAP_IF_MAX; ii++) {
        if(((tp_ifs[ii].flags & TPCAP_IF_VALID) != 0) &&
           (strcmp(tp_ifs[ii].name, ifname) == 0))
        {
            // Update interface type
            tp_ifs[ii].if_type = if_type;
            // The interface is already listed, but it might have been
            // removed and added again, so verify its ifindex is the same.
            // If ifindex is not the same consider it no longer valid.
            if(tp_ifs[ii].ifidx != if_nametoindex(ifname)) {
                tp_ifs[ii].flags &= ~TPCAP_IF_VALID;
                break;
            }
            // IP configuration might change, check it every time
            DEV_IP_CFG_t new_ipcfg;
            memset(&new_ipcfg, 0, sizeof(new_ipcfg));
            if(util_get_ipcfg(ifname, &new_ipcfg) != 0) {
                log("%s: warning, IP configuration update for %s has failed\n",
                    __func__, ifname);
            }
            else if(memcmp(&tp_ifs[ii].ipcfg, &new_ipcfg, sizeof(new_ipcfg))) {
                log("%s: updating IP configuration for %s\n", __func__, ifname);
                memcpy(&tp_ifs[ii].ipcfg, &new_ipcfg, sizeof(tp_ifs[ii].ipcfg));
            }
            return 0;
        }
    }
    for(ii = 0; ii < TPCAP_IF_MAX; ii++)
    {
        if((tp_ifs[ii].flags & TPCAP_IF_VALID) == 0) {
            break;
        }
    }
    if(ii >= TPCAP_IF_MAX) {
        log("%s: no free slots in the interface table\n", __func__);
        return -1;
    }

    // Populate the interface information
    memset(&(tp_ifs[ii]), 0, sizeof(TPCAP_IF_t));

    strncpy(tp_ifs[ii].name, ifname, IFNAMSIZ - 1);
    tp_ifs[ii].name[IFNAMSIZ - 1] = 0;
    tp_ifs[ii].ifidx = if_nametoindex(ifname);
    if(tp_ifs[ii].ifidx <= 0) {
        log("%s: if_nametoindex(%s) error, %s\n",
            __func__, ifname, strerror(errno));
        return -2;
    }
    if(util_get_mac(ifname, tp_ifs[ii].mac) != 0) {
        log("%s: error getting MAC for %s\n",
            __func__, ifname);
        return -3;
    }
    if(util_get_ipcfg(ifname, &(tp_ifs[ii].ipcfg)) != 0) {
        log("%s: error getting IP configuration for %s\n",
            __func__, ifname);
        return -4;
    }
    if(tpcap_prep_if_stats(ii, ifname) != 0) {
        log("%s: error getting initializing counters for %s\n",
            __func__, ifname);
        return -5;
    }
    tp_ifs[ii].flags |= TPCAP_IF_VALID;

    return 0;
}

// Free packet capturing resources associated with the interface
static void tpcap_cleanup_if(TPCAP_IF_t *tpif)
{
    if((tpif->flags & TPCAP_IF_FD_READY) == 0) {
        log("%s: resources for %s already freed\n", __func__, tpif->name);
        return;
    }

    if(munmap(tpif->ring, tpif->ring_len) != 0) {
        log("%s: error unmapping memory at %p, len %d\n",
            __func__, tpif->ring, tpif->ring_len);
    }
    tpif->ring = NULL;
    tpif->ring_len = 0;

    close(tpif->fd);
    tpif->fd = -1;

    tpif->flags &= ~(TPCAP_IF_FD_READY);
    return;
}

// Prepare interface for the packet capturing
static int tpcap_setup_if(TPCAP_IF_t *tpif)
{
    int val;
    int fd;
    unsigned char *ring;
    unsigned int ring_len;
    struct sockaddr_ll addr;
    struct tpacket_req req;

    if((tpif->flags & TPCAP_IF_FD_READY) != 0) {
        log("%s: interface %s already set up\n", __func__, tpif->name);
        log("%s: ring at %p, ring length %d\n", __func__,
            tpif->ring, tpif->ring_len);
        return 0;
    }

    fd = socket(PF_PACKET, SOCK_RAW, 0);
    if(fd < 0) {
        log("%s: socket error, %s\n", __func__, strerror(errno));
        return -1;
    }

    val = TPACKET_V2;
    if(setsockopt(fd, SOL_PACKET, PACKET_VERSION, &val, sizeof(val))) {
        log("%s: error setting PACKET_VERSION, %s\n",
            __func__, strerror(errno));
        close(fd);
        return -2;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sll_family = PF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = tpif->ifidx;
    if(bind(fd, (void *)&addr, sizeof(addr)) < 0) {
        log("%s: %s bind error %s\n", __func__, tpif->name, strerror(errno));
        close(fd);
        return -3;
    }

#ifdef FEATURE_AP_ONLY
    // For standalone AP firmware we want to see all packets passing through
    // the bridge, so put the interface into promiscuous mode
    struct packet_mreq mr;
    memset(&mr, 0, sizeof(mr));
    mr.mr_ifindex = tpif->ifidx;
    mr.mr_type = PACKET_MR_PROMISC;
    if(setsockopt(fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) < 0) {
        log("%s: error setting PACKET_MR_PROMISC for %s, %s\n",
            __func__, tpif->name, strerror(errno));
    }
#endif // FEATURE_AP_ONLY

    if(set_sockopt_hwtimestamp(fd, tpif->name) != 0) {
        log("%s: no hardware timestamps for %s, %s\n",
            __func__, tpif->name, strerror(errno));
    }

    memset(&req, 0, sizeof(req));
    req.tp_frame_size = TPCAP_SNAP_LEN;
    req.tp_frame_nr   = TPCAP_NUM_PACKETS;
    req.tp_block_size = getpagesize();
    req.tp_block_nr   = (TPCAP_SNAP_LEN * TPCAP_NUM_PACKETS) /
                        req.tp_block_size;
    if(setsockopt(fd, SOL_PACKET, PACKET_RX_RING,
                  (void*) &req, sizeof(req)))
    {
        log("%s: error setting PACKET_RX_RING, %s\n",
            __func__, strerror(errno));
        close(fd);
        return -4;
    }

    ring_len = req.tp_block_size * req.tp_block_nr;
    ring = mmap(0, ring_len,
                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(ring == MAP_FAILED) {
        log("%s: mmap error, %s\n", __func__, strerror(errno));
        close(fd);
        return -4;
    }

    tpif->fd = fd;
    tpif->ring = ring;
    tpif->ring_len = ring_len;
    tpif->flags |= TPCAP_IF_FD_READY;

    return 0;
}

// Return poll event string name or empty string if unknown
static char *poll_event_name(int event)
{
    if(event == POLLERR) {
        return "POLLERR";
    } else if(event == POLLHUP) {
        return "POLLHUP";
    } else if(event == POLLNVAL) {
        return "POLLNVAL";
    }
    return "";
}

// Query TPACKET capturing stats for all the tpif[] interfaces
// and record how many packets we were able to process.
// Record all the tpif[] interface counters.
// Rcord the interface counters for WAN interface.
// Returns: the number of stats entries populated
static int update_stats()
{
    int err;
    unsigned int ii, len, ifcount;
    TPCAP_IF_STATS_t *tpst = NULL;
    TPCAP_IF_t *tpif = NULL;
    unsigned long new_t;
    struct timespec ts;
#ifndef FEATURE_LAN_ONLY
    TPCAP_IF_t tpwan = { TPCAP_IF_VALID | TPCAP_IF_FD_READY };
    strncpy(tpwan.name, GET_MAIN_WAN_NET_DEV(), sizeof(tpwan.name) - 1);
#endif // !FEATURE_LAN_ONLY

    ifcount = 0;
    for(ii = 0; ii < TPCAP_STAT_IF_MAX; ii++)
    {
        NET_DEV_STATS_t st;
        struct tpacket_stats stats;

        if(ii < TPCAP_IF_MAX) {
            tpif = &(tp_ifs[ii]);
#ifndef FEATURE_LAN_ONLY
        } else if(ii == TPCAP_WAN_STATS_IDX) {
            // Silently ignore WAN in the AP mode
            if(IS_OPM(UNUM_OPM_AP)) {
                continue;
            }
            // In non AP (gateway) mode collect WAN stats
            tpif = &tpwan;
#endif // !FEATURE_LAN_ONLY
        } else {
            log("%s: invalid stats interface index %d\n", __func__, ii);
            continue;
        }
        tpst = &(tp_if_stats[ii]);

        if((tpif->flags & TPCAP_IF_FD_READY) == 0) {
            memset(tpst, 0, sizeof(TPCAP_IF_STATS_t));
            continue;
        }

        err = util_get_dev_stats(tpif->name, &st);
        if(err != 0) {
            log("%s: failed to get counters for %s\n", __func__, tpif->name);
            memset(tpst, 0, sizeof(TPCAP_IF_STATS_t));
            continue;
        }

        // Capture the time
        new_t = util_time(1000);
        clock_gettime(CLOCK_REALTIME, &ts);

        // If it is the first time we are getting the counters after
        // a failure or interface counters got reset, only capture the base
        // for diffs calculation later.
        if(tpst->last_t == 0 || // Note: time in msecs is ok to roll over
           tpst->last_in > st.rx_bytes || tpst->last_out > st.tx_bytes)
        {
            log("%s: resetting counters for %s\n", __func__, tpif->name);
            memset(tpst, 0, sizeof(TPCAP_IF_STATS_t));
            tpst->last_t = new_t;
            tpst->last_in = st.rx_bytes;
            tpst->last_out = st.tx_bytes;
            tpst->last_proc = tpif->proc_pkt_count;
            continue;
        }

        // Calculate the values
        tpst->rt = ts;
        tpst->len_t = new_t - tpst->last_t;
        tpst->last_t = new_t;
        tpst->bytes_in = (unsigned long)(st.rx_bytes - tpst->last_in);
        tpst->last_in = st.rx_bytes;
        tpst->bytes_out = (unsigned long)(st.tx_bytes - tpst->last_out);
        tpst->last_out = st.tx_bytes;

        // Done with the stats for WAN interface
        if(ii == TPCAP_WAN_STATS_IDX) {
            tpst->tp_packets = tpst->proc_pkts = tpst->tp_drops = 0;
            ++ifcount;
            continue;
        }
        tpst->proc_pkts =
            (unsigned long)(tpif->proc_pkt_count - tpst->last_proc);
        tpst->last_proc = tpif->proc_pkt_count;

        // Get the tpcap stats
        len = sizeof(stats);
        err = getsockopt(tpif->fd, SOL_PACKET, PACKET_STATISTICS, &stats, &len);
        if(err < 0) {
            log("%s: ignoring PACKET_STATISTICS ioctl error for %s: %s\n",
                __func__, tpif->name, strerror(errno));
            // If failed just assume nothing was dropped
            tpst->tp_drops = 0;
            tpst->tp_packets = tpst->proc_pkts;
        }
        tpst->tp_drops = stats.tp_drops;
        tpst->tp_packets = stats.tp_packets;
        ++ifcount;
    }

    return ifcount;
}

// Walk through captured packets in the ring calling processing function
// for each one and releasing to be reused by the kernel.
// Returns: the number of processed packets
static int process_packets(TPCAP_IF_t *tpif)
{
    int count;
    struct tpacket2_hdr *hdr;
    int idx = tpif->idx;

#ifdef DEBUG
    if(tpcap_test_param.int_val == TPCAP_TEST_BASIC)
    {
        printf("%llu: Starting proocessing packets, ring idx %d\n",
               util_time(1000), idx);
    }
#endif // DEBUG

    // Process up to max packets in the ring to avoid non-stop looping
    // if they are fed in faster than we can handle them.
    for(count = 0; count < TPCAP_NUM_PACKETS; count++)
    {
        hdr = (struct tpacket2_hdr*)(tpif->ring + (idx * TPCAP_SNAP_LEN));
        // Reached the packet slot that has no captured packet yet
        if(!(hdr->tp_status & TP_STATUS_USER)) {
            break;
        }

        struct ethhdr *eth = (struct ethhdr *)((char *)hdr + hdr->tp_mac);
        uint16_t eth_proto = ntohs(eth->h_proto);

        // Ignore incomplete and frames w/ low ethtype (only need Ethernet II)
        if(hdr->tp_snaplen >= sizeof(struct ethhdr) &&
           eth_proto >= TPCAP_ETHTYPE_MIN)
        {
            tpcap_process_packet(tpif, hdr, eth);
        }

        __sync_synchronize();
        hdr->tp_status = TP_STATUS_KERNEL;
        idx = (idx + 1) % TPCAP_NUM_PACKETS;
    }

    // Update packet index in the interface structure
    tpif->idx = idx;

    // Update processed packet counter
    tpif->proc_pkt_count += count;

#ifdef DEBUG
    if(tpcap_test_param.int_val == TPCAP_TEST_BASIC)
    {
        printf("%llu: Processed %d packets, new ring idx %d\n",
               util_time(1000), count, idx);
    }
#endif // DEBUG

    return count;
}

// Packets capture and processing for up to 'timeout'.
// timeout - how long to run, in milliseconds
// pfd - array of fd's to pass to poll function
// tpif_idx - mapping of pollfd array -> tp_ifs array
// ifcount - # of items in pollfd array
// Returns: updated interface count (changes if we operation on
//          any of our  interfaces start failing)
static int capture_and_process(unsigned int timeout,
                               struct pollfd *pfd,
                               int *tpif_idx,
                               int ifcount)
{
    int ii, jj, err;
    unsigned int t_in, t_remains, t_now;
    int ret = 0;

    for(t_now = t_in = util_time(1000), t_remains = timeout;
        t_remains > 0 && t_remains <= timeout;
        t_now = util_time(1000), t_remains = timeout - (t_now - t_in))
    {
        ret = poll(pfd, ifcount, t_remains);
        err = errno;
        // If poll has failed just sleep till the dicovery interval expires
        if(ret < 0) {
            log("%s: poll failed, %s\n", __func__, strerror(err));
            t_now = util_time(1000);
            if((t_now - t_in) < timeout) {
                t_remains = timeout - (t_now - t_in);
                util_msleep(t_remains);
            }
            break;
        }
        // Nothing has happened
        if(ret == 0) {
            break;
        }
        // We have some events...
        for(ii = ifcount - 1; ii >= 0; --ii) {
            jj = tpif_idx[ii];
            // We are only expecting POLLIN bit or the errors
            if(pfd[ii].revents == POLLIN) {
                // process captured packets
                process_packets(&(tp_ifs[jj]));
                pfd[ii].revents = 0;
                continue;
            }
            if(pfd[ii].revents == 0) {
                // nothing has happened on the interface
                continue;
            }
            log("%s: unexpected poll event %s(0x%x) for %s\n",
                __func__, poll_event_name(pfd[ii].revents),
                pfd[ii].revents, tp_ifs[jj].name);
            // Close the fd, shift remaining interfaces down and/or
            // if none left just sleep till the end of the interval
            tpcap_cleanup_if(&(tp_ifs[jj]));
            for(jj = ii; jj < TPCAP_IF_MAX - 1; jj++) {
                memcpy(&(pfd[jj]), &(pfd[jj + 1]), sizeof(struct pollfd));
                memset(&pfd[jj + 1], 0, sizeof(struct pollfd));
                tpif_idx[jj] = tpif_idx[jj + 1];
                tpif_idx[jj + 1] = 0;
            }
            --ifcount;
            log("%s: remainig interface count is %d\n", __func__, ifcount);
            if(ifcount <= 0) {
                break;
            }
        }
    }

    return ifcount;
}

// Discovers and prepares interfaces for capturing packets on.
// pfd - (OUT) array of fd's to pass to poll function
// tpif_idx - (OUT) mapping of pollfd array -> tp_ifs array
// Returns: interface count or negative if error
static int discover_and_prep_interfaces(struct pollfd *pfd,
                                        int *tpif_idx)
{
    int ii, ifcount;

    // Update the list of interfaces we have to monitor
    if(util_enum_ifs(UTIL_IF_ENUM_RTR_LAN |
                     UTIL_IF_ENUM_EXT_DATA, tpcap_add_if, NULL) != 0)
    {
        log("%s: error updating interface list, proceeding anyway\n",
            __func__);
    }

    // Set up the packet capturing sockets if necessary and
    // populate the pollfd structure
    ifcount = 0;
    for(ii = 0; ii < TPCAP_IF_MAX; ii++)
    {
        if((tp_ifs[ii].flags & TPCAP_IF_VALID) == 0) {
            continue;
        }
        if((tp_ifs[ii].flags & TPCAP_IF_FD_READY) == 0 &&
           tpcap_setup_if(&(tp_ifs[ii])) != 0)
        {
            log("%s: skipping interface %s\n", __func__, tp_ifs[ii].name);
            continue;
        }
        pfd[ifcount].fd = tp_ifs[ii].fd;
        pfd[ifcount].events = POLLIN;
        tpif_idx[ifcount] = ii;
        ++ifcount;
    }

    // Prepare WAN interface for collecting stats
#ifndef FEATURE_LAN_ONLY
    if(IS_OPM(UNUM_OPM_GW) &&
       tpcap_prep_if_stats(TPCAP_WAN_STATS_IDX, GET_MAIN_WAN_NET_DEV()) != 0)
    {
        log("%s: failed to prepare %s for collecting stats, continuing\n",
            __func__, GET_MAIN_WAN_NET_DEV());
    }
#endif // !FEATURE_LAN_ONLY

    return ifcount;
}

// Packet capturing thread entry point function
static void tpcap(THRD_PARAM_t *p)
{
    unsigned int iteration;
    struct pollfd pfd[TPCAP_IF_MAX];
    int tpif_idx[TPCAP_IF_MAX];

    log("%s: started\n", __func__);

    log("%s: waiting for activate to complete\n", __func__);
    wait_for_activate();
    log("%s: done waiting for activate\n", __func__);

    // Adjust the thread priority if any adjustment is requested
    if(unum_config.tpcap_nice >= -NZERO && unum_config.tpcap_nice < NZERO) {
        pid_t tid = syscall(SYS_gettid);
        if(setpriority(PRIO_PROCESS, tid, unum_config.tpcap_nice) != 0) {
            log("%s: setpriority() failed: %s\n", __func__, strerror(errno));
        }
    }

    // Packet capturing and processing loop
    for(iteration = 0; TRUE; iteration++)
    {
        int ifcount;

        // Run the intrface discovery and packet capturing sockets preparation
        if((iteration % TPCAP_IF_DISCOVERY_NUM_SLICES) == 0)
        {
            memset(pfd, 0, sizeof(pfd));
            memset(tpif_idx, 0, sizeof(tpif_idx));
            ifcount = discover_and_prep_interfaces(pfd, tpif_idx);
#ifdef DEBUG
            if(tpcap_test_param.int_val == TPCAP_TEST_BASIC)
            {
                printf("%llu: Rescan interfaces, ifcount: %d\n",
                       util_time(1000), ifcount);
            }
#endif // DEBUG
        }

#ifdef DEBUG
        if(tpcap_test_param.int_val == TPCAP_TEST_BASIC)
        {
            printf("%llu: Starting capturing, ifcount: %d\n",
                   util_time(1000), ifcount);
        }
#endif // DEBUG

        // If no interfaces to monitor sleep through the data collection time
        // slice, otherwise caprure and process packets.
        if(ifcount <= 0) {
            util_msleep(unum_config.tpcap_time_slice * 1000);
        } else {
            ifcount = capture_and_process(unum_config.tpcap_time_slice * 1000,
                                          pfd, tpif_idx, ifcount);
        }

#ifdef DEBUG
        if(tpcap_test_param.int_val == TPCAP_TEST_BASIC)
        {
            printf("%llu: Capturing cycle complete, ifcount: %d\n",
                   util_time(1000), ifcount);
        }
#endif // DEBUG

        // Collect capturing and interface usage statistics
        // Note: we do not use the stats in the AP mode although still collect
        //       them for the LAN interfaces (there's just no dev telemetry to
        //       report them in).
        update_stats();

        // Call capturing cycle complete handlers for the
        // users of the tpacket subsystem.
        tpcap_cycle_complete();

    }

    // Never reaches here
    log("%s: done\n", __func__);
    return;
}

// Get the pointer to the interface stats table
// Should only be used in the the tpcap thread
TPCAP_IF_STATS_t *tpcap_get_if_stats(void)
{
    return tp_if_stats;
}

// Get the pointer to the interface table
// Should only be used in the the tpcap thread
TPCAP_IF_t *tpcap_get_if_table(void)
{
    return tp_ifs;
}

// Subsystem init fuction
int tpcap_init(int level)
{
    int ret = 0;
    if(level == INIT_LEVEL_TPCAP) {
        // Start the packet capturing job
        ret = util_start_thrd("tpcap", tpcap, NULL, NULL);
    }
    return ret;
}

#ifdef DEBUG
// The packet capturing testing parameter(s)
THRD_PARAM_t tpcap_test_param;

int tpcap_test(int test_mode)
{
    tpcap_test_param.int_val = test_mode;
    set_activate_event();
    // Test tpcap DNS, connections or stats info collector
    // The main collector has to be initializd for all the
    // devices telemetry related tests, the rest depending
    // on the test might not be necessary.
    // The fingerpinting JSON generation
    if(test_mode == TPCAP_TEST_DNS     ||
       test_mode == TPCAP_TEST_IFSTATS ||
       test_mode == TPCAP_TEST_DT      ||
       test_mode == TPCAP_TEST_DT_JSON ||
       test_mode == TPCAP_TEST_FP_JSON)
    {
        // Initialize the device info collector
        if(dt_main_collector_init() != 0) {
            printf("%s: Error, dt_main_collector_init() has failed\n",
                   __func__);
            return -1;
        }
        // Initialize the DNS info collector
        if(dt_dns_collector_init() != 0) {
            printf("%s: Error, dt_dns_collector_init() has failed\n",
                   __func__);
            return -2;
        }
        // For the dev telemetry main table tests we now need to add
        // festats (IP fast forwarding engine stats) subsystem init.
        // It's only available for some subsystems.
        if(test_mode == TPCAP_TEST_DT_JSON || test_mode == TPCAP_TEST_DT) {
            // Set ARP and connection tracking files for the test
            __attribute__((weak)) void test_festats_set_src_files(void);
            if(test_festats_set_src_files != NULL) {
                test_festats_set_src_files();
            }
            if(fe_start_subsystem != NULL && fe_start_subsystem() != 0) {
                printf("%s: Error, fe_start_subsystem() has failed\n",
                       __func__);
                return -3;
            }
        }
        // Init fingerprinting info collection if testing it,
        // it can only be tested along with the device telemetry
        // since it is sent in its JSON.
        if(test_mode == TPCAP_TEST_FP_JSON)
        {
            // Init timers subsystem (need them for SSDP)
            if(util_timers_init() != 0) {
                printf("%s: Error, util_timers_init() has failed\n",
                       __func__);
                return -4;
            }
            // Initialize the device info collector
            if(fp_init(INIT_LEVEL_FINGERPRINT) != 0) {
                printf("%s: Error, fp_main_collector_init() has failed\n",
                       __func__);
                return -5;
            }
        }
    }
    tpcap(&tpcap_test_param);
    return 0;
}
#endif // DEBUG
