// (c) 2017-2018 minim.co
// config subsystem platform include file

#ifndef _CONFIG_H
#define _CONFIG_H

// MD5 is available in mbedtls library
#include <mbedtls/md5.h>


// Enable internal config download support, comment it out if
// want the config download to be handled by a scripted command
#define CONFIG_DOWNLOAD_IN_AGENT

// For the platforms that require provisioning defaults from the
// server this define points to the location of the file indicating
// that the provisioning is complete. It is used by the config
// subsystem to delay upload of the configuration to the server
// till the defaults are populated.
// The timeout (in seconds) defines max time to wait for the
// flag file before giving up.
// This is only used on crippled platfroms and requires
// 'activate_script' feature to be enabled for the platform.
#define ACTIVATE_FLAG_FILE "/etc/unum/.wifi_provisioned"
#define ACTIVATE_FLAG_FILE_TIMEOUT 60

// Define the shell command for applying the new config file
// without rebooting the router.
#define APPLY_CONFIG_CMD "/usr/bin/restart_config.sh"

// Maximum time for the apply config script to complete (in seconds).
// If not complete within that time the wait is terminated, the shell
// applying config is killed and the device reboot is triggered.
#define APPLY_CONFIG_MAX_TIME 120

// Config debug tracing define that (to enable tracing) should be set to point
// to a path on the device where the config tracing code should store config
// file snapshots for each send/receive operation.
// The config snapshot file name format is:
// <path>/sX_YY.bin
// <path>/rX_YY.bin
// s/r - sent or received
// X - session # (increases with each agent start, one digit, 0-9)
// YY - sequence # in the session (increases with each send or receive)
// <path>/session - session # file
// Note: to track config changes causing reboots use persistent <path>,
//       define this constant in the platform specific config.h header
// Note1: The <path> has to exist for the tracing to work
//#define CONFIG_TRACING_DIR "/tmp/cfgtrace"
//#define CONFIG_TRACING_DIR PERSISTENT_FS_DIR_PATH "/cfgtrace"


// Typedef for config UID on the platform (MD5 hash here).
typedef char CONFIG_UID_t[16];

// Pull in the common include
#include "../config_common.h"

#endif // _CONFIG_H
