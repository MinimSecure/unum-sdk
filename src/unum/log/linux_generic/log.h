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

// Console device name (using default from log_common.h)
//#define UNUM_LOG_CONSOLE_NAME "/dev/console"

// Platform log flags (16 upper bits), define if available for the platform
#define LOG_FLAG_FILE   0x00010000  // logging to file
#define LOG_FLAG_STDOUT 0x00020000  // logging to stdout
#define LOG_FLAG_TTY    0x00040000  // logging to tty device
//#define LOG_FLAG_SYSLOG 0x00040000 // logging to sysylog

// Constans from log_common.h (the defaults should be fine for the platform):
// The below defaults determine log file sizes for big, medium and small
// logs. The platform can override those and set its own limits.
// The platform can also override the whole log_cfg[] if the default categories
// (big, medium, small) are not sufficient.
#undef UNUM_LOG_SIZE_BIG_KB
#undef UNUM_LOG_SIZE_MEDIUM_KB
#undef UNUM_LOG_SIZE_SMALL_KB
#define UNUM_LOG_SIZE_BIG_KB 512
#define UNUM_LOG_SIZE_MEDIUM_KB 256
#define UNUM_LOG_SIZE_SMALL_KB 128
// Maximum extra space for the log file to grow after it exceedes the limit,
// but before the messsage being written is cut off
//#define UNUM_LOG_CUT_EXTRA_KB 12
// Default number of files to keep when rotating logs after max size is reached
// (similarly to log size limits split into categories - high and low)
//#define UNUM_LOG_ROTATIONS_HIGH 2
//#define UNUM_LOG_ROTATIONS_LOW 1
// Any of the above constants can be undefined and then changed in platform
// specific log.h file, for example
//#undef UNUM_LOG_SIZE_BIG_KB then #define UNUM_LOG_SIZE_BIG_KB 256

// The below constant should be defined (in platform's log.h) if the platform
// allows for --log-dir option changing default location of the log files.
// The intended use is temporary relocation to the persistent filesystem for
// debugging the agent behavior across reboots. Relocation works for all the
// file paths in log_cfg[] structure that do not start with '/'.
#define UNUM_LOG_ALLOW_RELOCATION

#endif // _LOG_H
