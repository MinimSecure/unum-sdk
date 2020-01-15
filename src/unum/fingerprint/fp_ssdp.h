// Copyright 2019 - 2020 Minim Inc
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

// DHCP fingerprinting subsystem include file

#ifndef _FINGERPRINT_SSDP_H
#define _FINGERPRINT_SSDP_H

// Timeout (in sec) for scheduling the inital scan at startup
// (it is scheduled at init, but if the agent is not yet
// activated rescheduled w/ the same delay again)
#define FP_SSDP_DISCOVERY_INIT_DELAY (3 * 60)

// How many seconds to give the devices to respond to our
// discovery message
#define FP_SSDP_DISCOVERY_PERIOD 5

// How many times to do the discovery within each discovery cycle
#define FP_SSDP_DISCOVERY_ATTEMPTS 3

// After a discovery cycle is complete in how many seconds to schedule
// the next one (unless server requests it earlier by sending a command)
#define FP_SSDP_DISCOVERY_CYCLE_INTERVAL (24 * 60 * 60)


// Returns pointer to the SSDP info table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the returned data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
FP_TABLE_STATS_t *fp_ssdp_tbl_stats(int reset);

// Get the pointer to the SSDP info table
// The table should only be accessed from the tpcap thread
FP_SSDP_t **fp_get_ssdp_tbl(void);

// The "do_ssdp_discovery" command processor. It executes
// concurrently with the SSDP timer driven routines and
// has to make sure there is only one SSDP timer/discovery
// set/being processed at any time.
// Note: declared weak so platforms not doing it do not have to stub
void __attribute__((weak)) cmd_ssdp_discovery(void);

// Reset all SSDP tables (including the table usage stats)
void fp_reset_ssdp_tables(void);

// Init SSDP fingerprinting
// Returns: 0 - if successful
int fp_ssdp_init(void);

#endif // _FINGERPRINT_SSDP_H

