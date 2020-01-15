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
