// (c) 2017-2019 minim.co
// unum wireless extensions APIs, used only by platforms
// where drivers that work w/ libiwinfo
// Note: all the functions here are designed to be called from
//       the wireless telemetry thread only
// Note1: wt_iwinfo_mk_if_list() must be called before the functions
//        capable of resolving phys/interfaces/indices can be used

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Array w/ the list of interface configured on the system
static char wt_interfaces[WT_MAX_IWINFO_VIFS][IFNAMSIZ];
static int if_idx_next = 0;

// Array w/ the list of the phys
static char wt_phys[WT_MAX_IWINFO_PHYS][IFNAMSIZ];
static int phy_idx_next = 0;

// Array w/ mapping of interface to phy index in the above arrays
static int wt_if_to_phy[WT_MAX_IWINFO_VIFS];


// Get next interface index for specific phy (start w/ if_prev_idx == -1)
// Returns:
// (0, 1, ...), -1 no more interfaces for the radio
int wt_iwinfo_get_if_num_for_phy(int phy_idx, int if_prev_idx)
{
    int ii;
    for(ii = if_prev_idx + 1; ii >= 0 && ii < if_idx_next; ++ii)
    {
        if(wt_if_to_phy[ii] == phy_idx) {
            return ii;
        }
    }
    return -1;
}

// Get interface index by its name
// Returns:
// (0, 1, ...), -1 interface name not found
int wt_iwinfo_get_ifname_num(char *ifname)
{
    int ii, idx = -1;
    for(ii = 0; ii < if_idx_next; ++ii) {
        if(strcmp(wt_interfaces[ii], ifname) == 0) {
            idx = ii;
            break;
        }
    }
    return idx;
}

// Returns the index of the phy the interface belongs to
// (0, 1, ...), -1 if not a wireless interface
int wt_iwinfo_get_ifname_phy_num(char *ifname)
{
    int ii, idx = -1;
    for(ii = 0; ii < if_idx_next; ++ii) {
        if(strcmp(wt_interfaces[ii], ifname) == 0) {
            idx = wt_if_to_phy[ii];
            break;
        }
    }
    if(idx >= 0 && idx < phy_idx_next) {
        return idx;
    }
    return -1;
}

// Returns the index of the phy for the given phy name
// (0, 1, ...), -1 if not found
int wt_iwinfo_get_phy_num(char *phy)
{
    int ii, idx = -1;
    for(ii = 0; ii < phy_idx_next; ++ii) {
        if(strcmp(wt_phys[ii], phy) == 0) {
            idx = ii;
            break;
        }
    }
    if(idx >= 0 && idx < phy_idx_next) {
        return idx;
    }
    return -1;
}

// Return the name of the interface for a given index
char *wt_iwinfo_get_ifname(int ii)
{
    if(ii < 0 || ii >= if_idx_next) {
        return NULL;
    }
    return wt_interfaces[ii];
}

// Return the name of the phy for a given index
char *wt_iwinfo_get_phy(int ii)
{
    if(ii < 0 || ii >= phy_idx_next) {
        return NULL;
    }
    return wt_phys[ii];
}

// Get scan list for a phy or a VAP interface, write it into buf,
// the buf has to be at least IWINFO_BUFSIZE bytes long.
// Returns: the number of scan list entries if successful, negative if fails
int wt_iwinfo_get_scan(char *ifname, char *buf)
{
    int len = 0;
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);

    if(!iw) {
        log("%s: no backend for %s\n", __func__, ifname);
        return -1;
    }
    if(iw->scanlist(ifname, buf, &len) != 0)
    {
        log_dbg("%s: failed to get scanlist for %s\n", __func__, ifname);
        return -2;
    }
    else if(len < 0)
    {
        log("%s: negative number of stations for %s, assuming 0\n",
            __func__, ifname);
        return 0;
    }

    return len / sizeof(struct iwinfo_scanlist_entry);
}

// Get association list for a VAP interface, write it into buf,
// the buf has to be at least IWINFO_BUFSIZE bytes long.
// Returns: the number of stations if successful, negative if fails
int wt_iwinfo_get_stations(char *ifname, char *buf)
{
    int len = 0;
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);

    if(!iw) {
        log("%s: no backend for %s\n", __func__, ifname);
        return -1;
    }
    if(iw->assoclist(ifname, buf, &len) != 0)
    {
        log_dbg("%s: failed to get assoclist for %s\n", __func__, ifname);
        return -2;
    }
    else if(len < 0)
    {
        log("%s: negative number of stations for %s, assuming 0\n",
            __func__, ifname);
        return 0;
    }

    return len / sizeof(struct iwinfo_assoclist_entry);
}

// Get noise level on the interface (in dBm)
// Returns: 0 if successful, negative if fails
int wt_iwinfo_get_noise(char *ifname, int *noise)
{
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);

    if(!iw) {
        log("%s: no backend for %s\n", __func__, ifname);
        return -1;
    }

    if(iw->noise(ifname, noise) != 0) {
        log_dbg("%s: failed to get noise level for %s\n", __func__, ifname);
        return -2;
    }

    return 0;
}

// Get transmit power offset (in dBm, it is quite often not supported)
// Returns: 0 if successful, negative if fails
int wt_iwinfo_get_txpwr_offset(char *ifname, int *offset)
{
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);

    if(!iw) {
        log("%s: no backend for %s\n", __func__, ifname);
        return -1;
    }

    if(iw->txpower_offset(ifname, offset) != 0) {
        return -2;
    }

    return 0;
}

// Get max transmit power (in dBm if 'indbm' is TRUE)
// Returns: max Tx power in dBm if successful, negative if fails
int wt_iwinfo_get_max_txpwr(char *ifname, int indbm)
{
    int txpower;
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);

    if(!iw) {
        log("%s: no backend for %s\n", __func__, ifname);
        return -1;
    }

    if(iw->txpower(ifname, &txpower) != 0) {
        log_dbg("%s: failed to get max tx power for %s\n", __func__, ifname);
        return -2;
    }

    return txpower;
}

// Get ESSID (up to IWINFO_ESSID_MAX_SIZE == 32).
// essid - ptr to the buffer (32 bytes) where to save the ESSID (unless NULL)
// p_essid_len - ptr to int where to save the ESSID length (uness NULL)
// Note: essid can contain zeroes
// Returns: return 0 if successful, negative if error
int wt_iwinfo_get_essid(char *ifname, char *essid, int *p_essid_len)
{
    char buf[IWINFO_ESSID_MAX_SIZE + 1];
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);

    memset(buf, 0, sizeof(buf));
    if(!iw) {
        log("%s: no backend for %s\n", __func__, ifname);
        return -1;
    }
    *buf = 0;
    if(iw->ssid(ifname, buf) != 0) {
        log_dbg("%s: failed to get SSID for %s\n", __func__, ifname);
        return -2;
    }
    buf[IWINFO_ESSID_MAX_SIZE] = 0;

    memset(essid, 0, IWINFO_ESSID_MAX_SIZE);
    if(essid) {
        memcpy(essid, buf, IWINFO_ESSID_MAX_SIZE);
    }
    // The function does not return length of SSID, so we will not know if
    // it was set up w/ 0s at the end or it ends w/ the last non-0 character.
    if(p_essid_len) {
        int len;
        for(len = IWINFO_ESSID_MAX_SIZE; len >= 0 && buf[len] == 0; --len);
        *p_essid_len = len + 1;
    }

    return 0;
}

// Get BSSID.
// bssid - ptr where to store 6 bytes of BSSID MAC (unless NULL)
// bssid_str - ptr where to store BSSID string xx:xx:xx:xx:xx:xx (unless NULL)
// Returns: return 0 if successful, negative if error
int wt_iwinfo_get_bssid(char *ifname, unsigned char *bssid, char *bssid_str)
{
    char buf[32];
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);

    if(!iw) {
        log("%s: no backend for %s\n", __func__, ifname);
        return -1;
    }
    *buf = 0;
    if(iw->bssid(ifname, buf) != 0) {
        log_dbg("%s: failed to get BSSID for %s\n", __func__, ifname);
        return -2;
    }
    if(bssid_str != NULL) {
        strncpy(bssid_str, buf, MAC_ADDRSTRLEN);
        bssid_str[MAC_ADDRSTRLEN - 1] = 0;
        str_tolower(bssid_str);
    }
    if(bssid) {
        int o[6];
        int n = sscanf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
                       &o[0], &o[1], &o[2], &o[3], &o[4], &o[5]);
        if(n != 6) {
            log("%s: invalid BSSID <%s> for %s\n", __func__, buf, ifname);
            return -3;
        }
        for(n = 0; n < 6; n++) {
            bssid[n] = o[n];
        }
    }

    return 0;
}

// Get the interface or phy country code
char*  __attribute__((weak)) wt_iwinfo_get_country(char *ifname)
{
    static char cc[32];
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);

    if(!iw) {
        log("%s: no backend for %s\n", __func__, ifname);
        return NULL;
    }
    *cc = 0;
    if(iw->country(ifname, cc) != 0) {
        log_dbg("%s: failed to get country code for %s\n", __func__, ifname);
        return NULL;
    }

    return cc;
}

// Get the channel
int wt_iwinfo_get_channel(char *ifname)
{
    if (wt_platform_iwinfo_get_channel != NULL) {
        return wt_platform_iwinfo_get_channel(ifname);
    }
    int ch;
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);

    if(!iw) {
        log("%s: no backend for %s\n", __func__, ifname);
        return -1;
    }
    if(iw->channel(ifname, &ch) != 0) {
        log_dbg("%s: failed to get channel for %s\n", __func__, ifname);
        return -2;
    }

    return ch;
}

// Get the hardware mode the interface or phy is in
char *wt_iwinfo_get_hwmode(char *ifname)
{
    static char buf[32];
    int hwmodes;
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);

    if(!iw) {
        log("%s: no backend for %s\n", __func__, ifname);
        return NULL;
    }
    if(iw->hwmodelist(ifname, &hwmodes) != 0) {
        log_dbg("%s: failed to get HW modes for %s\n", __func__, ifname);
        return NULL;
    }
    if(hwmodes <= 0) {
        log("%s: invalid mode bitmap 0x%x for %s\n", __func__, hwmodes, ifname);
        return NULL;
    } else {
        snprintf(buf, sizeof(buf), "11%s%s%s%s%s",
                 (hwmodes & IWINFO_80211_A) ? "a" : "",
                 (hwmodes & IWINFO_80211_B) ? "b" : "",
                 (hwmodes & IWINFO_80211_G) ? "g" : "",
                 (hwmodes & IWINFO_80211_N) ? "n" : "",
                 (hwmodes & IWINFO_80211_AC) ? "ac" : "");
    }

    return buf;
}

// Get operation mode (master, client...).
// Returns: static string w/ mode name or NULL,
//          if mode_id is not NULL the mode ID is stored there
//          IWINFO_OPMODE_UNKNOWN
//          IWINFO_OPMODE_MASTER
//          IWINFO_OPMODE_ADHOC
//          IWINFO_OPMODE_CLIENT
//          IWINFO_OPMODE_MONITOR
//          IWINFO_OPMODE_AP_VLAN
//          IWINFO_OPMODE_WDS
//          IWINFO_OPMODE_MESHPOINT
//          IWINFO_OPMODE_P2P_CLIENT
//          IWINFO_OPMODE_P2P_GO
char *wt_iwinfo_get_mode(char *ifname, int *mode_id)
{
    static char buf[32];
    int mode;
    const struct iwinfo_ops *iw = iwinfo_backend(ifname);

    if(!iw) {
        log("%s: no backend for %s\n", __func__, ifname);
        return NULL;
    }
    if(iw->mode(ifname, &mode) != 0) {
        log_dbg("%s: failed to get opmode for %s\n", __func__, ifname);
        return NULL;
    }
    if(mode < 0) {
        log("%s: invalid opmode id %d for %s\n", __func__, mode, ifname);
        return NULL;
    }
    if(mode > IWINFO_OPMODE_P2P_GO) {
        snprintf(buf, sizeof(buf), "mode_%d", mode);
    } else {
        snprintf(buf, sizeof(buf), "%s", IWINFO_OPMODE_NAMES[mode]);
    }
    if(mode_id) {
        *mode_id = mode;
    }
    str_tolower(buf);

    return buf;
}

// Makes a list of radios and their correspondent VAP interfaces for
// use by the rest of the iwinfo functions. The data is stored in
// the local wt_interfaces, wt_phys and wt_if_to_phy arrays.
// Returns: 0 - success or negative int if error
int wt_iwinfo_mk_if_list(void)
{
    // Use platform interface list generator if defined
    if(wt_platform_iwinfo_mk_if_list != NULL) {
        return wt_platform_iwinfo_mk_if_list(wt_interfaces,
                            wt_phys,
                            wt_if_to_phy,
                            &if_idx_next,
                            &phy_idx_next);
    }

    char *sysnetdev_dir = "/sys/class/net";
    struct dirent *de;
    int err, ii;
    DIR *dd;

    dd = opendir(sysnetdev_dir);
    if(dd == NULL) {
        log("%s: error opening %s: %s\n",
            __func__, sysnetdev_dir, strerror(errno));
        return -1;
    }
    // Clean up the arrays
    memset(wt_interfaces, 0, sizeof(wt_interfaces));
    memset(wt_phys, 0, sizeof(wt_phys));
    memset(wt_if_to_phy, 0, sizeof(wt_if_to_phy));
    if_idx_next = phy_idx_next = 0;
    while((de = readdir(dd)) != NULL)
    {
        // Skip anything that starts w/ .
        if(de->d_name[0] == '.') {
            continue;
        }
        char *ifname = de->d_name;
        const struct iwinfo_ops *iw = iwinfo_backend(ifname);
        if(!iw) {
            continue;
        }
        if(if_idx_next >= WT_MAX_IWINFO_VIFS) {
            log("%s: too many (%d/%d) wireless interfaces\n",
                __func__, if_idx_next, WT_MAX_IWINFO_VIFS);
            break;
        }
        // get phyname for the interface
        char phyname[32]; // no bffer size, IFNAMSIZ?, but libiwinfo uses 32
        err = iw->phyname(ifname, phyname);
        if(err != 0 || *phyname == 0) {
            log("%s: error %d getting phy name for %s\n",
                __func__, err, ifname);
            continue;
        }
        if(strlen(phyname) >= IFNAMSIZ) {
            log("%s: error phy name for <%s> is too long\n", __func__, ifname);
            continue;
        }
        // check if we can add or already have the phy
        for(ii = 0; ii < phy_idx_next; ++ii) {
            if(strcmp(phyname, wt_phys[ii]) == 0) {
                break;
            }
        }
        if(ii >= WT_MAX_IWINFO_PHYS) {
            log("%s: too many (%d) wireless radios/phys\n",
                __func__, ii);
            continue;
        }
        if(phy_idx_next == ii) {
            ++phy_idx_next;
            strncpy(wt_phys[ii], phyname, IFNAMSIZ - 1);
        }
        wt_if_to_phy[if_idx_next] = ii;
        strncpy(wt_interfaces[if_idx_next], ifname, IFNAMSIZ - 1);
        ++if_idx_next;
    }
    closedir(dd);
    dd = NULL;

    return 0;
}

// Initializes iwinfo
// Returns: 0 - success or negative int if error
int wt_iwinfo_init(void)
{
    // Nothing to do here yet
    return 0;
}
