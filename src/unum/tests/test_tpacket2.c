// (c) 2017-2019 minim.co
// unum packet capturing tests, common code

#include "unum.h"

// Compile only in debug version
#ifdef DEBUG

// Forward declarations
static void eth_pkt_rcv_cb(TPCAP_IF_t *tpif,
                           PKT_PROC_ENTRY_t *pe,
                           struct tpacket2_hdr *thdr,
                           struct ethhdr *ehdr);
static void ip_pkt_rcv_cb(TPCAP_IF_t *tpif,
                          PKT_PROC_ENTRY_t *pe,
                          struct tpacket2_hdr *thdr,
                          struct iphdr *iph);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define MY_ETH_ARP_PROTO 0x0608
#  define MY_ETH_IP_PROTO  0x0008
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define MY_ETH_ARP_PROTO 0x0806
#  define MY_ETH_IP_PROTO  0x0800
#else
#  error Unable to determine the byte order
#endif

// Array storing test filters/hooks
static PKT_PROC_ENTRY_t tst_hooks[MAX_PKT_PROC_ENTRIES] = {
    { PKT_MATCH_ETH_UCAST_SRC | PKT_MATCH_ETH_MCAST_DST,
      {},
      0,
      {},
      0,
      {},
      eth_pkt_rcv_cb, NULL, NULL, NULL,
      "Any pkt from unicast to multicast eaddr" },
    { PKT_MATCH_ETH_MAC_ANY,
      { {0x00,0x0c,0x29,0x3b,0xb1,0x70} },
      PKT_MATCH_ALL_IP,
      {},
      0,
      {},
      NULL, ip_pkt_rcv_cb, NULL, NULL,
      "Any IP to or from 00:0c:29:3b:b1:70" },
    { PKT_MATCH_ETH_TYPE,
      { {}, MY_ETH_ARP_PROTO },
      0,
      {},
      0,
      {},
      eth_pkt_rcv_cb, NULL, NULL, NULL,
      "Any ARP pkt" },
    { PKT_MATCH_ETH_TYPE | PKT_MATCH_ETH_TYPE_NEG,
      { {}, MY_ETH_IP_PROTO },
      0,
      {},
      0,
      {},
      eth_pkt_rcv_cb, NULL, NULL, NULL,
      "Any non-IP pkt" },
    { PKT_MATCH_ETH_UCAST_ANY | PKT_MATCH_ETH_MAC_NEG,
      {},
      0,
      {},
      0,
      {},
      eth_pkt_rcv_cb, NULL, NULL, NULL,
      "Both src&dst MACs are multicast" },
    { 0,
      {},
      PKT_MATCH_IP_NET_ANY,
      { { .b = {192, 168, 0, 1}}, { .b = {255, 255, 255, 0}}},
      0,
      {},
      NULL, ip_pkt_rcv_cb, NULL, NULL,
      "IP to/from 192.168.0.1/255.255.255.0" },
    { 0,
      {},
      0,
      {},
      PKT_MATCH_TCPUDP_P1_ANY | PKT_MATCH_TCPUDP_UDP_ONLY,
      { .p1 = 53 },
      NULL, ip_pkt_rcv_cb, NULL, NULL,
      "UDP src/dst port 53 (DNS)" },
};
// Count for the tst_hooks[]
static int tst_hooks_cnt = 0;


// Print info from the packet headers
void tpkt_print_pkt_info(struct tpacket2_hdr *thdr,
                         struct ethhdr *ehdr,
                         struct iphdr *iph)
{
        printf("  Eth " MAC_PRINTF_FMT_TPL " -> " MAC_PRINTF_FMT_TPL
               " (0x%04x)\n",
               MAC_PRINTF_ARG_TPL(ehdr->h_source),
               MAC_PRINTF_ARG_TPL(ehdr->h_dest),
               ntohs(ehdr->h_proto));

        if(iph) {
            printf("  IP  " IP_PRINTF_FMT_TPL " -> " IP_PRINTF_FMT_TPL
                   " (%d)\n",
                   IP_PRINTF_ARG_TPL(&iph->saddr),
                   IP_PRINTF_ARG_TPL(&iph->daddr),
                   iph->protocol);
            if(iph->protocol == 6) {
                struct tcphdr* tcph = ((void *)iph) + sizeof(struct iphdr);
                printf("  TCP %u -> %u\n", ntohs(tcph->source),
                                           ntohs(tcph->dest));
            } else if(iph->protocol == 17) {
                struct udphdr* udph = ((void *)iph) + sizeof(struct iphdr);
                printf("  UDP %u -> %u\n", ntohs(udph->source),
                                           ntohs(udph->dest));
            } else if(iph->protocol == 1) {
                struct icmphdr* icmp = ((void *)iph) + sizeof(struct iphdr);
                printf("  ICMP type %u code %u", icmp->type,
                                                 icmp->code);
                if(icmp->type == ICMP_ECHO || icmp->type == ICMP_ECHOREPLY) {
                    printf(" seq %u\n", ntohs(icmp->un.echo.sequence));
                } else {
                    printf("\n");
                }
            }
        }
        printf("    tp_snaplen : %d\n", thdr->tp_snaplen);
        printf("    tp_sec     : %d\n", thdr->tp_sec);
        printf("    tp_nsec    : %d\n", thdr->tp_nsec);

//#define DBG_PRINT_PAYLOAD
#ifdef DBG_PRINT_PAYLOAD
        printf("    payload    :");
        int ii, max = (thdr->tp_snaplen > 255) ? 255 : thdr->tp_snaplen;
        unsigned char *ptr = (unsigned char *)ehdr;
        for(ii = 0; ii < max; ++ii)
        {
            if(ii % 16 == 0) {
                printf("\n      %03d:", ii);
            }
            printf(" %02x", (int)ptr[ii]);
        }
        printf("\n");
#endif // DBG_PRINT_PAYLOAD

        return;
}

// Ethernet function for tpcap processing entries test
static void eth_pkt_rcv_cb(TPCAP_IF_t *tpif,
                           PKT_PROC_ENTRY_t *pe,
                           struct tpacket2_hdr *thdr,
                           struct ethhdr *ehdr)
{
    printf("Packet on interface: %s, hook: %02d\n",
           tpif->name, (pe - tst_hooks) + 1);
    tpkt_print_pkt_info(thdr, ehdr, NULL);
}

// IP function for tpcap processing entries test
static void ip_pkt_rcv_cb(TPCAP_IF_t *tpif,
                          PKT_PROC_ENTRY_t *pe,
                          struct tpacket2_hdr *thdr,
                          struct iphdr *iph)
{
    struct ethhdr *ehdr = (struct ethhdr *)((void *)thdr + thdr->tp_mac);
    printf("Packet on interface: %s, hook: %02d\n",
           tpif->name, (pe - tst_hooks) + 1);
    tpkt_print_pkt_info(thdr, ehdr, iph);
}

// Runs packet capturing for test that requre it in a separate thread
static void launch_tpcap(THRD_PARAM_t *p)
{
    TPCAP_RUN_TEST(TPCAP_TEST_FILTERS);
}

// Test packet capturing filters and callback hooks
// It tries to load filters from a file if present, if not
// present loads a few hardcoded filters & hooks.
// Flter file format (filter per line):
// 0x#### 0x#### 0x#### ##:##:##:##:##:## ##### #.#.#.# #.#.#.# ### ##### #####
// flags_eth ip tcpudp  MAC               etype   IP a1      a2 prot   p1    p2
// For all entries the receive hook is the print function, no chaining
// etype, p1, p2 - do not convert to network byte order
int tpcap_test_filters(char *filters_file)
{
    FILE *f;
    char str[256];
    int ii = 0;

    printf("Starting the packet capturing thread...\n");
    util_start_thrd("tpcap", launch_tpcap, NULL, NULL);
    sleep(1);
    printf("Done. Trying to load filters from %s\n", filters_file);

    f = fopen(filters_file, "r");
    if(f) {
        unsigned int f1, f2, f3, etype, proto, p1, p2;
        int n;
        char mac[20];
        char a1[20];
        char a2[20];

        tst_hooks_cnt = 0;
        while(fgets(str, sizeof(str), f) != NULL)
        {
            memset(&(tst_hooks[0]), 0, sizeof(PKT_PROC_ENTRY_t));
            n = sscanf(str, "0x%x 0x%x 0x%x %17s %u %15s %15s %u %u %u",
                       &f1, &f2, &f3, mac, &etype, a1, a2, &proto, &p1, &p2);
            if(n != 10) {
                printf("Filters line %d, read %d out of 10 parameters\n",
                       tst_hooks_cnt + 1, n);
                return -1;
            }
            PKT_PROC_ENTRY_t *pe = &(tst_hooks[tst_hooks_cnt]);
            pe->flags_eth = f1;
            pe->flags_ip = f2;
            pe->flags_tcpudp = f3;
            pe->eth.proto = htons(etype);
            pe->ip.proto = proto;
            pe->tcpudp.p1 = p1;
            pe->tcpudp.p2 = p2;

            if(!inet_aton(a1, (struct in_addr *)pe->ip.a1.b) ||
               !inet_aton(a2, (struct in_addr *)pe->ip.a2.b))
            {
                printf("Filters line %d, invalid IP '%s' or '%s'\n",
                       tst_hooks_cnt + 1, a1, a2);
                return -1;
            }
            if(!ether_aton_r(mac, (struct ether_addr *)pe->eth.mac))
            {
                printf("Filters line %d, invalid MAC '%s'\n",
                       tst_hooks_cnt + 1, mac);
                return -1;
            }
            pe->desc = strdup(str);
            if(f2 || f3) {
                pe->ip_func = ip_pkt_rcv_cb;
            } else if(f1) {
                pe->eth_func = eth_pkt_rcv_cb;
            }
            tst_hooks_cnt++;
        }
        fclose(f);
        printf("Read %d filters\n", tst_hooks_cnt);
    } else {
        printf("No file %s, using hardcoded filters/hooks\n", filters_file);
        for(tst_hooks_cnt = 0;
            tst_hooks_cnt < MAX_PKT_PROC_ENTRIES;
            tst_hooks_cnt++)
        {
            PKT_PROC_ENTRY_t *pe = &(tst_hooks[tst_hooks_cnt]);
            if(!pe->flags_tcpudp && !pe->flags_ip && !pe->flags_eth)
            {
                break;
            }
        }
    }
    if(tst_hooks_cnt == 0) {
        printf("No filters/hooks found, exiting.\n");
        return -1;
    }
    printf("There are %d filters/hooks found\n", tst_hooks_cnt);

    int hook_added[MAX_PKT_PROC_ENTRIES] = {};
    for(;;) {
        printf("Choose an action (1-%d: add/remove entry, l: list, e: edit):\n",
               tst_hooks_cnt);
        scanf("%02s", str);
        str[2] = 0;
        ii = 0;
        sscanf(str, "%d", &ii);
        --ii;
        if(strcasecmp(str, "l") == 0) {
           for(ii = 0; ii < tst_hooks_cnt; ii++) {
               PKT_PROC_ENTRY_t *pe = &(tst_hooks[ii]);
               printf("%02d: 0x%08x 0x%08x 0x%08x %s\n", ii + 1,
                      pe->flags_eth, pe->flags_ip, pe->flags_tcpudp,
                      (pe->desc ? pe->desc : ""));
               if(pe->flags_eth) {
                   printf("    MAC: " MAC_PRINTF_FMT_TPL " Ethtype: 0x%04x\n",
                          MAC_PRINTF_ARG_TPL(pe->eth.mac), pe->eth.proto);
               }
               if(pe->flags_ip) {
                   printf("    A1: " IP_PRINTF_FMT_TPL " A2: " IP_PRINTF_FMT_TPL
                          " Proto: %u\n",
                          IP_PRINTF_ARG_TPL(pe->ip.a1.b),
                          IP_PRINTF_ARG_TPL(pe->ip.a2.b),
                          pe->ip.proto);
               }
               if(pe->flags_tcpudp) {
                   printf("    P1: %u P2: %u\n",
                          pe->tcpudp.p1, pe->tcpudp.p2);
               }
               printf("    Status: %s\n",
                      hook_added[ii] ? "active" : "inactive");
           }
        } else if(strcasecmp(str, "e") == 0) {
            printf("Choose an entry to edit (1-%d):\n",
                   tst_hooks_cnt);
            scanf("%02s", str);
            str[2] = 0;
            ii = 0;
            sscanf(str, "%d", &ii);
            --ii;
            if(ii >= 0 && ii < tst_hooks_cnt) {
                PKT_PROC_ENTRY_t *pe = &(tst_hooks[ii]);
                if(pe->flags_eth) {
                    printf("MAC: " MAC_PRINTF_FMT_TPL "\n",
                           MAC_PRINTF_ARG_TPL(pe->eth.mac));

                    do {
                        printf("MAC: ");
                        scanf("%s", str);
                    } while(sscanf(str, MAC_SSCANF_FMT_TPL,
                                   MAC_SSCANF_ARG_TPL(pe->eth.mac)) != 6);

                    printf("Ethtype: 0x%04x\n", pe->eth.proto);
                    do {
                        printf("Ethtype: 0x");
                        scanf("%s", str);
                   } while(sscanf(str, "%04hx", &pe->eth.proto) != 1);
                }
                if(pe->flags_ip) {
                    printf("A1: " IP_PRINTF_FMT_TPL "\n",
                           IP_PRINTF_ARG_TPL(pe->ip.a1.b));
                    do {
                        printf("A1: ");
                        scanf("%s", str);
                    } while(sscanf(str, IP_SSCANF_FMT_TPL,
                                   IP_SSCANF_ARG_TPL(pe->ip.a1.b)) != 4);

                    printf("A2: " IP_PRINTF_FMT_TPL "\n",
                           IP_PRINTF_ARG_TPL(pe->ip.a2.b));
                    do {
                        printf("A2: ");
                        scanf("%s", str);
                    } while(sscanf(str, IP_SSCANF_FMT_TPL,
                                   IP_SSCANF_ARG_TPL(pe->ip.a2.b)) != 4);

                    printf("Proto: %u\n", pe->ip.proto);
                    do {
                        printf("Proto: ");
                        scanf("%s", str);
                    } while(sscanf(str, "%hhu", &pe->ip.proto) != 1);
                }
                if(pe->flags_tcpudp) {
                    printf("P1: %u\n", pe->tcpudp.p1);
                    do {
                        printf("P1: ");
                        scanf("%s", str);
                    } while(sscanf(str, "%hu", &pe->tcpudp.p1) != 1);

                    printf("P2: %u\n", pe->tcpudp.p2);
                    do {
                        printf("P2: ");
                        scanf("%s", str);
                    } while(sscanf(str, "%hu", &pe->tcpudp.p2) != 1);
                }
                pe->desc = NULL;
            } else {
                printf("Invalid entry\n");
            }
        } else if(ii >= 0 && ii < tst_hooks_cnt) {
            PKT_PROC_ENTRY_t *pe = &(tst_hooks[ii]);
            if(hook_added[ii] > 0) {
                tpcap_del_proc_entry(pe);
                --hook_added[ii];
                printf("An instance of entry %d deactivated, %d remains\n",
                       ii + 1, hook_added[ii]);
            } else {
                if(tpcap_add_proc_entry(pe) == 0) {
                    printf("An instance of entry %d added\n", ii + 1);
                    hook_added[ii]++;
                } else {
                    printf("Failed to add an instance of entry %d\n", ii + 1);
                }
            }
        }
    }

    return 0;
}

#endif // DEBUG
