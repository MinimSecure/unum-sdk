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

// device fingerprinting info collection and reporting

#ifndef _FINGERPRINT_H
#define _FINGERPRINT_H

// Pull in the common include
#include "../fingerprint_common.h"

// Note: all the below limits are for the information
// collected during single reporting time period
// (currently the data is reported w/ devices telemetry)

// Max number of devices
#define FP_MAX_DEV 256

#endif // _FINGERPRINT_H

