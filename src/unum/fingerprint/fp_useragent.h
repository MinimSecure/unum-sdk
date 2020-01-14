// (c) 2019 minim.co
// HTTP UserAgent fingerprinting subsystem include file

#ifndef _FINGERPRINT_USERAGENT_H
#define _FINGERPRINT_USERAGENT_H

// Returns pointer to the UserAgent info table stats.
// Use from the tpcap callbacks only.
// Subsequent calls override the returned data.
// Pass TRUE to reset the table (allows to get data and reset in one call)
FP_TABLE_STATS_t *fp_useragent_tbl_stats(int reset);

// Get the pointer to the UserAgent info table
// The table should only be accessed from the tpcap thread
FP_USERAGENT_t *fp_get_useragent_tbl(void);

// Reset all UserAgent tables (including the table usage stats)
void fp_reset_useragent_tables(void);

// Init UserAgent fingerprinting
// Returns: 0 - if successful
int fp_useragent_init(void);

#endif // _FINGERPRINT_USERAGENT_H
