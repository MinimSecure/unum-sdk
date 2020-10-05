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

// A file containing the Prefix(directory)
// All log files (For example unum.log)
// will be prefixed with the content in this filename
#ifdef PERSISTENT_FS_DIR_PATH
#define LOGS_PREFIX_FILE PERSISTENT_FS_DIR_PATH"/unum_prefix_file.txt"
#endif

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
