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

// mDNS fingerprinting subsystem include file

#ifndef _FINGERPRINT_MDNS_H
#define _FINGERPRINT_MDNS_H

// Returns pointer to the mDNS info table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the returned data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
FP_TABLE_STATS_t *fp_mdns_tbl_stats(int reset);

// Get the pointer to the mDNS info table
// The table should only be accessed from the tpcap thread
FP_MDNS_t **fp_get_mdns_tbl(void);

// The "do_mdns_discovery" command processor.
int cmd_mdns_discovery(char *cmd, char *s, int s_len);

// Reset all mDNS tables (including the table usage stats)
void fp_reset_mdns_tables(void);

// Init mDNS fingerprinting
// Returns: 0 - if successful
int fp_mdns_init(void);

#endif // _FINGERPRINT_MDNS_H
