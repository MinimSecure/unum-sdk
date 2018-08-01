// Copyright 2018 Minim Inc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// unum helper net utils code

#include "unum.h"


// Checks if the interface is administratively UP
// ifname - the interface name
// Returns: TRUE - interface is up, FALSE - down or error
int util_net_dev_is_up(char *ifname)
{
    int ret, sockfd;
    struct ifreq ifr;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return FALSE;
    }

    for(;;) {
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        ifr.ifr_name[IFNAMSIZ - 1] = 0;
        if(ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
            ret = FALSE;
            break;
        }
        ret = ((ifr.ifr_flags & IFF_UP) != 0);
        break;
    }

    close(sockfd);

    return ret;
}

// Calculates the checksum to be used with TCP & IP packets
unsigned short util_ip_cksum(unsigned short *addr, int len)
{
    register int sum = 0;
    u_short answer = 0;
    register u_short *w = addr;
    register int nleft = len;

    // Our algorithm is simple, using a 32 bit accumulator (sum), we add
    // sequential 16 bit words to it, and at the end, fold back all the
    // carry bits from the top 16 bits into the lower 16 bits.
    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    // mop up an odd byte, if necessary
    if (nleft == 1) {
        *(u_char *) (&answer) = *(u_char *) w;
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16); // add carry
    answer = ~sum; // truncate to 16 bits

    return answer;
}

// Get device base MAC (format xx:xx:xx:xx:xx:xx, has to be static string).
// Until the MAC is successfully read from the file the function
// might be returning NULL;
char *util_device_mac()
{
#ifdef BASE_MAC_FILE
    static char mac_str_buf[MAC_ADDRSTRLEN];
    static char *mac_str = NULL;

    if(!mac_str) {
        FILE *f;
        f = fopen(BASE_MAC_FILE, "rb");
        if(f) {
            if(fgets(mac_str_buf, sizeof(mac_str_buf), f) == NULL) {
                log("%s: error (%d) reading MAC address from %s\n",
                    __func__, errno, BASE_MAC_FILE);
            } else {
                str_tolower(mac_str_buf);
                // Some devices (Linksys wrt1900acs) might try to save on
                // MAC address blocks and assert the "local" bit to form unique
                // LAN MAC address, resetting that and mcast bit to 0
                int val = mac_str_buf[1];
                val = ((val <= '9') ? (val - '0') : ((val - 'a') + 10)) & 0x0c;
                mac_str_buf[1] = UTIL_MAKE_HEX_DIGIT(val);
                mac_str = mac_str_buf;
                log("%s: device base MAC: %s\n", __func__, mac_str_buf);
            }
            fclose(f);
        }
    }

    return mac_str;
#else  // BASE_MAC_FILE
    return platform_device_mac();
#endif // BASE_MAC_FILE
}

// Get the IPv4 address (as a string) of a network device.
// The buf length should be at least INET_ADDRSTRLEN bytes.
// Returns 0 if successful, error code if fails.
int util_get_ipv4(char *dev, char *buf)
{
    struct ifreq ifr;
    int fd = -1;
    int ret = -1;

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, dev, IFNAMSIZ-1);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    for(;;) {
        if(fd < 0 || ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
            ret = (errno != 0 ? errno : -1);
            break;
        }
        snprintf(buf, INET_ADDRSTRLEN, "%s",
                 inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
        ret = 0;
        break;
    }
    if(fd >= 0) {
        close(fd);
    }

    return ret;
}

// Get the MAC address (binary) of a network device.
// The mac buffer length should be at least 6 bytes.
// If mac is NULL just executes ioctl and reports success or error.
// Returns 0 if successful, error code if fails.
int util_get_mac(char *dev, unsigned char *mac)
{
    struct ifreq ifr;
    int fd = -1;
    int ret = -1;

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, dev, IFNAMSIZ-1);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    for(;;) {
        if(fd < 0 || ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
            ret = (errno != 0 ? errno : -1);
            break;
        }
        if(mac) {
            memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
        }
        ret = 0;
        break;
    }
    if(fd >= 0) {
        close(fd);
    }

    return ret;
}

// Using getaddrinfo we do a name lookup and then
// *res will be set to the first ip4 sockaddr (or ignored if NULL)
// return negative number on error
int util_get_ip4_addr(char *name, struct sockaddr *res)
{
  struct addrinfo *cur_info;
  struct addrinfo *info_list;
  int ret = -1;

  // resolve the domain name into a list of addresses
  if(getaddrinfo(name, NULL, NULL, &info_list) == 0) {

    // Loop through the linked list and find a ip4 address
    for(cur_info = info_list; cur_info != NULL; cur_info = cur_info->ai_next) {

      // If we found our ip4 address... copy our data to our result
      if(cur_info->ai_family == AF_INET && cur_info->ai_addr &&
         cur_info->ai_addr->sa_family == AF_INET             &&
         cur_info->ai_addrlen <= sizeof(struct sockaddr))
      {
        if(res != NULL) {
          memset(res, 0, sizeof(struct sockaddr));
          memcpy(res, cur_info->ai_addr, cur_info->ai_addrlen);
        }
        ret = 0;
        break;
      }
    }

    // free our linked list
    freeaddrinfo(info_list);
  }

  return ret;
}

// Sends out an ICMP packet to a host and waits for a reply
// If the timeout is set to 0, we do not wait for the reply
// Returns the time it took in milliseconds, or -1 if error
//
// Bug: Currently does not work with ipv6
int util_ping(struct sockaddr *s_addr, int timeout_in_sec)
{
    int ret = 0;
    int sockfd;
    int siz;
    int addr_size;
    char packet[56 + 60 + 76];
    char buffer[1024];
    char sender_str[20];
    char dst_addr[INET6_ADDRSTRLEN];
    unsigned long long start;
    struct timeval timeout;
    struct icmp *icmp_pkt;
    struct icmp *icmp_recv;
    struct iphdr *ip_reply;
    struct sockaddr_in connection;
    struct sockaddr_storage sender;

    socklen_t sendsize = sizeof(sender);
    bzero(&sender, sizeof(sender));

    if(s_addr->sa_family == AF_INET) {
        addr_size = INET_ADDRSTRLEN;
    } else {
        addr_size = INET6_ADDRSTRLEN;
    }

    inet_ntop(s_addr->sa_family,
              &((struct sockaddr_in *)s_addr)->sin_addr,
              dst_addr, addr_size);
    log_dbg("%s: Destination address: %s\n", __func__, dst_addr);

    // Set up our connection to use the dst_addr
    connection.sin_family = s_addr->sa_family;
    connection.sin_addr.s_addr = inet_addr(dst_addr);

    // Attempt to crate our socket
    if((sockfd = socket(s_addr->sa_family, SOCK_RAW, 1)) < 0) {
        log("%s: We hit a problem setting up the socket - %s\n",
            __func__, strerror(errno));
        return -1;
    }

    // Create our ICMP Packet and populate our packet with
    // the data
    icmp_pkt = (struct icmp *)packet;
    memset(icmp_pkt, 0, sizeof(*icmp_pkt));
    icmp_pkt->icmp_type = ICMP_ECHO;
    icmp_pkt->icmp_cksum = util_ip_cksum((unsigned short *)icmp_pkt,
                                         56 + ICMP_MINLEN);

    // Mark our start time and transmit the packet
    start = util_time(1000);
    sendto(sockfd, &packet, 56 + ICMP_MINLEN, 0,
           (struct sockaddr *)&connection, sizeof(struct sockaddr));

    // If our timeout is set to 0... we dont wait for the response
    if(timeout_in_sec != 0)
    {
        // Recieve loop
        while(TRUE)
        {
            // Set a timeout for the socket. If time has passed we
            // should recalculate our timeout to reflect that
            unsigned long msces = (timeout_in_sec * 1000 -
                                   (util_time(1000) - start));
            timeout.tv_sec =  msces / 1000;
            timeout.tv_usec = (msces % 1000) * 1000;
            if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
                          (char *)&timeout, sizeof(timeout)) < 0)
            {
                log("%s: setsockopt failed: %s\n", __func__, strerror(errno));
                ret = -1;
                break;
            }

            // Wait for our response packet
            siz = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                           (struct sockaddr*)&sender, &sendsize);

            // We hit some sort of error...
            if(siz < 0) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    log_dbg("%s: ping timed out\n", __func__);
                } else {
                    log("%s: ping recv() error: %s\n",
                        __func__, strerror(errno));
                }
                ret = -1;
                break;
            }

            // Structure our buffer
            ip_reply = (struct iphdr*)buffer;
            icmp_recv = (struct icmp *) (buffer + (ip_reply->ihl << 2));

            // Make sure this is an ICMP Reply from our source
            if(icmp_recv->icmp_type == ICMP_ECHOREPLY &&
               connection.sin_addr.s_addr ==
                   ((struct sockaddr_in*)&sender)->sin_addr.s_addr)
            {
                inet_ntop(sender.ss_family,
                          &(((struct sockaddr_in*)&sender)->sin_addr),
                          sender_str, sizeof(sender_str));
                ret = util_time(1000) - start;

                log_dbg("%s: From - %s\n", __func__, sender_str);
                log_dbg("%s: ID: %d\n", __func__, ntohs(ip_reply->id));
                log_dbg("%s: TTL: %d\n", __func__, ip_reply->ttl);
                log_dbg("%s: Time: %lums\n", __func__, ret);

                break;
            }
        }
    }

    close(sockfd);
    return ret;
}

// Get the IP configuration of a network device.
// Returns 0 if successful, error code if fails.
int util_get_ipcfg(char *dev, DEV_IP_CFG_t *ipcfg)
{
    struct ifreq ifr;
    int fd = -1;
    int ret = -1;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    for(;;)
    {
        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_addr.sa_family = AF_INET;
        strncpy(ifr.ifr_name, dev, IFNAMSIZ-1);

        if(fd < 0 || ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
            ret = (errno != 0 ? errno : -1);
            break;
        }
        memcpy(&(ipcfg->ipv4),
               &(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),
               sizeof(ipcfg->ipv4));

        if(ioctl(fd, SIOCGIFNETMASK, &ifr) < 0) {
            ret = (errno != 0 ? errno : -2);
            break;
        }
        memcpy(&(ipcfg->ipv4mask),
               &(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),
               sizeof(ipcfg->ipv4));

        ret = 0;
        break;
    }
    if(fd >= 0) {
        close(fd);
    }

    return ret;
}

// Get network device statistics/counters.
// Returns 0 if successful, error code if fails.
int util_get_dev_stats(char *dev, NET_DEV_STATS_t *st)
{
    int ret;
    FILE *f;
    char buf[256];

    memset(st, 0, sizeof(NET_DEV_STATS_t));
    f = fopen("/proc/net/dev", "r");
    if (!f) {
        return -1;
    }
    fgets(buf, sizeof(buf), f);
    fgets(buf, sizeof(buf), f);

    ret = -3;
    while(fgets(buf, sizeof(buf), f)) {
        char *name, *data;
        int count;

        for(name = buf; *name == ' '; name++);
        for(data = name; *data != 0 && *data != ':'; data++);
        if(*data != ':') {
            // Not a valid device counters line
            continue;
        }
        *data++ = 0;
        if(strncmp(name, dev, IFNAMSIZ) != 0) {
            // Not the device we are looking for
            continue;
        }
        count = sscanf(data, UTIL_NET_DEV_CNTRS_FORMAT,
                       &st->rx_bytes,
                       &st->rx_packets,
                       &st->rx_errors,
                       &st->rx_dropped,
                       &st->rx_fifo_errors,
                       &st->rx_frame_errors,
                       &st->rx_compressed,
                       &st->rx_multicast,
                       &st->tx_bytes,
                       &st->tx_packets,
                       &st->tx_errors,
                       &st->tx_dropped,
                       &st->tx_fifo_errors,
                       &st->collisions,
                       &st->tx_carrier_errors,
                       &st->tx_compressed);
        if(count != UTIL_NET_DEV_CNTRS_FORMAT_COUNT) {
            ret = -2;
            break;
        }
        ret = 0;
        break;
    }
    fclose(f);
    return ret;
}

// Extract name from the DNS packet data (can handle references)
// Parameters:
// pkt - the pointer to the DNS packet (DNS header)
// ptr - the encoded name data pointer in the packet
// max - max bytes to look at past the ptr
// name <- buffer to write the name into, if NULL the data is not copied,
//         but the pkt is parsed and the return value is generated
// name_len - name buffer length (0 if name is NULL)
// level - recursion level (initial call with 0)
// to_lower - lower case the returned name
// Returns: the # of bytes of the encoded name in the high 2 bytes
//          the # of bytes (including terminating 0) written to 'name' in
//          the lower 2 bytes, negative if error.
int extract_dns_name(void *pkt, unsigned char *ptr, int max,
                     char *name, int name_len, int level, int to_lower)
{
    int ret = -1;
    int len = 0;
    unsigned char *cp = ptr;
    int remains = max;
    int ncount = 0;

    // Do not let it go crazy
    if(level > 3) {
        return -4;
    }

    while(remains > 0 && (!name || ncount <= name_len)) {
        int val = *cp;
        // First 2 bits indicate a reference (not in-place data) if set
        if((val & 0xC0) == 0xC0) {
            if(remains < 2) {
                break;
            }
            unsigned char *ncp = pkt + (((val & 0x3f) << 8) | cp[1]);
            ret = extract_dns_name(pkt, ncp, (ptr + max) - ncp,
                                   name + ncount, name_len - ncount, level + 1,
                                   to_lower);
            if(ret < 0) {
                break;
            }
            ncount += (ret & 0xffff);
            len += 2;
            remains -= 2;
            cp += 2;
            // reference terminates the name
            ret = (len << 16) | ncount;
            break;
        }
        // In-place encoded name
        if(val > 63) {
            // invalid segment length
            break;
        }
        // If the next byte is zero we are done
        if(val == 0) {
            ++len;
            ret = (len << 16) | ncount;
            break;
        }
        // No more space for the name extraction
        if(name && val + ncount >= name_len) {
            ret = -2;
            break;
        }
        // The segment runs beyond the end of the data we captured
        if(remains < val + 1) {
            ret = -3;
            break;
        }
        if(name) {
            memcpy(name + ncount, cp + 1, val);
            *(name + ncount + val) = '.';
        }
        ++val;
        ncount += val;
        len += val;
        cp += val;
        remains -= val;
    }

    // Relace the last dot with the terminating 0
    // and convert the name to lowercase
    if(ret > 0 && name && ncount > 0) {
        *(name + ncount - 1) = 0;
        if(to_lower) {
            str_tolower(name);
        }
    }
    return ret;
}

// Send UDP packet
int send_udp_packet(char *ifname, UDP_PAYLOAD_t *payload)
{
    // Create UDP socket
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(s < 0) {
        log("%s: error creating UDP socket: %s\n",
            __func__, strerror(errno));
        return -1;
    }

    // Get the interface IP config
    DEV_IP_CFG_t ipcfg;
    memset(&ipcfg, 0, sizeof(ipcfg));
    if(util_get_ipcfg(ifname, &ipcfg) != 0) {
        log("%s: failed to get IPv4 addr for <%s>, using INADDR_ANY\n",
            __func__, ifname);
        ipcfg.ipv4.i = htonl(INADDR_ANY);
    }

    // Allow to reuse address on bind
    int enable = 1;
    if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        log("%s: warning, SO_REUSEADDR failed on <%s>: %s\n",
            __func__, ifname, strerror(errno));
    }

    // Bind to specific source addrss/port
    struct sockaddr_in srcaddr;
    memset(&srcaddr, 0, sizeof(srcaddr));
    srcaddr.sin_family = AF_INET;
    srcaddr.sin_addr.s_addr = ipcfg.ipv4.i;
    srcaddr.sin_port = htons(payload->sport);
    if(bind(s, (void *)&srcaddr, sizeof(srcaddr)) < 0) {
        close(s);
        log("%s: bind() to IP/port failed <%s>: %s\n",
            __func__, ifname, strerror(errno));
        return -2;
    }

    // Bind the socket to the interface
#ifdef SO_BINDTODEVICE
    if(setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE,
                  ifname, strlen(ifname) + 1) != 0)
    {
        close(s);
        log("%s: SO_BINDTODEVICE failed <%s>: %s\n",
            __func__, ifname, strerror(errno));
        return -3;
    }
#else  // SO_BINDTODEVICE
    struct sockaddr_ll saddr = {};
    saddr.sll_family = PF_PACKET;
    saddr.sll_protocol = htons(ETH_P_ALL);
    saddr.sll_ifindex = if_nametoindex(ifname);
    if(saddr.sll_ifindex <= 0 || bind(s, (void *) &saddr, sizeof(saddr)) < 0)
    {
        log("%s: bind to <%s> error: %s\n",
            __func__, ifname, strerror(errno));
        close(s);
        return -3;
    }
#endif // SO_BINDTODEVICE

    // Enable broadcast
    int broadcast = 1;
    if(setsockopt(s, SOL_SOCKET,SO_BROADCAST,
                  &broadcast, sizeof(broadcast)) != 0)
    {
        log("%s: error enabling broadcasts: %s\n",
            __func__, strerror(errno));
        close(s);
        return -4;
    }

    // Set up the packet destination address and port
    struct sockaddr_in daddr;
    memset(&daddr, 0, sizeof(daddr));
    daddr.sin_family = AF_INET;
    inet_pton(AF_INET, payload->dip, &daddr.sin_addr);
    daddr.sin_port = htons(payload->dport);

    // Send the UDP packet
    int ret = sendto(s, payload->data, payload->len, 0,
                     (struct sockaddr*)&daddr, sizeof(daddr));
    if(ret < 0) {
        log("%s: error sending the UDP packet: %s\n",
            __func__, strerror(errno));
        close(s);
        return -5;
    }

    close(s);
    return 0;
}

// Set up a netlink socket
// s - address to NL_SOCK_t where to store the socket info
// type - netlink socket type
// Returns: 0 - success, negative - error
static int nl_sock(NL_SOCK_t *s, int type)
{
    struct sockaddr_nl nladdr;
    int err = 0;
    int fd = -1;

    for(;;) {
        if(!s) {
            err = EINVAL;
            break;
        }

        fd = socket(PF_NETLINK, SOCK_RAW, type);
        if(fd < 0) {
            err = errno;
            break;
        }

        memset(&nladdr, 0, sizeof(nladdr));
        nladdr.nl_family = AF_NETLINK;
        if(bind(fd, (struct sockaddr *)&nladdr, sizeof(nladdr)) < 0)
        {
            err = errno;
            break;
        }

        // get the ID the kernel assigned for this netlink connection
        memset(&nladdr, 0, sizeof(nladdr));
        socklen_t addr_len = sizeof(nladdr);
        if(getsockname(fd, (struct sockaddr *)&nladdr, &addr_len) < 0)
        {
            err = errno;
            break;
        }

        break;
    }

    // Cleanup if there was an error
    if(err != 0) {
        if(fd >= 0) {
            close(fd);
        }
        errno = err;
        return -1;
    }

    s->s = fd;
    s->pid = nladdr.nl_pid;
    return 0;
}

// Receive netlink response message w/ given sequence number
// s - pointer to NL_SOCK_t from nl_sock() call
// buf - where to store the data
// len - length of the data buffer
// seq_num - sequence # of the message to receive
// tout - call timeout in seconds, 0 no timeout
static int nl_recv(NL_SOCK_t *s, char *buf, unsigned int len,
                   int seq_num, int tout)
{
    struct nlmsghdr *nlhdr;
    int r_len = 0, msg_len = 0;
    long diff_t = tout * 1000;
    unsigned long long end_t = util_time(1000) + diff_t;

    for(;; diff_t = end_t - util_time(1000))
    {
        // If we run out of time...
        if(tout != 0 && diff_t <= 0) {
            log("%s: error, timed out\n", __func__);
            return -1;
        }

        // Set the remaining operation timeout
        struct timeval tv;
        tv.tv_sec = diff_t / 1000;
        tv.tv_usec = (diff_t % 1000) * 1000;
        if(setsockopt(s->s, SOL_SOCKET, SO_RCVTIMEO,
                      (const char*)&tv, sizeof(tv)) < 0)
        {
            log("%s: SO_RCVTIMEO error: %s\n", __func__, strerror(errno));
            return -2;
        }

        // Read response from the kernel
        if((r_len = recv(s->s, buf + msg_len, len - msg_len, 0)) < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                // retry
                continue;
            }
            log("%s: recv() error: %s\n", __func__, strerror(errno));
            return -3;
        }

        nlhdr = (struct nlmsghdr *)(buf + msg_len);

        // Check the header
        if(!NLMSG_OK(nlhdr, r_len))
        {
            log("%s: bad message from netlink\n", __func__);
            return -4;
        }

        // If not our message
        if((nlhdr->nlmsg_seq != seq_num) || (nlhdr->nlmsg_pid != s->pid)) {
            continue;
        }

        // Check the message
        if(nlhdr->nlmsg_type == NLMSG_ERROR)
        {
            struct nlmsgerr *nl_err = (struct nlmsgerr*)NLMSG_DATA(nlhdr);
    		errno = -nl_err->error;
            log("%s: netlink error: %s\n", __func__, strerror(errno));
            return -5;
        }

        // Check if the last message
        if(nlhdr->nlmsg_type == NLMSG_DONE)
        {
            break;
        }

        // Update message length
        msg_len += r_len;

        // Check if a multi part message
        if((nlhdr->nlmsg_flags & NLM_F_MULTI) == 0)
        {
            // not multipart, done
            break;
        }

        // If we run out of the buffer, but still not done
        if(msg_len >= len)
        {
            log("%s: %d bytes buffer is too short\n", __func__, len);
            return -6;
        }
    }

    return msg_len;
}

// Get default gateway IPv4 address
// Returns: 0 - if gw_ip is populated w/ the gateway IP address,
//          negative error code otherwise
int util_get_ipv4_gw(IPV4_ADDR_t *gw_ip)
{
    IPV4_ADDR_t gw;
    NL_SOCK_t s = { .s = -1 };
    struct nlmsghdr *nlmsg;
    int len, msg_seq = 0;
    int ret = -1;

    int buf_len = 8192;
    char *buf = UTIL_MALLOC(buf_len);

    for(;;)
    {
        // Check and init the buffer
        if(!buf) {
            log("%s: out of memory\n", __func__);
            ret = -2;
            break;
        }
        memset(buf, 0, buf_len);

        // Create socket
        if(nl_sock(&s, NETLINK_ROUTE) < 0)
        {
            log("%s: socket() error: %s\n", __func__, strerror(errno));
            ret = -3;
            break;
        }

        // point the header and the msg structure pointers into the buffer
        nlmsg = (struct nlmsghdr *)buf;

        // set up nlmsg header for getting routes
        nlmsg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
        nlmsg->nlmsg_type = RTM_GETROUTE;
        nlmsg->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
        nlmsg->nlmsg_seq = ++msg_seq; // sequence of the message packet
        nlmsg->nlmsg_pid = s.pid; // our netlink connection address

        // send the request
        if(send(s.s, nlmsg, nlmsg->nlmsg_len, 0) < 0)
        {
            log("%s: socket() error: %s\n", __func__, strerror(errno));
            ret = -4;
            break;
        }

        // Read the response, allow 10sec till timeout
        len = nl_recv(&s, buf, buf_len, msg_seq, 10);
        if(len < 0)
        {
            // nl_recv() logs the error message
            ret = -5;
            break;
        }

        // parse the response
        for(; NLMSG_OK(nlmsg, len); nlmsg = NLMSG_NEXT(nlmsg, len))
        {
            struct rtmsg *rtmsg;
            struct rtattr *rtattr;
            int rt_len;

            rtmsg = (struct rtmsg *)NLMSG_DATA(nlmsg);

            // only consider AF_INET routes in the main routing table
            // that have 0-length (default gw) mask
            if((rtmsg->rtm_family != AF_INET) ||
               (rtmsg->rtm_table != RT_TABLE_MAIN) ||
               (rtmsg->rtm_dst_len != 0))
            {
                continue;
            }

            rtattr = (struct rtattr *)RTM_RTA(rtmsg);
            rt_len = RTM_PAYLOAD(nlmsg);
            gw.i = 0;

            // walk through the attributes looking for RTA_GATEWAY
            for(; RTA_OK(rtattr, rt_len); rtattr = RTA_NEXT(rtattr, rt_len))
            {
                if(rtattr->rta_type == RTA_GATEWAY) {
                    memcpy(&gw.b, RTA_DATA(rtattr), sizeof(gw.b));
                    break;
                }
            }

            if(gw.i != 0) {
                gw_ip->i = gw.i;
                ret = 0;
                break;
            }
        }

        break;
    }

    // Cleanup
    if(buf) {
        UTIL_FREE(buf);
        buf = NULL;
    }
    if(s.s >= 0) {
        close(s.s);
    }

    return ret;
}

// Structure to exchange data between util_find_if_by_ip() and
// its find_if_ip() callback
typedef struct _FIND_IF_IP {
    IPV4_ADDR_t *ip; // Ptr to IP we are looking up intrface for
    char *ifname;    // Interface name buffer
} FIND_IF_IP_t;

// Target IP check callback for util_find_if_by_ip(). It returns
// non-0 value if device IP belong to one of the LAN interface subnets.
// The util_platform_enum_ifs() then returns the number of times a non-0
// value was reported by the callback.
static int find_if_ip(char *ifname, void *ptr)
{
    FIND_IF_IP_t *pd = (FIND_IF_IP_t *)ptr;
    DEV_IP_CFG_t ipcfg;

    if(util_get_ipcfg(ifname, &ipcfg) != 0) {
        // can't get IP config for the interface, so no match
        return 0;
    }

    if((ipcfg.ipv4.i & ipcfg.ipv4mask.i) == (pd->ip->i & ipcfg.ipv4mask.i)) {
        // The interface subnet matches, no dealing w/ the same subnet on
        // multiple LAN interfaces, will treat it as an error.
        strncpy(pd->ifname, ifname, IFNAMSIZ);
        pd->ifname[IFNAMSIZ - 1] = 0;
        return 1;
    }

    return 0;
}

// Find a monitored LAN interface for sending traffic to the specified IP
// address. Parameters:
// ip - pointer to IP address to lookup a match for
// ifn_buf - at least IFNAMSIZ long buffer to return the interface name in
// Returns: 0 - ok, negative - none found, positive - more than 1 found
int util_find_if_by_ip(IPV4_ADDR_t *ip, char *ifn_buf)
{
    int ret;
    FIND_IF_IP_t data = { .ip = ip, .ifname = ifn_buf };

    // Walk through all our LAN IPs, return error unless exactly 1 match found
    ret = util_platform_enum_ifs(UTIL_IF_ENUM_RTR_LAN, find_if_ip, &data);
    if(ret == 1)
    {
        return 0;
    }
    if(ret > 0)
    {
        return 1;
    }

    return -1;
}
