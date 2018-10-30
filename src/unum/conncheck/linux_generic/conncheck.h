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

#endif // _CONNCHECK_H

