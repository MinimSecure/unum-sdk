// (c) 2019 minim.co
// unum firmware updater platform specific include file

// Common firmware updater header shared across all platforms
#include "../fw_update_common.h"

#ifndef _FW_UPDATE_H
#define _FW_UPDATE_H

// This define sets the file name where common code saves the
// new firmware file
#define UPGRADE_FILE_NAME "/tmp/linux.trx"

// Define this to make common code run shell command to do the
// firmware upgrade.
#define UPGRADE_CMD "/usr/bin/fw_upgrade.sh " UPGRADE_FILE_NAME

// Time period in seconds to wait after the upgrade script
// returns (regardless of the return status) before assuming
// failure, cleanup and continuing checking for updates again
#define UPGRADE_GRACE_PERIOD 360

#endif // _FW_UPDATE_H

