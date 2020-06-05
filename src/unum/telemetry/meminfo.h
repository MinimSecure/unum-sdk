// (c) 2018 minim.co
// unum router meminfo include file

#ifndef __MEMINFO_H
#define __MEMINFO_H

#include "unum.h"


// Structure to hold the Memory Information
typedef struct {
    int meminfo_free_perc;
    int meminfo_available_perc;
} meminfo_t;


// Calback returning pointer to the integer value JSON builder adds
// to the request.
int *mem_info_f(char *key);

// Get counter of the latest meminfo update
int get_meminfo_counter(void);

// Update the Memory Information counters. Returns consecutive number
// of the mem info captured (which is unchanged if it fails to capture
// the new info).
int update_meminfo(void);

#endif // __MEMINFO_H

