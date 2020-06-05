// (c) 2017-2019 minim.co
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
// Note: declared weak so platforms not doing it do not have to stub
int __attribute__((weak)) cmd_mdns_discovery(char *cmd, char *s, int s_len);

// Reset all mDNS tables (including the table usage stats)
void fp_reset_mdns_tables(void);

// Init mDNS fingerprinting
// Returns: 0 - if successful
int fp_mdns_init(void);

#endif // _FINGERPRINT_MDNS_H
