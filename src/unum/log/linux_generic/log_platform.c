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

// Platfrom specific unum logging subsystem code

#include "unum.h"

// The log control & configuration for the platform

LOG_CONFIG_t log_cfg[] = {
[LOG_DST_STDOUT ] = {LOG_FLAG_STDOUT},
[LOG_DST_CONSOLE] = {LOG_FLAG_TTY | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "/dev/stdout"},
[LOG_DST_UNUM   ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     LOG_PATH_PREFIX "/unum.log", 128*1024, 140*1024, 2},
[LOG_DST_HTTP   ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     LOG_PATH_PREFIX "/http.log", 128*1024, 140*1024, 2},
[LOG_DST_MONITOR] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     LOG_PATH_PREFIX "/monitor.log", 32*1024, 48*1024, 1},
#ifdef DEBUG
[LOG_DST_DEBUG  ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     LOG_PATH_PREFIX "/debug.log", 64*1024, 76*1024, 1},
#endif // DEBUG
[LOG_DST_MAX    ] = {} // for consistency, does not really need an entry
};
