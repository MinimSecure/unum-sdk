// (c) 2018 minim.co
// unum router cpuinfo include file

#ifndef __CPUINFO_H
#define __CPUINFO_H

#include "unum.h"


// Max CPUs we can handle
#define CPUINFO_MAX_CPUS 32

typedef struct {
    int num_of_cpus;
    struct {
        unsigned long cpu_usage_cycles;
        unsigned long cpu_idle_cycles;
        unsigned long cpu_softirq_cycles;
     } stats[CPUINFO_MAX_CPUS];
} cpuinfo_t;


// Calback returning pointer to the integer value JSON builder adds
// to the request.
int *cpu_usage_f(char *key);

// Get counter of the latest CPU info update
int get_cpuinfo_counter(void);

// Updates the CPU info. The old data end up in cpuinfo_prev_p, the new
// in cpuinfo_cur_p. Returns consecutive number of the CPU info captured
// (which is unchanged if it fails to capture the new info).
int update_cpuinfo(void);

#endif // __CPUINFO_H
