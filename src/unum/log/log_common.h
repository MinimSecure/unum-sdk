// (c) 2017-2019 minim.co
// Unum logging subsystem include

#ifndef _LOG_COMMON_H
#define _LOG_COMMON_H

// Default log output destinations for log() and log_dbg() macros.
// It can be redefined to make all the log() macros in a file point
// to the different log destination. See also set_log_dst_*() functions.
#define LOG_DST     LOG_DST_UNUM
#define LOG_DBG_DST LOG_DST_CONSOLE /*LOG_DST_DEBUG*/

// Logging macros (see note about LOG_DST/LOG_DBG_DST above)
#define log(args...) unum_log(LOG_DST, args)
#ifdef DEBUG
#  define log_dbg(args...) unum_log(LOG_DBG_DST, args)
#else // DEBUG
#  define log_dbg(args...) /* Nothing */
#endif // DEBUG

// Max log file pathsname length the code will deal with
#define LOG_MAX_PATH 128

// Console device (default, override in platform header if needed)
#define UNUM_LOG_CONSOLE_NAME "/dev/console"

// The below defaults determine log file sizes for big, medium and small
// logs. The platform can override those and set its own limits.
// The platform can also override the whole log_cfg[] if the default categories
// (big, medium, small) are not sufficient.
#define UNUM_LOG_SIZE_BIG_KB 128
#define UNUM_LOG_SIZE_MEDIUM_KB 64
#define UNUM_LOG_SIZE_SMALL_KB 32
// Maximum extra space for the log file to grow after it exceedes the limit,
// but before the messsage being written is cut off
#define UNUM_LOG_CUT_EXTRA_KB 12
// Default number of files to keep when rotating logs after max size is reached
// (similarly to log size limits split into categories - high and low)
#define UNUM_LOG_ROTATIONS_HIGH 2
#define UNUM_LOG_ROTATIONS_LOW 1
// Any of the above constants can be undefined and then changed in platform
// specific log.h file, for example
// #undef UNUM_LOG_SIZE_BIG_KB then #define UNUM_LOG_SIZE_BIG_KB 256

// The below constant should be defined (in platform's log.h) if the platform
// allows for --log-dir option changing default location of the log files.
// The intended use is temporary relocation to the persistent filesystem for
// debugging the agent behavior across reboots. Relocation works for all the
// file paths in log_cfg[] structure that do not start with '/'.
//#define UNUM_LOG_ALLOW_RELOCATION

// Note: the platforms are expected to provide LOG_PATH_PREFIX define
//       pointing to their default logs directory, but in case it's missing
//       the default is set to "/var/log" in unum.h


// Enum of available log output destinations
typedef enum {
    LOG_DST_STDOUT,  // logging to stdout (should be first)
    LOG_DST_CONSOLE, // logging to the serial console
    LOG_DST_UNUM,    // generic agent logging to a file
    LOG_DST_HTTP,    // HTTP req/rsp logging to a file
    LOG_DST_MONITOR, // monitor process logging to a file
#ifdef FW_UPDATER_RUN_MODE
    LOG_DST_UPDATE,  // firmware updater process, file
    LOG_DST_UPDATE_MONITOR, // updater monitor, file
#endif // FW_UPDATER_RUN_MODE
#ifdef SUPPORT_RUN_MODE
    LOG_DST_SUPPORT, // support portal process, file
#endif // SUPPORT_RUN_MODE
#ifdef DEBUG
    LOG_DST_DEBUG,   // debug logging to a file
#endif // DEBUG
    LOG_DST_DROP,    // drop the messages (keep it last)
    LOG_DST_MAX
} LOG_DST_t;

// Patname template for building log file names when rotating. The
// sprintf() parameters are the log name and the integer file number.
#define LOG_ROTATE_TEMPLATE "%s.%d"

// Log rotation cleanup number (if log setup changes from
// X to Y, and Y < X log_init will clean up all the old
// log names from Y to LOG_ROTATE_CLEANUP_MAX)
#define LOG_ROTATE_CLEANUP_MAX 9

// The 16 lower bits are common flags. The upper 16bits are
// platform specific flags (LOG_FLAG_FILE, LOG_FLAG_STDOUT...
// see log_platform.h).
#define LOG_FLAG_INIT_DONE 0x0001 // The entry has been initialized
#define LOG_FLAG_INIT_FAIL 0x0002 // The log entry init has failed
#define LOG_FLAG_MUTEX     0x0004 // The entry requre mutex protextion
#define LOG_FLAG_INIT_MSG  0x0008 // Log the agent startup info at init

// Logging control & configurtion structure (defines the elements of
// the logging config array of LOG_DST_MAX size).
typedef struct {
    unsigned int flags;// see LOG_FLAG_* above
    UTIL_MUTEX_t m;    // mutex protection (across threads, not processes)
    char *name;        // for the "file" mode file pathname template
    size_t max_size;   // rotate file if reaches the max_size
    size_t cut_size;   // cut to the size (in case exceeding max_size too much)
    unsigned int max;  // files to keep when rotating (in addition to the log)
    FILE *f;           // current log file pointer
} LOG_CONFIG_t;

// Logging configuration and control structure (see log_platform.c)
extern LOG_CONFIG_t log_cfg[];

// Optional platform function to get runtime logs dir
char * __attribute__((weak)) get_platform_logs_dir(void);

// Log print function
void unum_log(LOG_DST_t dst, char *str, ...);
// Set/clear default log destination for the process (until cleared overrides
// LOG_DST macro value for the process and its children)
int set_proc_log_dst(LOG_DST_t dst);
void clear_proc_log_dst();
// Set disabled log destinations bitmask. Takes bitmask with
// bits for the logs that should be disabled set.
void set_disabled_log_dst_mask(unsigned long mask);
// Log subsystem init function
int log_init(int level);

#endif // _LOG_COMMON_H
