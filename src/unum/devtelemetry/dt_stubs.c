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

// device telemetry information collector stubs

#include "unum.h"


// Stub for the platforms that do not compile in festats subsystem.
// Always return NULL which should prevent inclusion of the data.
FE_TABLE_STATS_t * __attribute__((weak)) fe_conn_tbl_stats(int done)
{
    // Nothing to do here
    return NULL;
}

// Stub for the platforms that do not compile in festats subsystem
void __attribute__((weak)) fe_report_conn(void)
{
    // Nothing to do here
    return;
}
