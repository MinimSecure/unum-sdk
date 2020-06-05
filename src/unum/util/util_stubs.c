// (c) 2018 minim.co
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
