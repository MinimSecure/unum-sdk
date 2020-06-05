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

// unum platform specific include file for wireless

#ifndef _UNUM_WIRELESS_H
#define _UNUM_WIRELESS_H

#include <iwinfo.h>


// Max number of radios we can have for the platform (used for platforms
// where wireless code could be shared between hardare with potentially
// different number of radios, for now iwinfo and wl)
#define WIRELESS_MAX_RADIOS 4

// Max VAPs per radio we can report (used in wireless_common.h)
#define WIRELESS_MAX_VAPS 16


// Common header shared across all platforms
#include "../wireless_common.h"

// Header shared across platforms using wireless extensions
#include "../wireless_iwinfo.h"


// Max time we might potentially block the thread while doing wireless
// fiunctions calls (used to set watchdog).
#define WIRELESS_MAX_DELAY (60 * 5)

#endif // _UNUM_WIRELESS_H
