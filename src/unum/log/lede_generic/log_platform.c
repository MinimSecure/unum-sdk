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
                     UNUM_LOG_CONSOLE_NAME},
[LOG_DST_UNUM   ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "unum.log",
                     UNUM_LOG_SCALE_FACTOR * 32 * 1024,
                     (UNUM_LOG_SCALE_FACTOR * 32 + UNUM_LOG_SCALE_FACTOR * 32
                            / UNUM_LOG_CUT_FRACTION) * 1024,
                     UNUM_LOG_EXTRA_ROTATIONS},
[LOG_DST_HTTP   ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "http.log",
                     UNUM_LOG_SCALE_FACTOR * 32 * 1024,
                     (UNUM_LOG_SCALE_FACTOR * 32 + UNUM_LOG_SCALE_FACTOR * 8
			/ UNUM_LOG_CUT_FRACTION) * 1024,
                     UNUM_LOG_EXTRA_ROTATIONS},
[LOG_DST_MONITOR] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "monitor.log",
                     UNUM_LOG_SCALE_FACTOR * 8 * 1024,
                     (UNUM_LOG_SCALE_FACTOR * 8 + UNUM_LOG_SCALE_FACTOR * 16
			/ UNUM_LOG_CUT_FRACTION) * 1024,
                     -1 + UNUM_LOG_EXTRA_ROTATIONS},
#ifdef FW_UPDATER_RUN_MODE
[LOG_DST_UPDATE ] = {LOG_FLAG_FILE | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "updater.log",
                     UNUM_LOG_SCALE_FACTOR * 8 * 1024,
                     (UNUM_LOG_SCALE_FACTOR * 8 + UNUM_LOG_SCALE_FACTOR * 8
			/ UNUM_LOG_CUT_FRACTION) * 1024,
                     -1 + UNUM_LOG_EXTRA_ROTATIONS},
[LOG_DST_UPDATE_MONITOR] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "updater_monitor.log",
                     UNUM_LOG_SCALE_FACTOR * 8 * 1024,
                     (UNUM_LOG_SCALE_FACTOR * 8 + UNUM_LOG_SCALE_FACTOR * 8
			/ UNUM_LOG_CUT_FRACTION) * 1024,
                     -1 + UNUM_LOG_EXTRA_ROTATIONS},
#endif // FW_UPDATER_RUN_MODE
#ifdef SUPPORT_RUN_MODE
[LOG_DST_SUPPORT] = {LOG_FLAG_FILE | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "support.log",
                     UNUM_LOG_SCALE_FACTOR * 8 * 1024,
                     (UNUM_LOG_SCALE_FACTOR * 8 + UNUM_LOG_SCALE_FACTOR * 8
			/ UNUM_LOG_CUT_FRACTION) * 1024,
                     -1 + UNUM_LOG_EXTRA_ROTATIONS},
#endif // SUPPORT_RUN_MODE
#ifdef DEBUG
[LOG_DST_DEBUG  ] = {LOG_FLAG_FILE | LOG_FLAG_MUTEX | LOG_FLAG_INIT_MSG,
                     UTIL_MUTEX_INITIALIZER,
                     "debug.log",
                     UNUM_LOG_SCALE_FACTOR * 16 * 1024,
                     (UNUM_LOG_SCALE_FACTOR * 16 + UNUM_LOG_SCALE_FACTOR * 16
			/ UNUM_LOG_CUT_FRACTION) * 1024,
                     -1 + UNUM_LOG_EXTRA_ROTATIONS},
#endif // DEBUG
[LOG_DST_MAX    ] = {} // for consistency, does not really need an entry
};
