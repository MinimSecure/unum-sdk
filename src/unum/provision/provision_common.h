// Copyright 2019 - 2020 Minim Inc
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

// unum device provision include file

#ifndef _PROVISION_COMMON_H
#define _PROVISION_COMMON_H

// The first try to provision is random at 0-PROVISION_PERIOD_START)
#define PROVISION_PERIOD_START   0
// After each failure the delay is incremented by random from
// 0-PROVISION_PERIOD_INC range
#define PROVISION_PERIOD_INC    30
// Ater reaching PROVISION_MAX_PERIOD the delay stays there
#define PROVISION_MAX_PERIOD   600

// Subsystem init function
int provision_init(int level);

// This function blocks the caller till device is provisioned.
// If '-p' option is used the function never blocks.
void wait_for_provision(void);

// This function returns TRUE if the device is provisioned, FALSE otherwise.
// Even if '-p' option is used it still returns TRUE only if provisioned
// (maybe from a previous run while -p was not used).
int is_provisioned(void);

// This function returns TRUE if the device done with provisioning.
// If '-p' option is used it always returns TRUE.
int is_provision_done(void);

// If the device is provisioned this function cleans up existent provisioning
// info and terminates the agent with exit code requesting its restart (works
// only w/ monitor process). If the device is not provisioned it has no effect.
// The '-p' option does not affect the behavior of this function.
void destroy_provision_info(void);

// Set "provisioned" (used for debugging only)
void set_provisioned(void);

#endif // _PROVISION_COMMON_H
