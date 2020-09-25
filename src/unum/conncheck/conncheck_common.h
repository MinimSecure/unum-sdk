// (c) 2018-2020 minim.co
// unum connectivity checker include file

#ifndef _CONNCHECK_COMMON_H
#define _CONNCHECK_COMMON_H


// The loop iteration delay
#define CONNCHECK_LOOP_DELAY 5

// Add random startup delay up to CONNCHECK_START_DELAY seconds
#define CONNCHECK_START_DELAY 0

// How long to allow the cloud connectivity to fail before restarting the
// connectivity checker (for now just restart the agent), in sec. This
// constant is also used as the connectivity checker watchdog timeout.
#define CONNCHECK_OFFLINE_RESTART 600

// Max allowed time diff (in sec) before considering it an error and adjust
#define CONNCHECK_MAX_TIME_DIFF (60 * 5)

// Time (in sec) to stay quiet before starting over if unable to connect
// and/or recover. The time is increased at each failure till it becomes
// greater or equal to the max.
#define CONNCHECK_QUIET_TIME_START  30
#define CONNCHECK_QUIET_TIME_INC     5
#define CONNCHECK_QUIET_TIME_MAX   120

// Max time (in sec) to keep tryinig connectivity test before assuming
// that something is wrong and trying to recover. There are 3 types
// of the grace period. The "startup" (it should be enough for the router
// to complete the startup and bring up the WAN), the "recover" (is used
// after attempting to resolve a networking issue) and "retry" (is used
// for subsequent after the startup try-to-connect time periods).
#define CONNCHECK_START_GRACE_TIME   120
#define CONNCHECK_RECOVER_GRACE_TIME 15
#define CONNCHECK_RETRY_GRACE_TIME   30

// Amount of time to allow to pass between counter snapshots when checking
// Ethernet device activity (in sec)
#define CONNCHECK_TB_COUNTERS_WAIT 15

// How many times to try pinging the default gateway and the ping
// timeout (in sec)
#define CONNCHECK_GW_PINGS 5
#define CONNCHECK_GW_PINGS_TIMEOUT 5

// Flag file indicating that connectivity checker suspects
// DNS is not functioning
#define CONNCHECK_NO_DNS_FILE "/var/unum_dns_error"

// How many times to try DNS test (retrying only on prtial success)
// Note: it doesn't help much to retry more than once, that one
//       retry allows calling res_init() that helps in some cases
#define CONNCHECK_DNS_TRIES 2


// IDs for conncheck state (cstate)
typedef enum _CONNCHECK_STATES {
    CSTATE_UNINITIALIZED,    // 0 - not initialized
    CSTATE_CONNECTING,       // 1 - get server urls via DNS and connect to cloud
    CSTATE_RECOVERY,         // 3 - connecton recovery
    CSTATE_CHECKING_IPV4,    // 4 - checking IPv4 connectivity
    CSTATE_CHECKING_DNS,     // 5 - checking DNS
    CSTATE_CHECKING_HTTPS,   // 6 - checking HTTPS
    CSTATE_CHECKING_COUNTERS,// 7 - checking packet counters
    CSTATE_DONE,             // 8 - done, info ready
    CSTATE_MAX               // max
} CONNCHECK_STATES_t;

// Structure for storing connectivity check status
typedef struct _CONNCHECK_ST {
    // flags
    unsigned int can_connect:   1; // agent can establish HTTPS connection
    unsigned int http_connect:  1; // agent can connect over HTTP
    unsigned int use_minim_time:1; // NTP problems, use time from the cloud
    unsigned int set_time_error:1; // NTP problems, can't set cloud time
    unsigned int no_interface:  1; // no network interface to work with
    unsigned int no_counters:   1; // unable to get interface counters
    unsigned int no_rx_pkts:    1; // interface is not receiving packets
    unsigned int no_tx_pkts:    1; // interface is not sending packets
    unsigned int rx_errors:     1; // rx errors detected on the interface
    unsigned int tx_errors:     1; // tx errors detected on the interface
    unsigned int no_ipv4:       1; // unable to get interface IPv4 address
    unsigned int no_ipv4_gw:    1; // unable to get IPv4 default gateway info
    unsigned int bad_ipv4_gw:   1; // invalid IPv4 default gateway address
    unsigned int no_dns:        1; // no DNS, use hardcoded IPs for cloud access
    unsigned int no_servers:    1; // no servers, couldn't get servers from dns
    unsigned int slist_ready:   1; // server list can be used (this can happen
                                   // if we got it from DNS or gave up and let
                                   // the code use the defaults, see no_servers)
    // data
    unsigned long cloud_utc;       // UTC time in sec from the cloud
    unsigned long connect_uptime;  // uptime when connected
    unsigned long tb_start_uptime; // uptime when troubleshooting started
    char ifname[IFNAMSIZ];         // interface name to troubleshoot
    DEV_IP_CFG_t ipcfg;            // interface IP configuration
    IPV4_ADDR_t ipv4gw;            // IPv4 default gateway address
    int gw_ping_err_ratio;         // % of failed default gateway pings
    int gw_ping_rsp_time;          // default gateway ping avg response in ms
    char dns_fails[256];           // comma-separated non-resolving DNS names
} CONNCHECK_ST_t;


// Restart connectivity check and recovery
void restart_conncheck(void);

// Returns true if agent subsystems have to use IP address for connecting
// to the cloud server (instead of DNS name)
int conncheck_no_dns(void);

// Returns true if agent detected bad time and had to set it to the
// cloud time.
// Note: this only works in the context of the process running the
//       conncheck
int conncheck_no_time(void);

// Subsystem init function
int conncheck_init(int level);

// This function blocks the caller till the connectivity check is complete.
// If '-n' option is used the function never blocks.
void wait_for_conncheck(void);

#ifdef DEBUG
void test_conncheck(void);
#endif // DEBUG

#endif // _CONNCHECK_COMMON_H

