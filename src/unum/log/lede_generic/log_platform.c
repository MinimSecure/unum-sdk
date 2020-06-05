// (c) 2017-2019 minim.co
// Platfrom specific unum logging subsystem code

#include "unum.h"

// The log control & configuration for the platform
// Note: we can log to flash FS to save logs over reboot for LEDE, but that
//       might not be a good option for all the devices as it wears the
//       flash out (depends on the individual device flash)

LOG_CONFIG_t log_cfg[] = {
[LOG_DST_STDOUT ] = {LOG_FLAG_STDOUT},
[LOG_DST_CONSOLE] = {LOG_FLAG_TTY | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "/dev/console"},
[LOG_DST_UNUM   ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     LOG_PATH_PREFIX "/unum.log", 128*1024, 140*1024, 2},
[LOG_DST_HTTP   ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     LOG_PATH_PREFIX "/http.log", 128*1024, 140*1024, 2},
[LOG_DST_MONITOR] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     LOG_PATH_PREFIX "/monitor.log", 32*1024, 48*1024, 1},
#ifdef FW_UPDATER_RUN_MODE
[LOG_DST_UPDATE ] = {LOG_FLAG_FILE | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     LOG_PATH_PREFIX "/updater.log", 32*1024, 48*1024, 1},
[LOG_DST_UPDATE_MONITOR] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "/var/log/updater_monitor.log", 32*1024, 48*1024, 1},
#endif // FW_UPDATER_RUN_MODE
#ifdef SUPPORT_RUN_MODE
[LOG_DST_SUPPORT] = {LOG_FLAG_FILE | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     LOG_PATH_PREFIX "/support.log", 32*1024, 48*1024, 1},
#endif // SUPPORT_RUN_MODE
#ifdef DEBUG
[LOG_DST_DEBUG  ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     LOG_PATH_PREFIX "/debug.log", 64*1024, 76*1024, 1},
#endif // DEBUG
[LOG_DST_MAX    ] = {} // for consistency, does not really need an entry
};
