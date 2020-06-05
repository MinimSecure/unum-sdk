// (c) 2018 minim.co
// forwarding engine stats subsystem common include file

#ifndef _FESTATS_H
#define _FESTATS_H

// Header with data structures describing the stats tracking tables
#include "festats_data_tables.h"


// Default IP forwarding engine stats collection time interval
// Platform can override in festats.h
#ifndef FESTATS_INTERVAL_MSEC
#  define FESTATS_INTERVAL_MSEC 9990
#endif // FESTATS_INTERVAL_MSEC

// Default max number of IP forwarding connections to track
// Platform can override in festats.h
// Note: that unlike DTEL_MAX_CONN this table needs an entry
// for each src_ip:src_port -> dst_ip:dst_port permutation.
#ifndef FESTATS_MAX_CONN
#  define FESTATS_MAX_CONN 8192 // Must be power of 2
#endif // FESTATS_MAX_CONN

// Default max arp tracker entries
#ifndef FESTATS_MAX_ARP
#  define FESTATS_MAX_ARP 1024
#endif // FESTATS_MAX_ARP

// The values are placed in the tables based on their unique keys
// hash. This works well while the tables are mostly empty.
// When they fill up the number of collisions grows and the time needed to
// find or add an entry increases. In order to keep it fast we
// limit how much collisions the table functions will examine
// before giving up. Unlike the rest of the tables the festats define
// those limits explicitly (not by a divider). This is because festats
// ARP table is never cleaned and the connection table has to be defragmented.
// The above requires the search limits to be quite low to avoid performance
// degradation in case the tables fill up.
// Note: the usage data (for dev telemetry) show that more than 10 positions
//       migh need to be examined when the table becomes filled up over ~60%
#ifndef FESTATS_CONN_SEARCH_LIMITER
#  define FESTATS_CONN_SEARCH_LIMITER 32
#endif // FESTATS_CONN_SEARCH_LIMITER
#ifndef FESTATS_ARP_SEARCH_LIMITER
#  define FESTATS_ARP_SEARCH_LIMITER 16
#endif // FESTATS_ARP_SEARCH_LIMITER


// These variables are used by festats test to set custom
// paths to the files for ARP and platform connection trackers.
#if DEBUG
extern char test_arp_file[];
extern char test_conn_file[];
#endif // DEBUG


// Platform code can call this function to fix the connection header.
// It checks and swaps, if necessary, src/dst connection IP addresses
// & ports. It also updates the interface name where it was discovered.
// The intent is to allow platforms to prepare header structure by
// simply storing the connection IPs/ports in the header then call
// this function to finish setting all the remaining header fields.
// Parameters:
// hdr - pointer to the header to work on
// Returns:
// 0 - the header is ready, no reorder was needed
// positive - the header is ready, reordered src/dst
// negative - the header is not ready, ignore the connection
int fe_prep_conn_hdr(FE_CONN_HDR_t *hdr);

// Platform code calls this function to start an update (or addition) of a
// connection in the connection table. The finds or add a connection, locks it
// and returns the pointer to the platform code. The platform code should update
// the counters (if changed) and call fe_upd_conn_end() to release the entry.
// The function should only be called from the festats thread.
// Parameters:
// hdr - header of the connection to find
// Returns:
// Pointer to conection entry or NULL if it's not found and cannot be added
FE_CONN_t *fe_upd_conn_start(FE_CONN_HDR_t *hdr);

// Platform code calls this function to finish updating connection entry
// that has been returned by fe_upd_conn_start(). For the platforms that
// do not fill in MAC of the LAN device it tries to get it from ARP table.
// The function should only be called from the festats thread.
// Parameters:
// conn - pointer to connection being updated
// updated - TRUE if the counters changed, FALSE no changes,
//           but conection is still alive
// Returns:
// Pointer to conection entry or NULL if it's not found and cannot be added
void fe_upd_conn_end(FE_CONN_t *conn, int updated);

// Returns a copy of the connection table stats. 
// The "done" has to be FALSE when dev telemetry gets the pointer with
// data. The data is guaranteed to be unchanging at that point and till the
// function is called with "done" set to TRUE.
// Unlike similar .._tbl_stats() functions the reset is done automatically
// when the call has been made to capture the previous snapshot of the data.
// It will return null and will not reset if there was no new data snapshot
// done since the previous request.
FE_TABLE_STATS_t *fe_conn_tbl_stats(int done);

// Devices telemetry subsystem calls this function to collect connecton
// counters for the platforms that support IP forwarding acceleration.
// (for the ones that do not compile festats in it has an empty stub).
// Call this from the tpcap thread only.
// Note:
// If an entry in the festats connection table is locked, it will
// be skipped for the current pass and (hopefuly) will get added later.
void fe_report_conn(void);

// Function called by festats common code to start the IP forwarding
// engine stats collection pass.
void fe_platform_update_stats();

// Init and start festats subsystem (called by dev telemetry init).
// Declared weak as not all the subsystems include festats.
__attribute__((weak)) int fe_start_subsystem(void);

#endif // _FESTATS_H

