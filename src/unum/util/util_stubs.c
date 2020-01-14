// Copyright 2020 Minim Inc
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

// unum utils stubs

#include "unum.h"


// The default enumeritor of the IP interfaces for use in case platform does
// not provide one. It uses required PLATFORM_GET_MAIN_LAN_NET_DEV() and
// PLATFORM_GET_MAIN_WAN_NET_DEV() macros to get the interfaces.
// see util_enum_ifs() for more details.
int __attribute__((weak)) util_platform_enum_ifs(int flags, UTIL_IF_ENUM_CB_t f,
                                                 void *data)
{
    int ret = 0;
    int failed = 0;

    if(0 != (UTIL_IF_ENUM_RTR_LAN & flags))
    {
        ret = f(PLATFORM_GET_MAIN_LAN_NET_DEV(), data);
        failed += (ret == 0 ? 0 : 1);
    }
    if(0 != (UTIL_IF_ENUM_RTR_WAN & flags)) {
        ret = f(PLATFORM_GET_MAIN_WAN_NET_DEV(), data);
        failed += (ret == 0 ? 0 : 1);
    }

    return failed;
}
