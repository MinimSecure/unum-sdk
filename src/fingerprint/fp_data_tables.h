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

// devices telemetry data tables include file

#ifndef _FP_DATA_TABLES_H
#define _FP_DATA_TABLES_H

// The values are placed in the tables based on their unique keys
// hash. This works well while the tables are mostly empty.
// When they fill up the number of collisions grows and the time needed to
// find or add an entry increases. In order to keep it fast we
// limit how much collisions the table functions will examine
// before giving up (to add or find an item). The number is a fraction
// of the table max size. The define below determines what fraction
// that is. For example:
// 1 - do not give up until the whole table is examined.
// 2 - examine 1/2 of the table before giving up
// 4 - examine 1/4 of the table before giving up
// For the fingerprinting missing info is not critical, so in general
// the preference is to drop the information if the table is overloaded.
#define FP_SEARCH_LIMITER 4

// SSDP fingerprinting info
struct _FP_SSDP {
    unsigned char mac[6]; // MAC address of the device sending SSDP response
    unsigned short truncated; // TRUE if the data is truncated
#define FP_MAX_SSDP_DATA 768  // Average SSDP pkt is assumed to be 512 bytes long
    char data[FP_MAX_SSDP_DATA]; // SSDP payload data (0 - terminated)
};
typedef struct _FP_SSDP FP_SSDP_t;

// DHCP fingerprinting info
struct _FP_DHCP {
    unsigned char mac[6]; // MAC address of the device sending DHCP request
    unsigned short blob_len; // Length of options data in the blob array
#define FP_MAX_DHCP_OPTIONS 308
    unsigned char blob[FP_MAX_DHCP_OPTIONS]; // DHCP options data
};
typedef struct _FP_DHCP FP_DHCP_t;

// mDNS fingerprinting info
struct _FP_MDNS {
    unsigned char mac[6]; // MAC address of the device sending mDNS request
#define FP_MAX_MDNS_NAME 256
    unsigned char name[FP_MAX_MDNS_NAME]; // Name of mDNS TXT record
    unsigned short blob_len; // Length of TXT strings in the blob array
#define FP_MAX_MDNS_TXT 512
    unsigned char blob[FP_MAX_MDNS_TXT]; // mDNS TXT data
    unsigned short port; // SRV port
};
typedef struct _FP_MDNS FP_MDNS_t;

// Structure tracking statistics of the table operations
// (same layout for all the tables)
typedef struct _FP_TABLE_STATS {
    unsigned long add_all;   // All attempts to add an item
    unsigned long add_limit; // Items not added due to values being over limit
    unsigned long add_busy;  // Items not added due to no free rows
    unsigned long add_10;    // Items placed to <10 offset from the hash-row
    unsigned long add_found; // Items being added that are already in the table
    unsigned long add_new;   // New items added to the table
} FP_TABLE_STATS_t;

#endif // _FP_DATA_TABLES_H
