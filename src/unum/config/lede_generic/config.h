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

// Typedef for config UID on the platform (MD5 hash here).
typedef char CONFIG_UID_t[16];

// Pull in the common include
#include "../config_common.h"

#endif // _CONFIG_H
