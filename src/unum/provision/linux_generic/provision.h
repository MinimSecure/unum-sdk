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

