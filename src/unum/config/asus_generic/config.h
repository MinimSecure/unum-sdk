// (c) 2019-2020 minim.co
// config subsystem platform include file

#ifndef _CONFIG_H
#define _CONFIG_H

// MD5
#include <openssl/sha.h>
#include <openssl/md5.h>

// Enable internal config download support
#define CONFIG_DOWNLOAD_IN_AGENT

// We have key-value pairs of this length (the longest one was sshd_hostkey)
#define NVRAM_MAX_STRINGSIZE 1152

// Define the shell command for applying the new config file
// without rebooting the router.
#define APPLY_CONFIG_CMD "/sbin/apply_config.sh"

// Define the shell command for preparing to apply the new config file.
// Stop HTTPD to make sure config is not getting messed up.
#define PRE_APPLY_CONFIG_CMD "/sbin/rc rc_service stop_httpd"

// Maximum time for the (pre)apply config commands to complete (in seconds).
// If not complete within that time the wait is terminated and the shell
// applying config is killed
#define APPLY_CONFIG_MAX_TIME 60

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
