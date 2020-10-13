// Copyright 2018 - 2020 Minim Inc
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

// Platfrom specific unum logging subsystem code

#include "unum.h"

// The log control & configuration for the platform

LOG_CONFIG_t log_cfg[] = {
[LOG_DST_STDOUT ] = {LOG_FLAG_STDOUT},
[LOG_DST_CONSOLE] = {LOG_FLAG_TTY | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     UNUM_LOG_CONSOLE_NAME},
[LOG_DST_UNUM   ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "unum.log",
                     UNUM_LOG_SCALE_FACTOR * 32 * 1024,
                     (UNUM_LOG_SCALE_FACTOR * 32 + UNUM_LOG_SCALE_FACTOR * 32
                            / UNUM_LOG_CUT_FRACTION) * 1024,
                     UNUM_LOG_EXTRA_ROTATIONS},
[LOG_DST_HTTP   ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "http.log",
                     UNUM_LOG_SCALE_FACTOR * 32 * 1024,
                     (UNUM_LOG_SCALE_FACTOR * 32 + UNUM_LOG_SCALE_FACTOR * 32
			/ UNUM_LOG_CUT_FRACTION) * 1024,
                     UNUM_LOG_EXTRA_ROTATIONS},
[LOG_DST_MONITOR] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "monitor.log",
                     UNUM_LOG_SCALE_FACTOR * 8 * 1024,
                     (UNUM_LOG_SCALE_FACTOR * 8 + UNUM_LOG_SCALE_FACTOR * 8
			/ UNUM_LOG_CUT_FRACTION) * 1024,
                     -1 + UNUM_LOG_EXTRA_ROTATIONS},
#ifdef DEBUG
[LOG_DST_DEBUG  ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "debug.log",
                     UNUM_LOG_SCALE_FACTOR * 16 * 1024,
                     (UNUM_LOG_SCALE_FACTOR * 16 + UNUM_LOG_SCALE_FACTOR * 16
			/ UNUM_LOG_CUT_FRACTION) * 1024,
                     -1 + UNUM_LOG_EXTRA_ROTATIONS},
#endif // DEBUG
[LOG_DST_MAX    ] = {} // for consistency, does not really need an entry
};
