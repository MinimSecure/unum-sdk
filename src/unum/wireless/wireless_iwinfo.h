// (c) 2017-2019 minim.co
// unum wireless APIs include file (shared by openwrt/lede based
// platforms or anything that works through libiwinfo)

#ifndef _WIRELESS_IWINFO_H
#define _WIRELESS_IWINFO_H


// Max number of PHY(radio) interfaces we can handle
#define WT_MAX_IWINFO_PHYS WIRELESS_MAX_RADIOS

// Max number of VAP interfaces we can handle
#define WT_MAX_IWINFO_VIFS (WIRELESS_MAX_RADIOS * WIRELESS_MAX_VAPS)

// Number of iwinfo modes in IWINFO_OPMODE_NAMES array
#define WT_NUM_IWINFO_MODES 10


// Get next interface index for specific phy (start w/ if_prev_idx == -1)
// Returns:
// (0, 1, ...), -1 no more interfaces for the radio
int wt_iwinfo_get_if_num_for_phy(int phy_idx, int if_prev_idx);

// Get interface index by its name
// Returns:
// (0, 1, ...), -1 interface name not found
int wt_iwinfo_get_ifname_num(char *ifname);

// Returns the index of the phy the interface belongs to
// (0, 1, ...), -1 if not found
int wt_iwinfo_get_ifname_phy_num(char *ifname);

// Returns the index of the phy for the given phy name
// (0, 1, ...), -1 if not found
int wt_iwinfo_get_phy_num(char *phy);

// Return the name of the interface name for a given index
char *wt_iwinfo_get_ifname(int ii);

// Return the name of the phy for a given index
char *wt_iwinfo_get_phy(int ii);

// Get scan list for a phy or a VAP interface, write it into buf,
// the buf has to be at least IWINFO_BUFSIZE bytes long.
// Returns: the number of scan list entries if successful, negative if fails
int wt_iwinfo_get_scan(char *ifname, char *buf);

// Get association list fo a VAP interface, write it into buf,
// the buf has to be at least IWINFO_BUFSIZE bytes long.
// Returns: the number of stations if successful, negative if fails
int wt_iwinfo_get_stations(char *ifname, char *buf);

// Get noise level on the interface (in dBm)
// Returns: 0 if successful, negative if fails
int wt_iwinfo_get_noise(char *ifname, int *noise);

// Get transmit power offset (in dBm)
// Returns: 0 if successful, negative if fails
int wt_iwinfo_get_txpwr_offset(char *ifname, int *offset);

// Get max transmit power (in dBm if 'indbm' is TRUE)
// Returns: max Tx power in dBm if successful, negative if fails
int wt_iwinfo_get_max_txpwr(char *ifname, int indbm);

// Get ESSID (up to IWINFO_ESSID_MAX_SIZE == 32).
// essid - ptr to the buffer (32 bytes) where to save the ESSID (unless NULL)
// p_essid_len - ptr to int where to save the ESSID length (uness NULL)
// Note: essid can contain zeroes
// Returns: return 0 if successful, negative if error
int wt_iwinfo_get_essid(char *ifname, char *essid, int *p_essid_len);

// Get BSSID.
// bssid - ptr where to store 6 bytes of BSSID MAC (unless NULL)
// bssid_str - ptr where to store BSSID string xx:xx:xx:xx:xx:xx (unless NULL)
// Returns: return 0 if successful, negative if error
int wt_iwinfo_get_bssid(char *ifname, unsigned char *bssid, char *bssid_str);

// Get the interface or phy country code
char *wt_iwinfo_get_country(char *ifname);

// Get the channel
int wt_iwinfo_get_channel(char *ifname);

// Get the hardware mode the interface or phy is in
char *wt_iwinfo_get_hwmode(char *ifname);

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
char *wt_iwinfo_get_mode(char *ifname, int *mode_id);

// Makes a list of radios and their correspondent VAP interfaces for
// use by the rest of the iwinfo functions. The data is stored in
// the local wt_interfaces, wt_phys and wt_if_to_phy arrays.
// Returns: 0 - success or negative int if error
int wt_iwinfo_mk_if_list(void);

// Initializes iwinfo
// Returns: 0 - success or negative int if error
int wt_iwinfo_init(void);

char* wt_iwinfo_get_vap_name(char *phyname);

// This is a dummy function to be invoked when we use defaults
// for enumerating the interface list.
// Return some value greater than 0 to differentiate between
// weak and non-weak functions.
int __attribute__((weak)) wt_platform_iwinfo_mk_if_list(
            char interfaces[WT_MAX_IWINFO_VIFS][IFNAMSIZ],
            char phys[WT_MAX_IWINFO_PHYS][IFNAMSIZ],
            int phy[WT_MAX_IWINFO_VIFS],
            int *iif_idx_next,
            int *pphy_idx_next);

// This function is used for QCA interfaces to get the channel number
// On iw drivers the channel number is fetched on phy
// However on QCA drivers it should be fetched on VAPs
int __attribute__((weak)) wt_platform_iwinfo_get_channel(char *phyname);

// Get the first VAP name
char* __attribute__((weak)) wt_platform_iwinfo_get_vap(char *phyname);

#endif // _WIRELESS_IWINFO_H
