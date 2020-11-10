// (c) 2017-2020 minim.co
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
#  define LOG_STDOUT "unum_out.txt"
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
//#define UNUM_LOG_SIZE_BIG_KB 128
//#define UNUM_LOG_SIZE_MEDIUM_KB 64
//#define UNUM_LOG_SIZE_SMALL_KB 32
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
