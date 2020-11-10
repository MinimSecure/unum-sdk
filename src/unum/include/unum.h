// (c) 2017-2020 minim.co
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
#include <linux/types.h>
#include <linux/reboot.h>
#include <linux/filter.h>
#include <linux/if_packet.h>
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

#ifdef USE_OPEN_SSL
#  include <openssl/crypto.h>
#  include <openssl/sha.h>
#else
#  include <mbedtls/sha256.h>
#endif

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

// Macro for checking the agent current operation mode flags
#define IS_OPM(feature) (((feature) & unum_config.opmode_flags) != 0)

// Default agent config path, platforms can override in platform.h.
#ifndef UNUM_CONFIG_PATH
#  ifdef PERSISTENT_FS_DIR_PATH
#    define UNUM_CONFIG_PATH PERSISTENT_FS_DIR_PATH "/unum.conf"
#  else // PERSISTENT_FS_DIR_PATH
#    define UNUM_CONFIG_PATH "/etc/unum.conf"
#  endif // PERSISTENT_FS_DIR_PATH
#endif // UNUM_CONFIG_PATH

// Default agent logs path, platforms can override in platform.h.
// The path should be slash-terminated.
#ifndef LOG_PATH_PREFIX
#  define LOG_PATH_PREFIX "/var/log/"
#endif // LOG_PATH_PREFIX

// Main configuration context (for global startup options and paramters)
typedef struct {
    unsigned int daemon     : 1;   // daemonize after processing options
    unsigned int unmonitored: 1;   // do not run monitor process
    unsigned int no_provision:1;   // no provisioning (no wait for cert/key)
    unsigned int no_conncheck:1;   // no connectivity check
    char *url_prefix;              // custom "http(s)://<host>" URL part
    char *mode;                    // special run mode <u[pdate]|s[upport]...>
#define UNUM_OPMS_MD          "md" // managed device mode
#define UNUM_OPMS_AP          "ap" // AP mode
#define UNUM_OPMS_GW          "gw" // gateway mode
#define UNUM_OPMS_MESH_11S_AP "mesh_11s_ap" // AP in ieee802.11s mesh mode
#define UNUM_OPMS_MESH_11S_GW "mesh_11s_gw" // gateway in ieee802.11s mesh mode
#define UNUM_OPMS_MESH_QCA_AP "mesh_qca_ap" // QCA Mesh AP
#define UNUM_OPMS_MESH_QCA_GW "mesh_qca_gw" // QCA Mesh GW
#define UNUM_OPMS_MTK_EM_AP   "mesh_em_ap"  // MTK EasyMesh AP
#define UNUM_OPMS_MTK_EM_GW   "mesh_em_gw"  // MTK EasyMesh GW
    char *opmode;                  // agent opmode string (above) <gw|ap|...>
#define UNUM_OPM_AUTO     0x01 // can pick operating mode automatically
#define UNUM_OPM_GW       0x02 // operating in gateway mode
#define UNUM_OPM_AP       0x04 // operating in wireless AP mode
#define UNUM_OPM_MD       0x08 // operating in managed device mode
#define UNUM_OPM_MESH_11S 0x10 // operating in ieee802.11s mesh mode
#define UNUM_OPM_MESH_QCA 0x20 // operating in QCA mesh mode
#define UNUM_OPM_MTK_EM   0x40 // operating in MTK EasyMesh mode
    int opmode_flags;              // opmode flags (above, used by IS_OPM(...))
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
    char *logs_dir;                // path to logs directory (used for saving
                                   // logs across reboots, for troubleshooting)
    char *cfg_trace;               // path where to store config changes tracing
                                   // files (for troubleshooting config changes)
    int dns_timeout;               // dns timeout value in seconds
#ifdef FEATURE_GZIP_REQUESTS
    int gzip_requests;             // threshold beyond which the request is
                                   // to be compressed
#endif // FEATURE_GZIP_REQUESTS
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

// The init level that we have executed all the init routines for
extern int init_level_completed;


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
