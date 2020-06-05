// (c) 2017-2018 minim.co
// devices telemetry subsystem common include file

#ifndef _DEVTELEMETRY_COMMON_H
#define _DEVTELEMETRY_COMMON_H


// Number of time slot intervals (the TPCAP_TIME_SLICE intervals) in
// each telemetry transmission
#define DEVTELEMETRY_NUM_SLICES 1


// Pull in data tables structures
#include "dt_data_tables.h"

// Pull in the festats include (festats is essentially a dev telemetry's
// subsystem, so do it here rather than in the unum.h)
#include "../festats/festats.h"


// Get the interface counters and capturing stats
DT_IF_STATS_t *dt_get_if_stats(void);

// This functon handles adding festats connection info to devices
// telemetry. It's unused for platforms that do not support festats.
// Returns: TRUE - connection info added, FALSE - unable to add
//          all or some of the info
int dt_add_fe_conn(FE_CONN_t *fe_conn);

// Reset all DNS tables (including the table usage stats)
void dt_reset_dns_tables(void);

// Get the pointer to the DNS ip addresses table
// The table should only be accessed from the tpcap thread
DT_DNS_IP_t *dt_get_dns_ip_tbl(void);

// Get the pointer to the devices table
// The table should only be accessed from the tpcap thread
DT_DEVICE_t **dt_get_dev_tbl(void);

// Generate and pass to the sender the devices telemetry JSON.
// If the previously submitted JSON buffer is not yet consumed
// replace it with the new one.
// The function is called from the TPCAP thread/handler.
// Avoid blocking if possible.
void dt_sender_data_ready(void);

// Returns pointer to the DNS name table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
DT_TABLE_STATS_t *dt_dns_name_tbl_stats(int reset);

// Returns pointer to the DNS IP table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
DT_TABLE_STATS_t *dt_dns_ip_tbl_stats(int reset);

// Returns pointer to the devices table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
DT_TABLE_STATS_t *dt_dev_tbl_stats(int reset);

// Returns pointer to the connections table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
DT_TABLE_STATS_t *dt_conn_tbl_stats(int reset);

// Find DNS IP enty in the DNS IP table
// Returns a pointer to the IP table entry of NULL if not found
// Call only from the TPCAP thread/handlers
DT_DNS_IP_t *dt_find_dns_ip(IPV4_ADDR_t *ip);

// Start the device telemetry sender (it runs in its own thread)
int dt_sender_start(void);

// DNS info collector init function
int dt_dns_collector_init(void);

// Device info collector init function
// Returns: 0 - if successful
int dt_main_collector_init(void);

// Subsystem init fuction
int devtelemetry_init(int level);

#ifdef DEBUG
void dt_dns_tbls_test(void);
void dt_if_counters_test(void);
void dt_main_tbls_test(void);
#endif // DEBUG

#endif // _DEVTELEMETRY_COMMON_H

