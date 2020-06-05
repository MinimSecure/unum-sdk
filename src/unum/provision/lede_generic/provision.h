// (c) 2017-2018 minim.co
// unum device provision platform specific include file

// Common device provision header shared across all platforms
#include "../provision_common.h"

#ifndef _PROVISION_H
#define _PROVISION_H


// Device certificate file pathname
#define PROVISION_CERT_PNAME PERSISTENT_FS_DIR_PATH "/unum.pem"

// Device certificate file pathname
#define PROVISION_KEY_PNAME PERSISTENT_FS_DIR_PATH "/unum.key"

// Function re-provisioning the router
static __inline__ void restart_provision(void) {
    unlink(PROVISION_CERT_PNAME);
    unlink(PROVISION_KEY_PNAME);
    util_restart(UNUM_START_REASON_PROVISION);
}

#endif // _PROVISION_H

