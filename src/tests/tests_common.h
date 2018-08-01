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

// unum command processor common include file

#ifndef _TESTS_COMMON_H
#define _TESTS_COMMON_H


// Defines fot all test IDs. Platforms might implement subset,
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
#define U_TEST_UNUSED       18 // next available entry


// Test load cfg (stubbed)
int test_loadCfg(void);

// Test save cfg (stubbed)
int test_saveCfg(void);

// Test wireless info collection (stubbed)
void test_wireless(void);

// Test the agent crash handling code
void test_crash_handling(void);

// Print info from the packet headers
void tpkt_print_pkt_info(struct tpacket2_hdr *thdr,
                         struct ethhdr *ehdr,
                         struct iphdr *iph);

// Test packet capturing filters and callback hooks
int tpcap_test_filters(char *filters_file);

// Returns the running test number (0 if no test running)
int get_test_num(void);

// Test mode main entry point
int run_tests(char *test_num_str);

#endif // _TESTS_COMMON_H
