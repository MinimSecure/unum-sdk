// (c) 2019-2020 minim.co
// unum helper utils platform specific include file

#ifndef _UTIL_H
#define _UTIL_H


// Common util header shared across all platforms
#include "../util_common.h"
// Mutex wrappers
#include "../util_mutex.h"
// Event wrappers
#include "../util_event.h"
// Jobs
#include "../jobs.h"
// Timers
#include "../util_timer.h"
// Networking
#include "../util_net.h"
// JSON
#include "../util_json.h"
// Crash handling
#include "../util_crashinfo.h"


// This string is the hardware kind. It should be in sync with the server.
// Usually it comes from the Makefile in the gcc invocation options
//#define DEVICE_PRODUCT_NAME "hardware_type_here"

// Graceful reboot command for the platform (if not defined
// the kernel reboot syscall will be used)
#define REBOOT_CMD "/sbin/reboot"

// Rewritable persistent filesystem folder location (for the platfroms
// that support persistent storage) where to store the agent files
#define PERSISTENT_FS_DIR_PATH "/jffs"

// Commands to execute to factory reset the router
// We use util_platform_factory_reset() platform function
//#define FACTORY_RESET_CMD "nvram erase; " REBOOT_CMD

// Define for the platforms where the agent version is used
// as the firmware version # for identification and upgrade
// purposes. The platforms that do not use agent version here
// should implement platform_fw_version() function.
#define AGENT_VERSION_IS_FW_VERSION

// Base MAC file (name of the file to read the device base
// MAC address from). This should be the MAC address that never
// changes (i.e. not the WAN MAC for devices that have it
// configurable). If there is no such file for the platform
// it should implement its own platform_device_mac() function.
#define BASE_MAC_FILE "/sys/class/net/eth1/address"

// Main LAN network device. It is used to get internal IP address
// and capture networking traffic for the default VLAN.
#define PLATFORM_GET_MAIN_LAN_NET_DEV() "br0"

// Main WAN network device. So far expecting only one WAN device
// per router. It is used to get device external IP.
#define PLATFORM_GET_MAIN_WAN_NET_DEV() "eth0"

// Max stack size for the platform threads (unless the system
// default is not good)
//#define JOB_MAX_STACK_SIZE (2 * 1024 *1024)

// The format string for scanning device counters for the platform
// and the minimum number of values sscanf should return to accept it.
#define UTIL_NET_DEV_CNTRS_FORMAT \
  "%llu%llu%lu%lu%lu%lu%lu%lu%llu%llu%lu%lu%lu%lu%lu%lu"
#define UTIL_NET_DEV_CNTRS_FORMAT_COUNT 16
// Get Interface kind
// Parameters:
// ifname - The name of the interface
int util_get_interface_kind(char *ifname);
// Get Radio kind
// Returns Radio kind
// -1 if the Radio name is not found
int util_get_radio_kind(char *ifname);

#endif // _UTIL_H
