// (c) 2017-2018 minim.co
// unum firmware updater platform specific include file

// Common firmware updater header shared across all platforms
#include "../fw_update_common.h"

#ifndef _FW_UPDATE_H
#define _FW_UPDATE_H

// This define sets the file name where common code saves the
// new firmware file
#define UPGRADE_FILE_NAME "/tmp/fw.bin"

// Define this to make common code run shell command to do the
// firmware upgrade.
#define UPGRADE_CMD "(/sbin/wifi down;"                          \
                    "/sbin/sysupgrade " UPGRADE_FILE_NAME " || " \
                    "/sbin/wifi up)&"

// Time period in seconds to wait after the upgrade script
// returns (regardless of the return status) before assuming
// failure, cleanup and continuing checking for updates again
#define UPGRADE_GRACE_PERIOD 180

// Override default empty download URL suffix since for LEDE we
// have to upgrade using the sysupgrade image, not the full
// size provisioning image for the initial deployments.
#undef FIRMWARE_RELEASE_INFO_URL_SUFFIX
#define FIRMWARE_RELEASE_INFO_URL_SUFFIX "?image=sysupgrade"

#endif // _FW_UPDATE_H

