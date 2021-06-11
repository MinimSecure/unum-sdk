// Copyright 2018 Minim Inc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// unum helper utils platform specific include file

#ifndef _UTIL_H
#define _UTIL_H


// Common util header shared across all platforms
#include "../util_common.h"
// Mutex wrappers
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
// Crash info
#include "../util_crashinfo.h"
// stime removed from glibc 2.31 and later
#include "../util_stime.h"

// This string is the hardware kind. It should be in sync with the server.
// Usually it comes from the Makefile in the gcc invocation options
//#define DEVICE_PRODUCT_NAME "hardware_type_here"

// Graceful reboot command for the platform (if not defined
// the kernel reboot syscall will be used)
#define REBOOT_CMD "/sbin/reboot"

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
#define BASE_MAC_FILE util_base_mac_file_name()

// With this defined, the base MAC address will not be stripped of its
// "local" or "mcast" bits.
#define BASE_MAC_NO_STRIP_LOCAL_MCAST

// Main LAN network device. It is used to get internal IP address
// and capture networking traffic for the default VLAN.
#define PLATFORM_GET_MAIN_LAN_NET_DEV() (util_main_lan_ifname())

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

// Rewritable persistent filesystem folder location where to store the
// agent files
#ifndef PERSISTENT_FS_DIR_PATH
#  define PERSISTENT_FS_DIR_PATH "/var/opt/unum"
#endif

// Time in seconds to wait for LAN & WAN to come up and uci config
// updated to reflect it during the agent (any instance) startup.
// If it does not happen in the given time the initialization
// proceeds anyway.
#define STARTUP_NETWORK_WAIT_TIME 120


// Functions returning main WAN and LAN interface names. The
// values are static strings captured during init. I.e. if
// network names are reconfigured the agent should be restarted.
// Get main WAN interface name, "eth0" if not set
char *util_main_wan_ifname(void);
// Get main LAN interface name, "eth1" if not set
char *util_main_lan_ifname(void);

// Get base MAC file name, it's the file w/ the MAC of the main LAN interface
char *util_base_mac_file_name(void);

#endif // _UTIL_H
