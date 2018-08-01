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

// fingerprinting subsystem common include file

#ifndef _FINGERPRINT_COMMON_H
#define _FINGERPRINT_COMMON_H


// Pull in data tables structures
#include "fp_data_tables.h"

// DHCP fingerprinting include
#include "fp_dhcp.h"

// mDNS fingerprinting include
#include "fp_mdns.h"

// SSDP (UPnP) fingerprinting include
#include "fp_ssdp.h"


// Functions returning template for building fingerprinting
// JSON, called by devices telemetry when it prepares its own JSON.
JSON_KEYVAL_TPL_t *fp_mk_json_tpl_f(char *key);

// Reset all fingerprinting tables (including the table usage stats)
void fp_reset_tables(void);

// Subsystem init fuction
int fp_init(int level);

#endif // _FINGERPRINT_COMMON_H
