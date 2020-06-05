// (c) 2017-2019 minim.co
// unum firmware updater include file

#ifndef _FW_UPDATER_COMMON_H
#define _FW_UPDATER_COMMON_H

// firmware upgrade check time period (in sec)
#define FW_UPDATE_CHECK_PERIOD 300
#define FORCE_FW_UPDATE_CHECK_PERIOD 10

// Number of retries if unable to do forced upgrade
#define FORCE_FW_UPDATE_RETRIES 6

// default suffix for download firmware image URL (allows to
// specify extra parameter for requesting specific platfrom
// image), override in the platform header
#define FIRMWARE_RELEASE_INFO_URL_SUFFIX ""

// Firmware updater main entry point
int fw_update_main(void);
void cmd_force_fw_update(void);

#endif // _FW_UPDATER_COMMON_H

