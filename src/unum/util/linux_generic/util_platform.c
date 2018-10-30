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
#include <uci.h>
#include "unum.h"

// Main WAN interface name (retrieved at startup only)
static char wan_ifname[IFNAMSIZ] = "";

// Main LAN interface name (retrieved at startup only)
static char lan_ifname[IFNAMSIZ] = "";

// Main LAN device name (retrieved at startup only)
static char lan_device[IFNAMSIZ] = "";

// Get main WAN interface name, NULL if not yet set
char *util_main_wan_ifname(void)
{
    if(*wan_ifname) {
        return wan_ifname;
    }
    return NULL;
}

// Get main LAN interface name, NULL if not yet set
char *util_main_lan_ifname(void)
{
    if(*lan_ifname) {
        return lan_ifname;
    }
    return NULL;
}

// Get base MAC file name, it's the file w/ the MAC of the main LAN interface
char *util_base_mac_file_name(void)
{
    char *device = "br-lan";
    static char fname[128] = "";

    if(*fname != 0) {
        return fname;
    }
    if(*lan_device == 0) {
        log("%s: LAN Ethernet interface name is not available, using <%s>\n",
            __func__, device);
    } else {
        device = lan_device;
    }
    snprintf(fname, sizeof(fname), "/sys/class/net/%s/address", device);

    return fname;
}

// Helper retrieving UCI string values.
// Returns NULL if no value. The value is available only
// until ctx is released.
static char *uci_get_str(struct uci_context *ctx, char *name)
{
    struct uci_option *o;
    struct uci_element *e;
    struct uci_ptr ptr;
    char uci_path[128];

    if(!ctx || !name) {
        return NULL;
    }
    // Note: uci_path has to be writeable since the awesome uci library
    //       wants to run strsep() on your data (which changes it)
    if(strlen(name) + 1 > sizeof(uci_path)) {
        log("%s: not enough space for <%s>\n", __func__, name);
        return NULL;
    }
    strcpy(uci_path, name);
    int ret = uci_lookup_ptr(ctx, &ptr, uci_path, TRUE);
    if(ret != UCI_OK ||
       !(ptr.flags & UCI_LOOKUP_COMPLETE) || (e = ptr.last) == NULL ||
       e->type != UCI_TYPE_OPTION || (o = ptr.o) == NULL ||
       o->type != UCI_TYPE_STRING)
    {
        return NULL;
    }

    return o->v.string;
}

// Optional platform init function, returns 0 if successful
// Note: this is currently used to populate all the necessary interface
//       names at startup, but the configuration might be changing, this
//       has to be revisited and done correctly
int util_platform_init(int level)
{
    struct uci_context *ctx = NULL;

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
        char *val;

        if(!ctx) {
            ctx = uci_alloc_context();
        }
        if(!ctx) {
            log("%s: uci_alloc_context() has faileed\n", __func__);
            break;
        }

        // Wait for the LAN and WAN to come up
        if(STARTUP_NETWORK_WAIT_TIME > util_time(1) - start_t) {
            static int lan_up = FALSE;
            static int wan_up = FALSE;

            val = uci_get_str(ctx, "network.lan.up");
            if(!lan_up && val && *val == '1') {
                log("%s: LAN is up\n", __func__);
                lan_up = TRUE;
            }

#ifndef AP_MODE
            val = uci_get_str(ctx, "network.wan.up");
            if(!wan_up && val && *val == '1')
            {
                log("%s: WAN is up\n", __func__);
                wan_up = TRUE;
            }
#else  // !AP_MODE
            wan_up = TRUE;
#endif // !AP_MODE

            log("lan_up %d... wan_up =%d\n", lan_up, wan_up);
            if(!lan_up || !wan_up) {
                // This code goes away eventually, just reopen context to
                // force it reload the config (there seem to be no easy
                // way to force it re-load the data)
                uci_free_context(ctx);
                ctx = NULL;
                sleep(10);
                continue;
            }
        }

        // Get main routing LAN interface type
        val = uci_get_str(ctx, "network.lan.type");
        if(val && strcmp(val, "bridge") != 0) {
            log("%s: error, LAN interface type <%s> is not supported\n",
                __func__, val);
            ret = -2;
            break;
        }

        // Get main LAN ethernet device (used to get base MAC address)
        val = uci_get_str(ctx, "network.lan.device");
        if(val == NULL) {
            log("%s: error, no LAN device found\n", __func__);
            ret = -2;
            break;
        }
        // The device field might contains a list, pick the first one
        int ii = 0;
        while(*val && *val != ' ') {
            lan_device[ii] = *val;
            ++val;
            ++ii;
        }
        lan_device[ii] = 0;

        // Get main LAN interface name (for TPCAP etc, has to have IP address)
        val = uci_get_str(ctx, "network.lan.ifname");
        if(val == NULL) {
            log("%s: error, no LAN interface name found\n", __func__);
            ret = -3;
            break;
        }
        strncpy(lan_ifname, val, sizeof(lan_ifname) - 1);
        strcpy(lan_device, lan_ifname);

#ifndef AP_MODE
        // Get WAN interface name (used for stats and WAN IP address reporting)
        val = uci_get_str(ctx, "network.wan.ifname");
        if(val == NULL) {
            log("%s: error, no WAN interface name found, trying device name\n",
                __func__);
            val = uci_get_str(ctx, "network.wan.device");
            if(val == NULL) {
                log("%s: error, no WAN device found\n", __func__);
                ret = -4;
                break;
            }
        }
        strncpy(wan_ifname, val, sizeof(wan_ifname) - 1);
#else  // !AP_MODE
        strncpy(wan_ifname, "lo", sizeof(wan_ifname) - 1);
#endif // !AP_MODE

        break;
    }
    uci_free_context(ctx);

    return ret;
}

// Enumerite the list of the LAN interfaces on the platform.
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

    // For now only deal w/ the main LAN interface. If we eventually
    // start supporting multiple VLAN configurations enumerate all
    // the config sections under "network" and report all where:
    // - name == "network.lan*" (naming convention)
    // - section type == "interface"
    // - option "up" == 1 (if it is down no point to waste memory on pkt ring)
    // - option "proto" == "static" (routing interfaces should not use DHCP)
    // Note: TPCAP uses this to rescans interfaces every 30sec, SSDP uses it to
    //       send discoveries, and there might be other uses for this function.
    if(0 != (UTIL_IF_ENUM_RTR_LAN & flags)) {
        ret = f(GET_MAIN_LAN_NET_DEV(), data);
        failed += (ret == 0 ? 0 : 1);
    }

#ifndef AP_MODE
    // We only support 1 WAN interface.
    // Note: This might need to be updated to deal with
    //       PPPoE and VPN-based WAN connections properly
    if(ret == 0 && 0 != (UTIL_IF_ENUM_RTR_WAN & flags)) {
        ret = f(GET_MAIN_WAN_NET_DEV(), data);
        failed += (ret == 0 ? 0 : 1);
    }
#endif // !AP_MODE

    return failed;
}
