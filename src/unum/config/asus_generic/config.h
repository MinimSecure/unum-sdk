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


// Typedef for config UID on the platform (MD5 hash here).
typedef char CONFIG_UID_t[16];

// Pull in the common include
#include "../config_common.h"

#endif // _CONFIG_H
