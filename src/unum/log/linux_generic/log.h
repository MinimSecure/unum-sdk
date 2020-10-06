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

// Unum logging subsystem platform specific include

// Log header shared across all platforms
#include "../log_common.h"

#ifndef _LOG_H
#define _LOG_H

// Define prefix for log files location.
#ifndef LOG_PATH_PREFIX
#  define LOG_PATH_PREFIX "/var/log"
#endif

// Where to direct stdout and stderr in the daemon mode. Warning, do not
// output to a file unless in debug mode. The file will eventually consume
// all the space (since it is not rotated).
#ifdef DEBUG
#  define LOG_STDOUT LOG_PATH_PREFIX "/unum_out.txt"
#else // DEBUG
#  define LOG_STDOUT "/dev/null"
#endif // DEBUG

// Platform log flags (16 upper bits), define if available for the platform
#define LOG_FLAG_FILE   0x00010000  // logging to file
#define LOG_FLAG_STDOUT 0x00020000  // logging to stdout
#define LOG_FLAG_TTY    0x00040000  // logging to tty device
//#define LOG_FLAG_SYSLOG 0x00040000 // logging to sysylog

#define UNUM_LOG_CONSOLE_NAME        "/dev/console"
#define UNUM_LOG_UNUM_MAX_SIZE       128 // KB
#define UNUM_LOG_UNUM_CUT_SIZE       140 // KB
#define UNUM_LOG_UNUM_MAX_FILES      2
#define UNUM_LOG_HTTP_MAX_SIZE       128 // KB
#define UNUM_LOG_HTTP_CUT_SIZE       140 // KB
#define UNUM_LOG_HTTP_MAX_FILES      2
#define UNUM_LOG_MONI_MAX_SIZE       32  // KB
#define UNUM_LOG_MONI_CUT_SIZE       48  // KB
#define UNUM_LOG_MONI_MAX_FILES      1
#define UNUM_LOG_DBG_MAX_SIZE        64  // KB
#define UNUM_LOG_DBG_CUT_SIZE        76  // KB
#define UNUM_LOG_DBG_MAX_FILES       1

#endif // _LOG_H
