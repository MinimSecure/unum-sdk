// (c) 2017-2018 minim.co
// unum command processor common include file

#ifndef _TESTS_COMMON_H
#define _TESTS_COMMON_H


// Defines for all test IDs. Platforms might implement subset,
// but all the IDs should stay the same for all platforms.
#define U_TEST_CFG_TO_FILE   1 // test dumping cfg to file
#define U_TEST_FILE_TO_CFG   2 // test restoring cfg from file
#define U_TEST_SPEEDTEST     3 // run speedtest
#define U_TEST_TPCAP_BASIC   4 // test basic packet capturing
#define U_TEST_TPCAP_HOOKS   5 // test tpcap filters and callback hooks
#define U_TEST_TPCAP_DNS     6 // test tpcap DNS indo collector
#define U_TEST_TPCAP_STATS   7 // packet capturring and interface stats
#define U_TEST_TPCAP_DT      8 // device and connection info collector
#define U_TEST_TPCAP_DTJSON  9 // dev telemetry JSON generation
#define U_TEST_TPCAP_FPRINT 10 // fingerprinting info collection
#define U_TEST_TIMERS       11 // test timers subsystem
#define U_TEST_WL_RT        12 // radio telemetry info collection
#define U_TEST_WL_SCAN      13 // radio telemetry info collection
#define U_TEST_PORT_SCAN    14 // port scan
#define U_TEST_CRASH        15 // crash handler/tracer
#define U_TEST_CONNCHECK    16 // test connectivity checker
#define U_TEST_PINGFLOOD    17 // test pingflood
#define U_TEST_RTR_TELE     18 // test router telemetry
#define U_TEST_FESTATS      19 // test festats subsystem
#define U_TEST_FE_DEFRAG    20 // test festats defrag
#define U_TEST_FE_ARP       21 // test festats ARP tracker
#define U_TEST_DNS          22 // test dns subsystem
#define U_TEST_ZIP          23 // test dns subsystem
#define U_TEST_UNUSED       24 // next available entry

// Test load cfg (stubbed)
int test_loadCfg(void);

// Test save cfg (stubbed)
int test_saveCfg(void);

// Test wireless info collection (stubbed)
void test_wireless(void);

// Test forwarding engine stats gathering subsystem (stubbed)
int test_festats(void);

// Test forwarding engine stats table defragmenter (stubbed)
int test_fe_defrag(void);

// Test forwarding engine stats ARP tracker (stubbed)
int test_fe_arp(void);

// Test the agent crash handling code
void test_crash_handling(void);

// Test radio telemetry
void test_telemetry(void);

// Print info from the packet headers
void tpkt_print_pkt_info(struct tpacket2_hdr *thdr,
                         struct ethhdr *ehdr,
                         struct iphdr *iph);

// Test packet capturing filters and callback hooks
int tpcap_test_filters(char *filters_file);

// Test DNS Subsystem
int test_dns(void);

// Test Minizip
int __attribute__((weak)) test_zip(void);

// Returns the running test number (0 if no test running)
int get_test_num(void);

// Test mode main entry point
int run_tests(char *test_num_str);

// Helper routine to dump a memory buffer
void print_mem_buf(void *buf, int len);

#endif // _TESTS_COMMON_H
