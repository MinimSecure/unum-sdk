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

// device telemetry collection and reporting

#ifndef _DEVTELEMETRY_H
#define _DEVTELEMETRY_H

// Pull in the common include
#include "../devtelemetry_common.h"

// Note: all the below limits are for the information
// collected during single telemetry time period

// Max number of devices
#define DTEL_MAX_DEV 256

// Max number of connections (total for all devices)
#define DTEL_MAX_CONN 4096

// Max DNS distinct names to keep
#define DTEL_MAX_DNS_NAMES 512
// Max IP addresses to map to the DNS names
#define DTEL_MAX_DNS_IPS  1024

#endif // _DEVTELEMETRY_H

