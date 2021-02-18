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

// Log control & configuration data structure for linux_generic.
// The structure is customized to use stdout for LOG_DST_CONSOLE.
// The defaults for all constants used in this structure are defined
// in log_common.h and can be overridden by the platform. Please refer
// to the log_common.h for more details.
LOG_CONFIG_t log_cfg[] = {
[LOG_DST_STDOUT ] = {LOG_FLAG_STDOUT},
[LOG_DST_CONSOLE] = {LOG_FLAG_STDOUT},
[LOG_DST_UNUM   ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "unum.log",
                     UNUM_LOG_SIZE_BIG_KB * 1024,
                     (UNUM_LOG_SIZE_BIG_KB + UNUM_LOG_CUT_EXTRA_KB) * 1024,
                     UNUM_LOG_ROTATIONS_HIGH},
[LOG_DST_HTTP   ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "http.log",
                     UNUM_LOG_SIZE_BIG_KB * 1024,
                     (UNUM_LOG_SIZE_BIG_KB + UNUM_LOG_CUT_EXTRA_KB) * 1024,
                     UNUM_LOG_ROTATIONS_HIGH},
[LOG_DST_MONITOR] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "monitor.log",
                     UNUM_LOG_SIZE_SMALL_KB * 1024,
                     (UNUM_LOG_SIZE_SMALL_KB + UNUM_LOG_CUT_EXTRA_KB) * 1024,
                     UNUM_LOG_ROTATIONS_LOW},
#ifdef FW_UPDATER_RUN_MODE
[LOG_DST_UPDATE ] = {LOG_FLAG_FILE | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "updater.log",
                     UNUM_LOG_SIZE_SMALL_KB * 1024,
                     (UNUM_LOG_SIZE_SMALL_KB + UNUM_LOG_CUT_EXTRA_KB) * 1024,
                     UNUM_LOG_ROTATIONS_HIGH},
[LOG_DST_UPDATE_MONITOR] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "updater_monitor.log",
                     UNUM_LOG_SIZE_SMALL_KB * 1024,
                     (UNUM_LOG_SIZE_SMALL_KB + UNUM_LOG_CUT_EXTRA_KB) * 1024,
                     UNUM_LOG_ROTATIONS_LOW},
#endif // FW_UPDATER_RUN_MODE
#ifdef SUPPORT_RUN_MODE
[LOG_DST_SUPPORT] = {LOG_FLAG_FILE | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "support.log",
                     UNUM_LOG_SIZE_SMALL_KB * 1024,
                     (UNUM_LOG_SIZE_SMALL_KB + UNUM_LOG_CUT_EXTRA_KB) * 1024,
                     UNUM_LOG_ROTATIONS_HIGH},
#endif // SUPPORT_RUN_MODE
#ifdef DEBUG
[LOG_DST_DEBUG  ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "debug.log",
                     UNUM_LOG_SIZE_MEDIUM_KB * 1024,
                     (UNUM_LOG_SIZE_MEDIUM_KB + UNUM_LOG_CUT_EXTRA_KB) * 1024,
                     UNUM_LOG_ROTATIONS_HIGH},
#endif // DEBUG
[LOG_DST_DROP   ] = {} // for consistency, does not really need an entry
};
