// (c) 2017-2018 minim.co
// DHCP fingerprinting subsystem include file

#ifndef _FINGERPRINT_DHCP_H
#define _FINGERPRINT_DHCP_H

// Returns pointer to the DHCP info table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the returned data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
FP_TABLE_STATS_t *fp_dhcp_tbl_stats(int reset);

// Get the pointer to the DHCP info table
// The table should only be accessed from the tpcap thread
FP_DHCP_t **fp_get_dhcp_tbl(void);

// Reset all DHCP tables (including the table usage stats)
void fp_reset_dhcp_tables(void);

// Init DHCP fingerprinting
// Returns: 0 - if successful
int fp_dhcp_init(void);

#endif // _FINGERPRINT_DHCP_H

