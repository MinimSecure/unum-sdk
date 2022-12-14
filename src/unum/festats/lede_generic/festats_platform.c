// (c) 2018-2022 minim.co
// platform specific code for fe stats gathering

#include "unum.h"
#include <linux/netfilter.h> // Move this unum.h?

/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Debug macros for printing only in tests
#ifdef DEBUG
#  define DPRINTF(args...) ((get_test_num() == U_TEST_FESTATS) ? \
                            log_dbg(args) : 0)
#else // DEBUG
#  define DPRINTF(args...) /* Nothiing */
#endif // DEBUG

// This macro does n't consider endianness. Take care of it when
// you want to reuse this
// Well known multicast address
// The first octet should be 224 (0xe0000000)
// The third octet should be 0
#define UTIL_IPV4_IS_WELL_KNOWN_MCAST(a) \
	((((a) & 0xff000000) == 0xe0000000) && \
		(((a) & 0x0000ff00) == 0))

// This macro does n't consider endianness. Take care of it when
// you want to reuse this
// Locally scoped multicast address
// The first octet should be 239 (0xef000000)
#define UTIL_IPV4_IS_LOCAL_MCAST(a) \
	(((a) & 0xff000000) == 0xef000000)

// ToDo: Investigate a way to avoid this and use the one from opensource
// This is a duplicate of what we already have in tpcap.c
DEV_IP_CFG_t ipcfg;

// Function called by common code to start the IP forwarding
// engine stats collection pass.
void fe_platform_update_stats()
{
    char str[512];
    char *str_p = NULL;
    FILE *fp_stats;
    DEV_IP_CFG_t new_ipcfg;

    char *file = "/proc/net/nf_conntrack";
#ifdef DEBUG
    if(*test_conn_file != 0) {
        file = test_conn_file;
    }
#endif // DEBUG
    fp_stats = fopen(file, "r");
    if(!fp_stats) {
        log("%s: error opening %s: %s\n", __func__, file, strerror(errno));
        return;
    }

    memset(&new_ipcfg, 0, sizeof(new_ipcfg));
    // For now adding home network only
    // We may have to rework on this when ipcfg values are reused from tpcap
    if(util_get_ipcfg(PLATFORM_GET_MAIN_LAN_NET_DEV(), &new_ipcfg) != 0) {
        log("%s: warning, IP configuration update for %s has failed\n",
                    __func__, PLATFORM_GET_MAIN_LAN_NET_DEV());
    }
    else if(memcmp(&ipcfg, &new_ipcfg, sizeof(new_ipcfg))) {
        log("%s: updating IP configuration for %s\n", __func__, PLATFORM_GET_MAIN_LAN_NET_DEV());
        memcpy(&ipcfg, &new_ipcfg, sizeof(ipcfg));
    }

    while(fgets(str, sizeof(str), fp_stats) != NULL)
    {
        unsigned long long d_in, d_out;
        int rxb = 0, txb = 0;
        str_p = str;

        /*
         * --- Format of nf_conntrack (split into
         * multiple lines) ----
         * ipv4     2 tcp      6 208 ESTABLISHED src=10.10.11.176
         * dst=117.102.109.186 sport=50898 dport=5201 packets=10 bytes=768
         * src=117.102.109.186 dst=192.168.11.103 sport=5201 dport=50898
         * packets=8 bytes=498 [ASSURED] iqprio=0 swaccel=4 hwaccel=4 use=2
         */

        // Scan the fixed columns
        // Examples:
        // ipv4     2 tcp      6 431927 ...
        // ipv4     2 tcp      6 19 ...
        // ipv4     2 udp      17 28 ...
        // ipv4     2 unknown  2 593 ...
        unsigned int ip_proto, proto;
        if(sscanf(str_p, "%*s %u %*s %u", &proto, &ip_proto) != 2) {
            log_dbg("%s: Error scanning line: %.30s...\n", __func__, str_p);
            continue;
        }
        if(ip_proto != IPPROTO_ICMP && ip_proto != IPPROTO_UDP && ip_proto != IPPROTO_TCP) {
            // Ignore if not a ICMP, TCP or UDP entry
            continue;
        }
        if(proto != NFPROTO_IPV4
#ifdef FEATURE_IPV6_TELEMETRY
           && proto != NFPROTO_IPV6
#endif // FEATURE_IPV6_TELEMETRY
            ) {
            // Ignore if not ipv4 or ipv6
            continue;
        }
        // Start filling in the header
        FE_CONN_HDR_t hdr;
        hdr.proto = ip_proto;
        hdr.pad = 0;

        if (proto == NFPROTO_IPV4) {
            hdr.af = AF_INET;

            unsigned int ip[4];
            // This src search is for IPv4 Address
            char *src = strstr(str_p, "src=");
            if(src == NULL ||
               sscanf(src, "src=" IP_PRINTF_FMT_TPL,
                      &ip[0], &ip[1], &ip[2], &ip[3]) != 4)
            {
                log("%s: Error, ipv4 no \"src=<IP>\": %.30s...\n", __func__, str);
                continue;
            }
            IPV4_ADDR_t dev_ipv4 = {.b = {ip[0], ip[1], ip[2], ip[3]}};
            hdr.dev.ipv4 = dev_ipv4;
            str_p = src;

            // Get Destination IPv4 Address
            char *dst = strstr(str_p, "dst=");
            if(dst == NULL ||
               sscanf(dst, "dst=" IP_PRINTF_FMT_TPL,
                      &ip[0], &ip[1], &ip[2], &ip[3]) != 4)
            {
                log("%s: Error, ipv4 no \"dst=<IP>\": %.30s...\n", __func__, str);
                continue;
            }
            IPV4_ADDR_t peer_ipv4 = {.b = {ip[0], ip[1], ip[2], ip[3]}};
            hdr.peer.ipv4 = peer_ipv4;

#ifdef FEATURE_IPV6_TELEMETRY
        } else {
            // ipv6     10 udp      17 28 src=fc00:dead:beef:0000:0000:0000:0000:038e dst=fc00:dead:beef:0000:0000:0000:0000:0001 sport=49701 dport=53 packets=1 bytes=82
            // src=fc00:dead:beef:0000:0000:0000:0000:0001 dst=fc00:dead:beef:0000:0000:0000:0000:038e sport=53 dport=49701 packets=1 bytes=98 mark=0 zone=0 use=2
            hdr.af = AF_INET6;
            {
                const char match_str[] = "src=";
                char *match = strstr(str_p, match_str);
                if(match == NULL) {
                    log("%s: Error, ipv6 no \"src=<IP>\": %.40s...\n", __func__, str_p);
                    continue;
                }
                char *match_end = strchr(match, ' ');
                if(match_end == NULL) {
                    log("%s: Error, ipv6 no end \"src=<IP>\": %.40s...\n", __func__, str_p);
                    continue;
                }
                match += sizeof(match_str)-1;
                *match_end = '\0';
                if (inet_pton(AF_INET6,
                              match,
                              hdr.dev.ipv6.b) != 1) {
                    log("%s: Error, ipv6 can't decode src \"%s\"\n", __func__, match);
                    continue;
                }
                str_p = match_end+1;
            }
            {
                const char match_str[] = "dst=";
                char *match = strstr(str_p, match_str);
                if(match == NULL) {
                    log("%s: Error, ipv6 no \"dst=<IP>\": %s\n", __func__, str_p);
                    continue;
                }
                char *match_end = strchr(match, ' ');
                if(match_end == NULL) {
                    log("%s: Error, ipv6 no end \"dst=<IP>\": %s\n", __func__, str_p);
                    continue;
                }
                match += sizeof(match_str)-1;
                *match_end = '\0';
                if (inet_pton(AF_INET6,
                              match,
                              hdr.peer.ipv6.b) != 1) {
                    log("%s: Error, ipv6 can't decode dst \"%s\"\n", __func__, match);
                    continue;
                }
                str_p = match_end+1;
            }
#endif // FEATURE_IPV6_TELEMETRY
        }

        char *sport = NULL;
        // Get Source Port
        if (str_p && ip_proto != IPPROTO_ICMP) {
            sport = strstr(str_p, "sport=");
            if(!sport || (hdr.dev_port = atoi(sport + sizeof("sport=")-1)) == 0) {
                log("%s: Error, invalid port 1: %.30s...\n", __func__,
                    (sport ? sport : "not found"));
                continue;
            }
        } else {
            // ICMP packet
            hdr.dev_port = 0;
        }

        char *dport = NULL;
        // Get Destination Port
        if (ip_proto != IPPROTO_ICMP && sport != NULL) {
            dport = strstr(sport + 6, "dport=");
            if(!dport || (hdr.peer_port = atoi(dport + sizeof("dport=")-1)) == 0) {
                log("%s: Error, invalid port 2: %.30s...\n", __func__,
                    (dport ? dport : "not found"));
                continue;
            }
        } else {
            // ICMP packet
            hdr.peer_port = 0;
        }

        // Get Tx bytes
        char *bytes1 = strstr(str_p, "bytes=");
        if (bytes1 == NULL) {
            log("%s: Error, Tx bytes not found  %.30s \n", __func__, str_p);
            continue;
        }
        d_out = strtoull(bytes1 + 6, NULL, 10);

        // Get Rx bytes
        char *bytes2 = strstr(bytes1 + 6, "bytes=");
        if (bytes2 == NULL) {
            log("%s: Error, Rx bytes not found  %.30s \n", __func__, str_p);
            continue;
        }
        d_in = strtoull(bytes2 + 6, NULL, 10);

        // Preprocess the header (fills in the remaining info)
        if(fe_prep_conn_hdr(&hdr) < 0) {
            // Per my observation this log is printed in case of traffic
            // generated from or received by the Router
            // This seems to be creating a lot of noise
            // Comment it out for now. 
            // We have the same issue with 7020 too
            // Discuss with Denis at code review time
#if 0
            log("%s: skipping p:%d " IP_PRINTF_FMT_TPL ":%hu -> "
                    IP_PRINTF_FMT_TPL ":%hu\n", __func__,
                    hdr.proto, IP_PRINTF_ARG_TPL(hdr.dev_ipv4.b), hdr.dev_port,
                    IP_PRINTF_ARG_TPL(hdr.peer_ipv4.b), hdr.peer_port);
#endif
            continue;
        }
        FE_CONN_t *conn = fe_upd_conn_start(&hdr);
        if(!conn) {
            log("%s: cannot add p:%d " IP_PRINTF_FMT_TPL ":%hu -> "
                IP_PRINTF_FMT_TPL ":%hu\n", __func__,
                hdr.proto, IP_PRINTF_ARG_TPL(hdr.dev.ipv4.b), hdr.dev_port,
                IP_PRINTF_ARG_TPL(hdr.peer.ipv4.b), hdr.peer_port);
            continue;
        }

        if(conn->hdr.rev) { // Reverse, swap Tx/Rx
            unsigned long long d_temp = d_in;

            d_in = d_out;
            d_out = d_temp;
        }

        if(d_in != conn->in.bytes) {
            rxb = 1;
        }
        if(d_out != conn->out.bytes) {
            txb = 1;
        }

        if (hdr.af == AF_INET) {
            unsigned int dest_ip = hdr.peer.ipv4.i;
            if (conn->hdr.rev) {
                dest_ip = hdr.dev.ipv4.i;
            }

            if (UTIL_IPV4_IS_WELL_KNOWN_MCAST(dest_ip) ||
                UTIL_IPV4_IS_LOCAL_MCAST(dest_ip) ||
                (dest_ip & ipcfg.ipv4mask.i) ==
                (ipcfg.ipv4.i & ipcfg.ipv4mask.i)) {
                // A multicast packet or broadcast packet or packets to the router
                // Well known and local multicast only
                // ie 224.0.0.0 to 224.0.0.255 and
                // 239.0.0.0 to 239.255.255.255
                // Ignore this
                // Our router does n't forward multicast packets to Internet
                // We need to count packets only those coming from / going to
                // Internet.
                fe_upd_conn_end(conn, FALSE);
                continue;
            }
#ifdef FEATURE_IPV6_TELEMETRY
        } else if (hdr.af == AF_INET6) {
            if (IN6_IS_ADDR_MULTICAST(hdr.peer.ipv6.b)) {
                // Our router doesn't forward multicast packets to Internet
                // We need to count packets only those coming from / going to
                // Internet.
                fe_upd_conn_end(conn, FALSE);
                continue;
            }
#endif // FEATURE_IPV6_TELEMETRY
        }
        if (conn->in.bytes > d_in || conn->out.bytes > d_out) {
            // If connection restarted between our polls zero the counters,
            log("%s: skipping and resetting p:%d " IP_PRINTF_FMT_TPL ":%hu -> "
                    IP_PRINTF_FMT_TPL ":%hu\n", __func__,
                    hdr.proto, IP_PRINTF_ARG_TPL(hdr.dev.ipv4.b), hdr.dev_port,
                    IP_PRINTF_ARG_TPL(hdr.peer.ipv4.b), hdr.peer_port);
            conn->in.bytes = 0;
            conn->out.bytes = 0;
            conn->out.bytes_read = 0;
            conn->in.bytes_read = 0;
        }
        conn->in.bytes = d_in;   // Download
        conn->out.bytes = d_out; // Upload

        fe_upd_conn_end(conn, (txb != 0 || rxb != 0));
        // Discard our pointer to the entry
        conn = NULL;
    }
    
    fclose(fp_stats);
    fp_stats = NULL;
    return;
}
