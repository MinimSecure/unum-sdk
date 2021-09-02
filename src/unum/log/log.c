// (c) 2017-2020 minim.co
// unum logging subsystem

#include "unum.h"

// Default log control & configuration data structure.
// The defaults for all constants used in this structure are defined
// in log_common.h and can be overridden by the platform. Please refer
// to the log_common.h for more details.
// Note: this structure itself can be overwritten in platform specific
// code (i.e. log_platform.c) to achieve more granualr control.
LOG_CONFIG_t __attribute__((weak)) log_cfg[] = {
[LOG_DST_STDOUT ] = {LOG_FLAG_STDOUT},
[LOG_DST_CONSOLE] = {LOG_FLAG_TTY | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     UNUM_LOG_CONSOLE_NAME},
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

// Default log destination for the process
static int proc_log_dst_id = -1;

// Disabled log destinations mask, by default disable special
// operation mode logs.
static unsigned long disabled_mask =
#ifdef FW_UPDATER_RUN_MODE
  (1 << LOG_DST_UPDATE) |
#endif // FW_UPDATER_RUN_MODE
#ifdef SUPPORT_RUN_MODE
  (1 << LOG_DST_SUPPORT) |
#endif // SUPPORT_RUN_MODE
  0;

// Forward declarations for local static functions
static int init_log_entry(LOG_DST_t dst);


// Set/clear default log destination for the process (overrides LOG_DST macro
// value for the process and all its children). Returns 0 if OK.
int set_proc_log_dst(LOG_DST_t dst)
{
    if(dst < 0 || dst >= LOG_DST_MAX) {
        return -1;
    }
    proc_log_dst_id = dst;
    init_log_entry(dst);

    return 0;
}
void clear_proc_log_dst()
{
    proc_log_dst_id = -1;
}

// Sets disabled log destinations bitmask. Takes bitmask with
// bits set for the logs that should be disabled.
void set_disabled_log_dst_mask(unsigned long mask)
{
    disabled_mask = mask;
}

// Initializes individual log config & control entry
static int init_log_entry(LOG_DST_t dst)
{
    int ret = 0;
    int mask = 0;
    int mutex_taken = FALSE;
    LOG_CONFIG_t *lc = NULL;

    if((disabled_mask & (1 << dst)) != 0 || dst == LOG_DST_DROP) {
        return 0;
    }

    if(dst >= LOG_DST_STDOUT && dst < LOG_DST_MAX) {
        lc = &(log_cfg[dst]);
    }

    // Quick check if we should even try to take mutex
    if((lc->flags & (LOG_FLAG_INIT_DONE | LOG_FLAG_INIT_FAIL)) != 0) {
        return 0;
    }

    if((lc->flags & LOG_FLAG_MUTEX) != 0) {
        UTIL_MUTEX_TAKE(&(lc->m));
        mutex_taken = TRUE;
    }

    for(;;)
    {
        // MT safe check if init is done
        if((lc->flags & (LOG_FLAG_INIT_DONE | LOG_FLAG_INIT_FAIL)) != 0) {
            break;
        }

#ifdef LOG_FLAG_FILE
        // Cleanup stale log files if the log settings change between images
        if(lc->max_size && lc->max && (lc->flags & LOG_FLAG_FILE) != 0)
        {
            int ii;
            for(ii = lc->max + 1; ii <= LOG_ROTATE_CLEANUP_MAX; ii++)
            {
                char fn[LOG_MAX_PATH + 4];
                snprintf(fn, sizeof(fn), "%s/%s.%d",
                         ((*(lc->name) != '/') ? unum_config.logs_dir : ""),
                         lc->name, ii);
                fn[sizeof(fn) - 1] = 0;
                unlink(fn);
            }
        }

        mask |= LOG_FLAG_FILE;
#endif // LOG_FLAG_FILE

#ifdef LOG_FLAG_TTY
        mask |= LOG_FLAG_TTY;
#endif // LOG_FLAG_TTY

        if(!lc->f && (lc->flags & mask) != 0)
        {
            char fn[LOG_MAX_PATH + 1];
            int fn_len;

            // Check for a platform specific runtime defined
            // logs directory
            if(get_platform_logs_dir != NULL) {
                char * logs_dir = get_platform_logs_dir();
                if(logs_dir) {
                    unum_config.logs_dir = logs_dir;
                } else {
                    unum_config.logs_dir = LOG_PATH_PREFIX;
                }
            }

            fn_len =
                snprintf(fn, sizeof(fn), "%s/%s",
                     ((*(lc->name) != '/') ? unum_config.logs_dir : ""),
                     lc->name);
            
            if(fn_len > LOG_MAX_PATH) {
                printf("%s: The log file name <%s> is too long",
                       __func__, lc->name);
                ret = -1;
                break;
            }
            lc->f = fopen(fn, "a+");
            if(lc->f) {
                lc->flags |= LOG_FLAG_INIT_DONE;
            } else {
                printf("%s: failed to open/create <%s>, log init error:\n%s",
                       __func__, fn, strerror(errno));
                lc->flags |= LOG_FLAG_INIT_FAIL;
            }
        }

        // If init is still not done, then either the platform does not support
        // the entry's type of logging or the init has failed.
        if((lc->flags & LOG_FLAG_INIT_DONE) == 0)
        {
            ret = -2;
            break;
        }

        // Log startup message
        if((lc->flags & LOG_FLAG_INIT_MSG) != 0 &&
           (lc->flags & LOG_FLAG_INIT_DONE) != 0)
        {
            // Make sure this call is made only if the init function
            // is already done.
            unum_log(dst, ">>>>> Starting Unum v%s%s%s <<<<<\n",
                     VERSION, unum_config.mode ? " mode: " : "",
                     unum_config.mode ? unum_config.mode : "");
        }

        break;
    }

    if(mutex_taken) {
        UTIL_MUTEX_GIVE(&(lc->m));
    }

    return ret;
}

// Log print function
void unum_log(LOG_DST_t dst, char *str, ...)
{
    LOG_CONFIG_t *lc = NULL;
    int mutex_taken = FALSE;
    long fpos;
    va_list ap;

    va_start(ap, str);

    // If process log destination override is set, use it
    if(proc_log_dst_id >= LOG_DST_STDOUT && proc_log_dst_id < LOG_DST_MAX) {
        dst = proc_log_dst_id;
    }

    if((disabled_mask & (1 << dst)) != 0 || dst == LOG_DST_DROP) {
        return;
    }

    // leave lc == NULL if unknown destination or logging to stdout
    if(dst > LOG_DST_STDOUT && dst < LOG_DST_MAX) {
        lc = &(log_cfg[dst]);
    }

    // If no dst or if logging to stdout printf and leave
    if(!lc || (lc->flags & LOG_FLAG_STDOUT) != 0) {
        vprintf(str, ap);
        return;
    }

    // Make sure the log entry is initialized
    init_log_entry(dst);

    // If logging entry failed to init, just printf and leave
    if((lc->flags & LOG_FLAG_INIT_FAIL) != 0) {
        vprintf(str, ap);
        return;
    }

    // Take mutex if required
    if((lc->flags & LOG_FLAG_MUTEX) != 0) {
        UTIL_MUTEX_TAKE(&(lc->m));
        mutex_taken = TRUE;
    }

    for(;;)
    {
        // Log to console
#ifdef LOG_FLAG_TTY
        if((lc->flags & LOG_FLAG_TTY) != 0) {
            vfprintf(lc->f, str, ap);
            fflush(lc->f);
            break;
        }
#endif // LOG_FLAG_TTY
#ifdef LOG_FLAG_FILE
        if((lc->flags & LOG_FLAG_FILE) != 0)
        {
            struct tm tm;
            time_t now;
            int tid = syscall(SYS_gettid);

            time(&now);
            gmtime_r(&now, &tm);
            fprintf(lc->f, "%d %d.%02d.%02d %02d:%02d:%02d ",
                    tid, 1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
                    tm.tm_hour, tm.tm_min, tm.tm_sec);
            vfprintf(lc->f, str, ap);
            fflush(lc->f);
            // Check the file size and rotate if necessary.
            // Note (TBD): in order to support long messages on platforms where
            // storing into very limited persistent storage is allowed this has
            // to be rewritten to print into temporary buffer, stripping off the
            // excess length of the long messages and then writing to the log.
            // That will also allow to handle duplicate messages.
            fpos = ftell(lc->f);
            if(lc->max_size && fpos >= lc->max_size)
            {
                int ii;
                if(lc->cut_size && fpos > lc->cut_size) {
                    fseek(lc->f, fpos - 3, SEEK_SET);
                    fprintf(lc->f, "...");
                    fflush(lc->f);
                    ftruncate(fileno(lc->f), lc->cut_size);
                }
                fclose(lc->f);
                for(ii = lc->max; ii >= 1; ii--) {
                    char buf_from[LOG_MAX_PATH + 4];
                    char to[LOG_MAX_PATH + 4];
                    char *from;

                    snprintf(buf_from, sizeof(buf_from), "%s/%s",
                            ((*(lc->name) != '/') ? unum_config.logs_dir : ""),
                            lc->name);
                    from = buf_from; // redundant
                    if(ii - 1 > 0) {
                        snprintf(buf_from, sizeof(buf_from), "%s/%s.%d",
                                 ((*(lc->name) != '/') ? unum_config.logs_dir : ""),
                                 lc->name, ii - 1);
                        buf_from[sizeof(buf_from) - 1] = 0;
                        from = buf_from; // redundant
                    }
                    snprintf(to, sizeof(to), "%s/%s.%d", 
                             ((*(lc->name) != '/') ? unum_config.logs_dir : ""),
                             lc->name, ii);
                    to[sizeof(to) - 1] = 0;
                    rename(from, to);
                }
                lc->flags &= ~(LOG_FLAG_INIT_FAIL | LOG_FLAG_INIT_DONE);
                char fn[LOG_MAX_PATH + 1];
                int fn_len =
                    snprintf(fn, sizeof(fn), "%s/%s",
                         ((*(lc->name) != '/') ? unum_config.logs_dir : ""),
                         lc->name);

                if(fn_len > LOG_MAX_PATH) {
                    printf("%s: The log file name <%s> is too long",
                       __func__, lc->name);
                    break;
                }
                lc->f = fopen(fn, "a+");
                if(lc->f) {
                    lc->flags |= LOG_FLAG_INIT_DONE;
                } else {
                    printf("%s: failed to create <%s/%s> after rotation,"
                           "error:%s\n",
                           __func__,
                           ((*(lc->name) != '/') ? unum_config.logs_dir : ""),
                           lc->name, strerror(errno));
                    lc->flags |= LOG_FLAG_INIT_FAIL;
                }
            }
        }
#endif // LOG_FLAG_FILE
        break;
    }

    if(mutex_taken) {
        UTIL_MUTEX_GIVE(&(lc->m));
    }

    return;
}


int log_init(int level)
{
    int ii;

    if(level == INIT_LEVEL_LOGGING) {
        for(ii = LOG_DST_STDOUT; ii < LOG_DST_MAX; ii++) {
            init_log_entry(ii);
        }
    }

    // Always return 0 to avoid app error if any of the log entries init fails
    return 0;
}
