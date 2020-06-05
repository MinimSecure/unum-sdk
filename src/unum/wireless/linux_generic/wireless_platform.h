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

// unum local platform specific include file for code that
// handles local platform specific stuff, it is included by
// .c files explicitly (not through common headers), it should
// only be included by files in this folder


#ifndef _WIRELESS_PLATFORM_H
#define _WIRELESS_PLATFORM_H

// Capture the STAs info for an interface
// Returns: # of STAs - if successful, negative error otherwise
int wt_rt_get_stas_info(int radio_num, char *ifname);

#endif // _WIRELESS_PLATFORM_H
