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

// Main unum include file that pulls in everything else

#ifndef _UNUM_H
#define _UNUM_H

#define _GNU_SOURCE

// System includes
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/ptrace.h>
#include <dirent.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <poll.h>
#include <unistd.h>
#include <linux/reboot.h>
#include <linux/filter.h>
#include <linux/if_packet.h>
#include <linux/virtio_net.h>
#include <linux/sockios.h>
#include <linux/rtnetlink.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <jansson.h>
#include <getopt.h>

// Local includes
#include "platform.h"
#include "tracer.h"
#include "util.h"
#include "log.h"
#include "http.h"
#include "fw_update.h"
#include "provision.h"
#include "telemetry.h"
#include "speedtest.h"
#include "activate.h"
#include "cmdproc.h"
#include "support.h"
#include "config.h"
#include "tpcap.h"
#include "devtelemetry.h"
#include "fingerprint.h"
#include "unum_wireless.h"
#include "security.h"
#include "monitor.h"
#include "conncheck.h"

#ifdef DEBUG
#  include "tests.h"
#endif // DEBUG


// Max init level. Upon startup unum iterates through all the init
// levels (1..max) and calls all the init functions for each level.
#define MAX_INIT_LEVEL 7
// Max init level for special modes of operation
#define MAX_INIT_LEVEL_FW_UPDATE 3
#define MAX_INIT_LEVEL_SUPPORT   3
#define MAX_INIT_LEVEL_TESTS     3

// The functions decide what to do at each of the levels.
// Levels currently used by the various subsystem init functions:
#define INIT_LEVEL_LOGGING      1
#define INIT_LEVEL_RAND         1
#define INIT_LEVEL_SETUP        1
#define INIT_LEVEL_THREADS      2
#define INIT_LEVEL_HTTP         3
#define INIT_LEVEL_TIMERS       4
#define INIT_LEVEL_CONNCHECK    5
#define INIT_LEVEL_PROVISION    6
#define INIT_LEVEL_ACTIVATE     6
#define INIT_LEVEL_CMDPROC      6
#define INIT_LEVEL_DEVTELEMETRY 6
#define INIT_LEVEL_TELEMETRY    7
#define INIT_LEVEL_CONFIG       7
#define INIT_LEVEL_TPCAP        7
#define INIT_LEVEL_FINGERPRINT  7
#define INIT_LEVEL_WIRELESS     7

// Single instance check and PID file creation defines.
// How many times to retry check that we run just one instance and
// attempt to create the instance PID file.
#define AGENT_STARTUP_RETRIES 3
// Delay (in seconds) between the retries
#define AGENT_STARTUP_RETRY_DELAY 10
// PID file prefix, full pathname is <AGENT_PID_FILE_PREFIX>-<SUFFIX>.pid where
// suffix is monitor, agent, support, fw_update, etc.
#define AGENT_PID_FILE_PREFIX "/var/run/unum"

// Default agent config path, platforms can override in platform.h.
#ifndef UNUM_CONFIG_PATH
#  define UNUM_CONFIG_PATH "/tmp/unum.conf"
#endif

// Main configuration context (for global startup options and paramters)
typedef struct {
    unsigned int daemon     : 1;   // daemonize after processing options
    unsigned int unmonitored: 1;   // do not run monitor process
    unsigned int no_provision:1;   // no provisioning (no wait for cert/key)
    unsigned int no_conncheck:1;   // no connectivity check
    char *url_prefix;              // custom "http(s)://<host>" URL part
    char *mode;                    // special op. mode <u[pdate]|s[upport]...>
    char *ca_file;                 // trusted CA file for CURL
    char *pid_fprefix;             // PID file prefix, e.g. "/var/run/unum"
    int telemetry_period;          // router telemetry reporting period
    int tpcap_time_slice;          // traffic counters and packet capturing
                                   // time slot interval
    int tpcap_nice;                // tpcap priority adjustment (i.e. niceness)
    int wireless_scan_period;      // neighborhood scan time period
    int wireless_telemetry_period; // radio & clients telemetry reporting
    int support_long_period;       // support connect attempt long period
                                   // not be less than SUPPORT_SHORT_PERIOD
    int fw_update_check_period;    // firmare upgrade check time period
    int sysinfo_period;            // sysinfo reporting time period
    int ipt_period;                // iptables reporting time period
    char *config_path;             // path to config file
    char wan_ifname[IFNAMSIZ];     // specify a custom wan interface name
    char lan_ifname[TPCAP_IF_MAX][IFNAMSIZ];             
                                   // list of custom lan interface names
    int lan_ifcount;               // specified lan interface count
    int wan_ifcount;               // 1 if wan interface was specified
} UNUM_CONFIG_t;

// Startup info (passed by the monitor to the agent at startup to identify
// the reason for the agent start/re-start and in case of the restart due to
// a failure the associated error information)
typedef struct {
#define UNUM_START_REASON_UNKNOWN 0 // power cycle, reset or unknow reason
#define UNUM_START_REASON_CRASH   1 // agent restart due to a crash
#define UNUM_START_REASON_RESTART 2 // self initiated restart
#define UNUM_START_REASON_REBOOT  3 // self initiated reboot (not yet)
#define UNUM_START_REASON_UPGRADE 4 // firmware upgrade (by agent, not yet)
    int code;                // stratup reason codes (see above)
    TRACER_CRASH_INFO_t *ci; // crash info (if UNUM_START_REASON_CRASH)
                             // this pointer (if set) can be freed by agent
                             // after it is spun off the monitor process
} UNUM_START_REASON_t;

// Type of the init function entry point, the return value must be
// 0 if the function succeedes, the parameter is the init level
// (see MAX_INIT_LEVEL above).
typedef int (*INIT_FUNC_t)(int);


// Array of init function pointers (the last enty is NULL)
extern INIT_FUNC_t init_list[];
// Array of init function names (matches the above)
extern char *init_str_list[];

// Unum configuration contex global structure
extern UNUM_CONFIG_t unum_config;

// The structure for capturing startup information (captured by the monitor
// process, monitored process after being forked can examine it and log, report
// to the server or just ignore it)
extern UNUM_START_REASON_t process_start_reason;


// The function parses config from the file or from the server activate
// response. The file (or the data buffer) must be a valid JSON and
// only contain the key-value pairs with the keys same as the long options.
// file_or_data - filename or JSON data (if at_activate is TRUE)
// at_activate - TRUE if passing null-terminated JSON string received from
//               server's activate response, FALSE if passing config file name
int parse_config(int at_activate, char *file_or_data);

// Turn on/off the fallback URL use flag.
void agent_use_fallback_url(int on);

// Check if the fallback URL use flag is on.
int agent_is_fallback_url_on(void);

// Check that the process is not already running and create its PID
// file w/ specified suffix.
// suffix  - PID file suffix ("monitor", "agent", or the op mode name from -m),
//           full pid file name /var/run/unum-<SUFFIX>.pid.
//           The call can be made w/ suffix == NULL. This cleans up state
//           inherited from parent that already called unum_handle_start()
// Returns: 0 - ok, negative - file operation error,
//          positive - already running (the file is locked)
// Note:  It is not thread safe (run it in the main thread of the process only)
// Note1: This function should normally be called for stratup and paired w/
//        unum_handle_stop() call before the exit.
// Note2: If the process is forked and the child might continue running without
//        the parent it should call unum_handle_stop(NULL) to clean up the
//        inhereted from the parent state.
int unum_handle_start(char *suffix);

// Clean up the process PID file (call right before exiting).
// Returns: none
// Note: It is not thread safe (run it in the main thread of the process only)
void unum_handle_stop();

// Function to call to tell the agent main thread to exit.
void agent_exit(int status);

// Agent generic init function (for anything that is not subsystem specific)
int agent_init(int level);

// Agent entry point (called when the agent process is daemonized and
// set up to do its work).
int agent_main(void);

#endif // _UNUM_H
