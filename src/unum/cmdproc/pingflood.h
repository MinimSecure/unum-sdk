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

// pingflood bandwidth tester include file

#ifndef _PINGFLOOD_H
#define _PINGFLOOD_H


// Default test time (in sec)
#define PINGFLOOD_TEST_TIME_SEC 10

// Default flooding rate limit in bytes/sec (100Mbps)
#define PINGFLOOD_SEND_BYPS_LIMIT (100UL * 1024UL * 1024UL / 8UL)

// Tx ring packet buffer size and the number of buffers
#define PINGFLOOD_TX_PKT_BUF_SIZE 2048
#define PINGFLOOD_TX_PKT_NUM_BUFS 256

// Payload size for ICMP test packets (must be divisible by 4)
#define PINGFLOOD_PAYLOAD_SIZE 1400

// Allowed range for the pigflood test duration
#define PINGFLOOD_MIN_TEST_TIME 5000
#define PINGFLOOD_MAX_TEST_TIME 300000

// Allowed range for the traffic generation rate limit
#define PINGFLOOD_MIN_RATE_LIMIT 32000UL     // 250Kbps
//#define PINGFLOOD_MAX_RATE_LIMIT 13107200UL  // 100mbps
#define PINGFLOOD_MAX_RATE_LIMIT 134217728UL // 1gbps

// Performs device connection speed test using ping flood
// Note: use for local wireless device only
// tgt - target device IP address
// Returns: JSON string w/ results (must be freed w/ util_free_json_str())
//          or NULL if fails
char *do_pingflood_test(char *tgt);

#ifdef DEBUG
void test_pingflood(void);
#endif // DEBUG

#endif // _PINGFLOOD_H
