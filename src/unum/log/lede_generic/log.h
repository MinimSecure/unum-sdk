// (c) 2017-2018 minim.co
// Unum logging subsystem platform specifc include

// Log header shared across all platforms
#include "../log_common.h"

#ifndef _LOG_H
#define _LOG_H

// Define prefix for log files location. By default use /var/log
// which typically points to RAM FS. Unless the device has a flash
// chip that can sustain high number of erase cycles keep it default.
#define LOG_PATH_PREFIX "/var/log"

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
#define UNUM_LOG_UNUM_MAX_SIZE       64  // KB
#define UNUM_LOG_UNUM_CUT_SIZE       140 // KB
#define UNUM_LOG_UNUM_MAX_FILES      2
#define UNUM_LOG_HTTP_MAX_SIZE       128 // KB
#define UNUM_LOG_HTTP_CUT_SIZE       140 // KB
#define UNUM_LOG_HTTP_MAX_FILES      1
#define UNUM_LOG_MONI_MAX_SIZE       32  // KB
#define UNUM_LOG_MONI_CUT_SIZE       48  // KB
#define UNUM_LOG_MONI_MAX_FILES      1
#define UNUM_LOG_DBG_MAX_SIZE        64  // KB
#define UNUM_LOG_DBG_CUT_SIZE        76  // KB
#define UNUM_LOG_DBG_MAX_FILES       1

#endif // _LOG_H
