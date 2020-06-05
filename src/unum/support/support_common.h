// (c) 2017-2018 minim.co
// support portal agent include file

#ifndef _SUPPORT_COMMON_H
#define _SUPPORT_COMMON_H

// Support connect attempt long time period (in sec)
#define SUPPORT_LONG_PERIOD 300
// Support connect attept short time period (in sec)
#define SUPPORT_SHORT_PERIOD 10
// How many times to try with short period after the startup
// delay and if the support session was requested.
#define SUPPORT_SHORT_PERIOD_TRIES 3
// Support connect on startup delay (in sec)
// The fixed part should be chosen to go slightly over the typical
// time to get link up during the startup. The random allows to
// lower the chance of large number of booting devices to flood
// the support portal servers.
// Set both to zero to prevent connecting to the support portal even
// on startup.
#define SUPPORT_STARTUP_DELAY_FIXED 30
#define SUPPORT_STARTUP_DELAY_RANDOM 30

// Max time ssh process is allowed to run (in sec)
#define SUPPORT_CONNECT_TIME_MAX (60 * 60)

// Support portal hostname and fallback IP for DNS errors recovery
// (make sure both are listed in .../files/ssh_keys/known_hosts)
#define SUPPORT_PORTAL_HOSTNAME "support.minim.co"
#define SUPPORT_PORTAL_FB_IP "52.7.196.251"

// This define sets the suffix of the PID file for the shell
// running the support ssh process. See AGENT_PID_FILE_PREFIX
// for the prefix and more info.
#define SUPPORT_PID_FILE_SFX "support-session"


// Support portal agent main entry point
int support_main(void);

// Create the support flag file
void create_support_flag_file(void);
#endif // _SUPPORT_COMMON_H

