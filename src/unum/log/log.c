// (c) 2017-2019 minim.co
// unum logging subsystem

#include "unum.h"

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

// Get Full path for log file name
// Prepend persistent directory (if configured so) path
// to lc->name (if exists) and copy to log_file.
// If there are any issues with prepending the persistent path,
// log_file will be populated with default path
static int get_log_file_name(LOG_CONFIG_t *lc, char *log_file)
{
    char prefix[LOG_MAX_PATH];
    char log_file_copy[LOG_MAX_PATH]; // We need this while using dirname
    char *newline;
    char *dir;

    memset(log_file, 0, LOG_MAX_PATH);
    if (lc->name == NULL) {
        // No logfile needed
        // Eg. STDOUT
        return 1;
    }
    strncpy(log_file, lc->name, LOG_MAX_PATH);

    if (lc->flags & LOG_FLAG_TTY) {
        // For console
        return 2;
    }
    // Check if the prefix file exists
    if (!util_path_exists(LOG_PREFIX_FILE)) {
        // Prefix file does n't exist. Use defaults.
        strncpy(log_file, lc->name, LOG_MAX_PATH - 1);
        return 0;
    }
    // Read the Prefix file content and if everything goes well, this needs
    // to be prepeneded to log file names.
    if (util_file_to_buf(LOG_PREFIX_FILE, prefix, LOG_MAX_PATH) < 0) {
        // Error while reading prefix file
        // Use defaults
        strncpy(log_file, lc->name, LOG_MAX_PATH - 1);
        return -1;
    }

    // util_file_to_buf seems to be appending \n at the end .
    // Remove it.
    newline = strchr(prefix, '\n');
    if (newline != NULL) {
        prefix[newline - prefix] = 0;
    }

    // Copy the prefix and then the file path into log_file.
    if ((strlen(lc->name) + strlen(prefix) + 1) <
                    LOG_MAX_PATH) {
        snprintf(log_file, LOG_MAX_PATH - 1, "%s/%s", prefix, lc->name);
    }

    // Use dirname to get the full direcotry path.
    // Need to create the full direcotry if it does n't exist.
    // dirname seems to be manipulating the argument.
    // So make a copy and use it.
    memset(log_file_copy, 0, LOG_MAX_PATH);
    strncpy(log_file_copy, log_file, LOG_MAX_PATH);
    dir = dirname(log_file_copy);
    if (dir == NULL) {
        // Some error. Use default path for logging
        strncpy(log_file, lc->name, LOG_MAX_PATH - 1);
        return -2;
    }

    // If the directories for the log_file (say /unum/var/log/unum.log)
    // do not exist, create them.
    if (!util_path_exists(dir)) {
        if (util_mkdir_rec(dir, 0700) < 0) {
            // Error while creating recursive directories
            // Use defaults
            strncpy(log_file, lc->name, LOG_MAX_PATH - 1);
        }
    }

    return 0;
}

// Initializes individual log config & control entry
static int init_log_entry(LOG_DST_t dst)
{
    int ret = 0;
    int mask = 0;
    int mutex_taken = FALSE;
    LOG_CONFIG_t *lc = NULL;
    char log_file[LOG_MAX_PATH];

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
                snprintf(fn, sizeof(fn), "%s.%d", lc->name, ii);
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
            if(strlen(lc->name) > LOG_MAX_PATH) {
                printf("%s: The log file name <%s> is too long",
                       __func__, lc->name);
                ret = -1;
                break;
            }
            get_log_file_name(lc, log_file);
            lc->f = fopen(log_file, "a+");
            if(lc->f) {
                lc->flags |= LOG_FLAG_INIT_DONE;
            } else {
                printf("%s: failed to open/create <%s>, log init error:\n%s",
                       __func__, log_file, strerror(errno));
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
    char log_file[LOG_MAX_PATH];

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
    if(!lc) {
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

                    from = lc->name;
                    if(ii - 1 > 0) {
                        snprintf(buf_from, sizeof(buf_from), "%s.%d", lc->name, ii - 1);
                        buf_from[sizeof(buf_from) - 1] = 0;
                        from = buf_from;
                    }
                    snprintf(to, sizeof(to), "%s.%d", lc->name, ii);
                    to[sizeof(to) - 1] = 0;
                    rename(from, to);
                }
                lc->flags &= ~(LOG_FLAG_INIT_FAIL | LOG_FLAG_INIT_DONE);
                get_log_file_name(lc, log_file);
                lc->f = fopen(log_file, "a+");
                if(lc->f) {
                    lc->flags |= LOG_FLAG_INIT_DONE;
                } else {
                    printf("%s: failed to create <%s> after rotation, error:\n%s",
                           __func__, log_file, strerror(errno));
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
