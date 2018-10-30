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
