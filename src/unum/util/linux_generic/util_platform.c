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

// unum helper platform specific utils code

#include <sys/utsname.h>
#include "unum.h"

// Get main WAN interface name, eth1 if not set
char *util_main_wan_ifname(void)
{
    if(unum_config.wan_ifcount > 0) {
        return unum_config.wan_ifname;
    }
    return "eth1";
}

// Get main LAN interface name, eth0 if not set
char *util_main_lan_ifname(void)
{
    if(unum_config.lan_ifcount > 0) {
        return unum_config.lan_ifname[0];
    }
    return "eth0";
}

// Get base MAC file name, it's the file w/ the MAC of the main LAN interface
char *util_base_mac_file_name(void)
{
    char *device = GET_MAIN_LAN_NET_DEV();
    static char fname[128] = "";

    if(*fname != 0) {
        return fname;
    }
    snprintf(fname, sizeof(fname), "/sys/class/net/%s/address", device);

    return fname;
}

// Optional platform init function, returns 0 if successful
// Note: this is currently used to populate all the necessary interface
//       names at startup, but the configuration might be changing, this
//       has to be revisited and done correctly
int util_platform_init(int level)
{
    if(level != INIT_LEVEL_SETUP) {
        return 0;
    }

    unsigned long start_t = util_time(1);
    int ret = 0;
    if(STARTUP_NETWORK_WAIT_TIME > 0) {
        log("%s: waiting up to %dsec for the interfaces to come up\n",
            __func__, STARTUP_NETWORK_WAIT_TIME);
    }
    for(;;)
    {
        // Wait for the LAN and WAN to come up
        if(STARTUP_NETWORK_WAIT_TIME > util_time(1) - start_t) {
            static int lan_up = FALSE;
            static int wan_up = FALSE;

            if(util_net_dev_is_up(GET_MAIN_LAN_NET_DEV())) {
                log("%s: LAN is up\n", __func__);
                lan_up = TRUE;
            }

#ifndef FEATURE_LAN_ONLY
            if(IS_OPM(UNUM_OPM_AP)) {
                // In AP mode set the flag to prevent further checking
                wan_up = TRUE;
            } else {
                // In non-AP mode do real check if WAN is up
                if (util_net_dev_is_up(GET_MAIN_WAN_NET_DEV())) {
                    log("%s: WAN is up\n", __func__);
                    wan_up = TRUE;
                }
            }
#else  // !FEATURE_LAN_ONLY
            wan_up = TRUE;
#endif // !FEATURE_LAN_ONLY

            log("lan_up=%d... wan_up=%d\n", lan_up, wan_up);
            if(!lan_up || !wan_up) {
                sleep(10);
                continue;
            }
        }
        break;
    }

    return ret;
}

// Enumerate the list of the LAN interfaces on the platform.
// For each interface a caller's callback is invoked until all the interfaces
// are enumerated.
// flags - flags indicating which interfaces to enumerate
//        UTIL_IF_ENUM_RTR_LAN - include all IP routing LAN interfaces
//        UTIL_IF_ENUM_RTR_WAN - include IP routing WAN interface
//        ... - see util_net.h
// f - callback function to invoke per interface
// data - data to pass to the callback function
// Returns: 0 - success, number of times the callback has failed
int util_platform_enum_ifs(int flags, UTIL_IF_ENUM_CB_t f, void *data)
{
    int ret = 0;
    int failed = 0;

    // For now only deal w/ the main LAN interface.
    // Note: TPCAP uses this to rescans interfaces every 30sec, SSDP uses it to
    //       send discoveries, and there might be other uses for this function.
    if(0 != (UTIL_IF_ENUM_RTR_LAN & flags)) {
        ret = f(GET_MAIN_LAN_NET_DEV(), data);
        failed += (ret == 0 ? 0 : 1);
    }

#ifndef FEATURE_LAN_ONLY
    // We only support 1 WAN interface.
    // Note: This might need to be updated to deal with
    //       PPPoE and VPN-based WAN connections properly
    if(ret == 0 && 0 != (UTIL_IF_ENUM_RTR_WAN & flags)) {
        ret = f(GET_MAIN_WAN_NET_DEV(), data);
        failed += (ret == 0 ? 0 : 1);
    }
#endif // !FEATURE_LAN_ONLY

    return failed;
}
