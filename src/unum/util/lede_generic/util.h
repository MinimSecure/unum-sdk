// (c) 2017-2020 minim.co
// unum helper utils platform specific include file

#ifndef _UTIL_H
#define _UTIL_H


// Common util header shared across all platforms
#include "../util_common.h"
// Mutex wrappers (using platfrom specific implementation
// since not all LEDE platforms have static initializer for
// recursive mutex)
#include "./util_mutex_platform.h"
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

// Commands to execute to factory reset the router
#define FACTORY_RESET_CMD "/sbin/firstboot -y ; " REBOOT_CMD

// Script to get AUTH INFO
// This returns the auth info key.
// Exact value depends on the platform
// For B1300, it is serial number
#define AUTH_INFO_GET_CMD "/sbin/get_auth_info_key.sh"
// ToDo: If needed we need to have separate lengths for each LEDE platform
#define AUTH_INFO_KEY_LEN 32

#define SERIAL_NUM_GET_CMD "/sbin/get_serial_num.sh"
// ToDo: If needed we need to have separate lengths for each LEDE platform
#define SERIAL_NUM_LEN 32

// Define for the platforms where the agent version is used
// as the firmware version # for identification and upgrade
// purposes. The platforms that do not use agent version here
// should implement platform_fw_version() function.
//#define AGENT_VERSION_IS_FW_VERSION

// Base MAC file (name of the file to read the device base
// MAC address from). This should be the MAC address that never
// changes (i.e. not the WAN MAC for devices that have it
// configurable). If there is no such file for the platform
// it should implement its own platform_device_mac() function.
#define BASE_MAC_FILE util_base_mac_file_name()

// Main LAN network device. It is used to get internal IP address
// and capture networking traffic for the default VLAN.
#define PLATFORM_GET_MAIN_LAN_NET_DEV() (util_main_lan_ifname())

// This macro returns the name of the guest interface the agent will monitor
// if the interface exists and is assigned an IP address.
#define PLATFORM_GET_GUEST_LAN_NET_DEV() "br-guest"

// This macro generayes the name of the additional 1..N VLAN interface the agent
// will monitor if the interface exists and assigned an IP address.
// For lede_generic we check for 2 more VLAN and the guest interface besides the
// main LAN one.
#define PLATFORM_GET_AUX_LAN_NET_DEV(num) ("br-lan" #num)

// Main WAN network device. So far expecting only one WAN device
// per router. It is used to get device external IP.
#define PLATFORM_GET_MAIN_WAN_NET_DEV() (util_main_wan_ifname())

// Max stack size for the platform threads (unless the system
// default is not good)
//#define JOB_MAX_STACK_SIZE (2 * 1024 *1024)

// The format string for scanning /proc/net/dev for the platform
// and the minimum number of values sscanf should return to accept it.
#define UTIL_NET_DEV_CNTRS_FORMAT \
  "%llu%llu%lu%lu%lu%lu%lu%lu%llu%llu%lu%lu%lu%lu%lu%lu"
#define UTIL_NET_DEV_CNTRS_FORMAT_COUNT 16

// Rewritable persistent filesystem folder location (for the platfroms
// that support persistent storage) where to store the agent files
#define PERSISTENT_FS_DIR_PATH "/etc/unum"

// File which contains the device id (mostly MAC Address read from flash)
#define DEVICEID_FILE "/var/tmp/deviceid.txt"

// Time in seconds to wait for LAN & WAN to come up and uci config
// updated to reflect it during the agent (any instance) startup.
// If it does not happen in the given time the initialization
// proceedes anyway.
#define STARTUP_NETWORK_WAIT_TIME 120

// Functions returning main WAN and LAN interface names. The
// values are static strings captured during init. I.e. if
// network names are reconfigured the agent should be restarted.
// Get main WAN interface name, NULL if not yet set
char *util_main_wan_ifname(void);
// Get main LAN interface name, NULL if not yet set
char *util_main_lan_ifname(void);

// The function returns the firmware version from /etc/openwrt_release
// file. The version string is cached on the first run and is returned
// back from the cache on the subsequent calls. In the case of an error
// the harcoded "unknown" string is returned.
char *platform_fw_version(void);

// Get base MAC file name, it's the file w/ the MAC of the main LAN interface
char *util_base_mac_file_name(void);
// Get Radio kind
// Returns Radio kind
// -1 if the Radio name is not found
int util_get_radio_kind(char *ifname);

#endif // _UTIL_H
