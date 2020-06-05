// (c) 2017-2019 minim.co
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

// HTTP User Agent fingerprinting include
#include "fp_useragent.h"


// Functions returning template for building fingerprinting
// JSON, called by devices telemetry when it prepares its own JSON.
JSON_KEYVAL_TPL_t *fp_mk_json_tpl_f(char *key);

// Reset all fingerprinting tables (including the table usage stats)
void fp_reset_tables(void);

// Subsystem init fuction
int fp_init(int level);

#endif // _FINGERPRINT_COMMON_H
