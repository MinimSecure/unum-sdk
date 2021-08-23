// (c) 2017-2020 minim.co
// unum helper platform specific utils code

#include "unum.h"

// Main WAN interface name (retrieved at startup only)
static char wan_ifname[IFNAMSIZ] = "";

// Main LAN interface name (retrieved at startup only)
static char lan_ifname[IFNAMSIZ] = "";

// Main LAN device name (retrieved at startup only)
static char lan_device[IFNAMSIZ] = "";

// Platform version number string we get from /etc/openwrt_release
static char owrt_version[32] = "";

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
    int err;
    struct stat st;

    if(*fname != 0) {
        return fname;
    }

    // Check if deviceid file exists
    // If so read MAC Address from there
    err = stat(DEVICEID_FILE, &st);

    if (err == 0) {
        snprintf(fname, sizeof(fname), DEVICEID_FILE);
    } else {
        if(*lan_device == 0) {
            log("%s: LAN Ethernet inteface name is not available, using <%s>\n",
                __func__, device);
        } else {
            device = lan_device;
        }
        snprintf(fname, sizeof(fname), "/sys/class/net/%s/address", device);
    }

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

    // Currently only need it to set up things that might vary
    // between LEDE firmware images
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
        int err;
        struct stat st;

        if(!ctx) {
            ctx = uci_alloc_context();
        }
        if(!ctx) {
            log("%s: uci_alloc_context() has faileed\n", __func__);
            break;
        }
        // Set path to current state folder (contains current state vars)
        uci_add_delta_path(ctx, "/var/state");

        // Switch to operation mode set in the configuration unless it has
        // been forced with the command line option
        if(IS_OPM(UNUM_OPM_AUTO)) {
            int status;
            val = uci_get_str(ctx, "minim.@unum[0].opmode");
            if(val == NULL) {
                log("%s: no opmode configured, using defaut \"%s\"\n",
                    __func__, unum_config.opmode);
            } else if((status = util_set_opmode(val)) != 0) {
                log("%s: error %d setting configured opmode \"%s\"\n",
                    __func__, status, val);
                // Keep going, maybe it will manage
            } else {
                log("%s: using configured opmode \"%s\"\n",
                    __func__, unum_config.opmode);
            }
            // Note: the auto flag is cleared if the mode is set successfully
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

#ifndef FEATURE_LAN_ONLY
            if(IS_OPM(UNUM_OPM_AP)) {
                // In AP mode set the flag to prevent furhter checking
                wan_up = TRUE;
            } else {
                // In non-AP mode do real check if WAN is up
                val = uci_get_str(ctx, "network.wan.up");
            }
            if(!wan_up && val && *val == '1')
            {
                log("%s: WAN is up\n", __func__);
                wan_up = TRUE;
            }
#else  // !FEATURE_LAN_ONLY
            wan_up = TRUE;
#endif // !FEATURE_LAN_ONLY

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
            log("%s: error, LAN inteface type <%s> is not supported\n",
                __func__, val);
            ret = -2;
            break;
        }

        err = stat(DEVICEID_FILE, &st);
        if (err != 0) {
            // DeviceID file does n't exist. Get the LAN device name here
            // Get main LAN ethernet device (used to get base MAC address)
            // Note: this doesn't work at all in 18.06.5, keeping it here
            //       for backward compatibility only.
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
        }

        // Get main LAN interface name (for TPCAP etc, has to have IP address)
        if(IS_OPM(UNUM_OPM_AP)) {
            val = "br-lan";
        } else {
            val = uci_get_str(ctx, "network.lan.ifname");
            if(val == NULL) {
                log("%s: error, no LAN intrface name found\n", __func__);
                ret = -3;
                break;
            }
        }
        strncpy(lan_ifname, val, sizeof(lan_ifname) - 1);

        // Get WAN interface name (used for stats and WAN IP address reporting)
        if(IS_OPM(UNUM_OPM_AP)) {
            val = "lo"; // Use loopback to bypass getting WAN name in AP mode
        } else {
            val = uci_get_str(ctx, "network.wan.ifname");
        }
        if(val == NULL) {
            log("%s: error, no WAN inteface name found, trying device name\n",
                __func__);
            val = uci_get_str(ctx, "network.wan.device");
            if(val == NULL) {
                log("%s: error, no WAN device found\n", __func__);
                ret = -4;
                break;
            }
        }
        strncpy(wan_ifname, val, sizeof(wan_ifname) - 1);

        break;
    }
    uci_free_context(ctx);

    return ret;
}

// Optional platform operation mode change handler, called if the
// opmode is changing after the INIT_LEVEL_SETUP is complete
// (i.e. at activate). The function does not have to set the new flags
// or modify the mode string in unum_config, just do platform specific
// canges and return 0 for the common code to finish making changes.
// old_flags - current opmode flags
// new_flags - new opmode flags
// Returns: 0 - ok, negative - can't accept the new flags
int platform_change_opmode(int old_flags, int new_flags)
{
    // We can't allow switching from ap to gw mode since the agent doesn't
    // know the WAN interface. We should never need to do that anyway.
    if((old_flags & UNUM_OPM_GW) == 0 && (new_flags & UNUM_OPM_GW) != 0) {
        log("%s: error, cannot change from AP mode to gateway\n", __func__);
        return -1;
    }
    // Nothing else to do if turning on/off mesh or becoming AP
    return 0;
}

// Enumerite the list of the IP interfaces for the platform.
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

    // For now only deal w/ the main LAN interface (name discovered at startup
    // dynamically) and specifically named guest and additional VLAN interfaces
    // (if exist).
    // Note: TPCAP uses this to rescan interfaces every 30sec, SSDP uses it to
    //       send discoveries, and there might be other uses for this function.
    if(0 != (UTIL_IF_ENUM_RTR_LAN & flags))
    {
        int ii;
        struct { char *ifs; int if_type; } if_info[] = {
            { PLATFORM_GET_MAIN_LAN_NET_DEV(),  IF_ENUM_CB_ED_IFT_PRIMARY },
            { PLATFORM_GET_GUEST_LAN_NET_DEV(), IF_ENUM_CB_ED_IFT_GUEST   },
            { PLATFORM_GET_AUX_LAN_NET_DEV(1),  0                         },
            { PLATFORM_GET_AUX_LAN_NET_DEV(2),  0                         },
        };

        for(ii = 0;
            ii < UTIL_ARRAY_SIZE(if_info) && if_info[ii].ifs != NULL;
            ++ii)
        {
            if(if_nametoindex(if_info[ii].ifs) <= 0) {
                continue;
            }
            if((flags & UTIL_IF_ENUM_EXT_DATA) != 0) {
                IF_ENUM_CB_EXT_DATA_t *e_data = (IF_ENUM_CB_EXT_DATA_t *)data;
                e_data->if_type = if_info[ii].if_type;
                e_data->flags |= IF_ENUM_CB_ED_FLAGS_IFT;
            }
            ret = f(if_info[ii].ifs, data);
            failed += (ret == 0 ? 0 : 1);
        }
    }

#ifndef FEATURE_LAN_ONLY
    // We only support 1 WAN interface.
    // Note: This might need to be updated to deal with
    //       PPPoE and VPN-based WAN connections properly
    if(IS_OPM(UNUM_OPM_GW) && 0 != (UTIL_IF_ENUM_RTR_WAN & flags)) {
        ret = f(PLATFORM_GET_MAIN_WAN_NET_DEV(), data);
        failed += (ret == 0 ? 0 : 1);
    }
#endif // !FEATURE_LAN_ONLY

    return failed;
}

// The function returns the firmware version from /etc/openwrt_release
// file. The version string is cached on the first run and is returned
// back from the cache on the subsequent calls. In the case of an error
// the harcoded "unknown" string is returned.
char *platform_fw_version(void)
{
    // If we already know the version
    if(*owrt_version != 0) {
        return owrt_version;
    }

    char str[128];
    char *ret = "unknown";
    char *fname = "/etc/openwrt_release";

    FILE *f = fopen(fname, "r");
    if(f == NULL) {
        log("%s: failed to open <%s>, error: %s\n",
            __func__, fname, strerror(errno));
        return ret;
    }
    while(fgets(str, sizeof(str), f) != NULL) {
        char match[] = "DISTRIB_RELEASE=";
        if(strncmp(str, match, sizeof(match) - 1) != 0) {
            continue;
        }
        char *val = str + (sizeof(match) - 1);
        // the values are in single quotas
        if(*val == '\'') {
            ++val;
            char *end = strchr(val, '\'');
            if(end) {
                *end = 0;
            }
        }
        if(val && strlen(val) > 0) {
            strncpy(owrt_version, val, sizeof(owrt_version) - 1);
            ret = owrt_version;
        }
        break;
    }
    fclose(f);

    return ret;
}

int platform_release_renew(void)
{
    char cmd_buf[64];
    struct uci_context *ctx = uci_alloc_context();

    // Allocate UCI Context
    if(ctx == NULL) {
        log("%s: Unable to allocate UCI context.\n", __func__);
        return 1;
    }

    // Get WAN Protocol & Check for DHCP
    char * wan_proto = uci_get_str(ctx, "network.wan.proto");
    if((wan_proto == NULL) || (0 != strcmp(wan_proto,"dhcp"))) {
        log("%s: WAN interface is not configured for DHCP.\n", __func__);
        uci_free_context(ctx);
        return 1;
    }
    uci_free_context(ctx);

    // Build Command String & NULL Terminate
    snprintf(cmd_buf, 64, "udhcpc -i %s -n", util_main_wan_ifname());
    cmd_buf[63] = '\0';

    // Execute Command & Print Result
    log("%s: Performing release/renew using cmd <%s>\n", __func__, cmd_buf);
    int result = system(cmd_buf);
    log("%s: Result = %d\n", __func__, result);

    // Wait For Changes to Take Effect
    sleep(5);

    return result;

}

// Returns network type,ie home or guest or other
// Return 0 for home network, 1 for guest
// and 2 for other
static int get_network_type(char *ifname)
{
    char br_path[64];
    struct stat st;

    // Check if the interface is associated with home network
    snprintf(br_path, sizeof(br_path) - 1,
                            "/sys/class/net/br-lan/brif/%s", ifname);
    if (stat(br_path, &st) == 0) {
        return 0;
    }

    // Check if the interface is associated with guest network
    snprintf(br_path, sizeof(br_path) - 1,
                            "/sys/class/net/br-guest/brif/%s", ifname);
    if (stat(br_path, &st) == 0) {
        return 1;
    }
    return 2;
}

// Get interface kind
// Returns Radio kind
// -1 if the interface name is not found
int util_get_interface_kind(char *ifname)
{
    int chan;
    int mode;

    // Check if the interface name is lan or wan
    if (strncmp(lan_ifname, ifname, IFNAMSIZ) == 0) {
        return UNUM_INTERFACE_KIND_LAN;
    } else if (strncmp(lan_ifname, ifname, IFNAMSIZ) == 0) {
        return UNUM_INTERFACE_KIND_WAN;
    }

    // Get channel and use it to determine 2.4Ghz vs 5Ghz
    chan = wt_iwinfo_get_channel(ifname);
    if (chan < 0) {
        // Error is logged in wt_iwinfo_get_channel
        return -1;
    }

    if (wt_iwinfo_get_mode(ifname, &mode) == NULL) {
        // Error is logged in wt_iwinfo_get_mode_id
        return -1;
    }

    // Check 2.4Ghz vs 5Ghz
    if (chan >= 1 && chan <= 14) {
        if (mode == IWINFO_OPMODE_MESHPOINT) {
            return UNUM_INTERFACE_KIND_MESH2;
        } else if (mode == IWINFO_OPMODE_MASTER) {
            int type = get_network_type(ifname);
            // Check home vs guest network
            if (type == 0) {
                return UNUM_INTERFACE_KIND_HOME2;
            } else if (type == 1) {
                return UNUM_INTERFACE_KIND_GUEST2;
            }
        }
        // Server is handling mesh and master modes only for now
    } else if (chan >= 36 && chan <= 196) {
        if (mode == IWINFO_OPMODE_MESHPOINT) {
            return UNUM_INTERFACE_KIND_MESH5;
        } else if (mode == IWINFO_OPMODE_MASTER) {
            // Check home vs guest network
            int type = get_network_type(ifname);
            if (type == 0) {
                return UNUM_INTERFACE_KIND_HOME5;
            } else if (type == 1) {
                return UNUM_INTERFACE_KIND_GUEST5;
            }
        }
        // Server is handling mesh and master modes only for now
    }
    // ToDo: Handle 6Ghz channel list when it is available
    return -1;
}

// Get interface kind
// Returns Radio kind
// -1 if the Radio name is not found
int util_get_radio_kind(char *ifname)
{
    // Get channel and use it to determine 2.4Ghz vs 5Ghz
    int chan = wt_iwinfo_get_channel(ifname);

    if (chan >= 1 && chan <= 14) {
        return UNUM_RADIO_KIND_2;
    } else if (chan >= 36 && chan <= 196) {
        return UNUM_RADIO_KIND_5;
    }
    // ToDo: Handle 6Ghz channel list when it is available
    return -1;
}
