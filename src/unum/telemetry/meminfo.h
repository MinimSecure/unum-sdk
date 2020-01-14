// Copyright 2020 Minim Inc
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

