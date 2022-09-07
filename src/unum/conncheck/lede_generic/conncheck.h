// (c) 2018 minim.co
// unum connectivity checker platform specific include file

// Common connectivity checker header shared across all platforms
#include "../conncheck_common.h"

#ifndef _CONNCHECK_H
#define _CONNCHECK_H

// Location of the mobile APP info file that provides LAN MAC (and maybe
// more info coming from activate) to the mobile app during its initial
// deployment/provisioning.
#define LAN_INFO_FOR_MOBILE_APP "/tmp/provision_info.json"
#define LAN_INFO_FOR_MOBILE_APP_TMP "/tmp/provision_info.json.tmp"

#define CONNCHECK_STATUS_REPORT_SCRIPT "/sbin/unum_status_report.sh"
#define CONNCHECK_STATUS_REPORT_SCRIPT_TIMEOUT 10

#endif // _CONNCHECK_H

