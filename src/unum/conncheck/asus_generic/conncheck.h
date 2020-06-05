// (c) 2019 minim.co
// unum connectivity checker platform specific include file

// Connectivity checker common header shared across all platforms
#include "../conncheck_common.h"

#ifndef _CONNCHECK_H
#define _CONNCHECK_H

// Location of the mobile APP info file that provides LAN MAC (and maybe
// more info coming from activate) to the mobile app during its initial
// deployment/provisioning.
#define LAN_INFO_FOR_MOBILE_APP "/var/provision_info.htm"
#define LAN_INFO_FOR_MOBILE_APP_TMP "/var/provision_info.htm.tmp"

#endif // _CONNCHECK_H

