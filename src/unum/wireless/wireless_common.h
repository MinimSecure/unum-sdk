// (c) 2017-2019 minim.co
// unum ieee802.11 common include file (shared by all platforms)

#ifndef _WIRELESS_COMMON_H
#define _WIRELESS_COMMON_H


// The wireless thread wakes up every few seconds (set by this define)
// to check if it is time to do any of the periodic actions (in seconds).
#define WIRELESS_ITERATE_PERIOD 5

// Neighborhood scan period (in seconds)
#define WIRELESS_SCAN_PERIOD (60 * 60 * 3)

// Radio & clients telemetry reporting period (in seconds)
#define WIRELESS_TELEMETRY_PERIOD 30

// Interface operation modes we report to the server
// the max len is WT_JSON_TPL_VAP_STATE_t mode[] field below.
#define WIRELESS_OPMODE_AP       "ap"       // typical AP
#define WIRELESS_OPMODE_MESH_11S "mesh_11s" // 802.1s mesh point
#define WIRELESS_OPMODE_STA      "sta"      // station in BSS

#define WIRELESS_RADIO_IS_DOWN   -2

// The structure for collecting state of the current radio being reported
// in the telemetry JSON
typedef struct _WT_JSON_TPL_RADIO_STATE {
    int flags;    // Flags (nothing yet)
    int num;      // The sequential number of the radio being processed
    char name[IFNAMSIZ]; // The name of the radio interface
    int chan;     // channel the radio is configured to
    JSON_KEYVAL_TPL_t *extras; // Ptr to template for platform extras
    int num_stas; // STAs on the radio (optional, if capturing them per-radio)
    int num_vaps; // VAPs in the 'vaps' array below
    char vaps[WIRELESS_MAX_VAPS][IFNAMSIZ]; // interface names of active VAPs
} WT_JSON_TPL_RADIO_STATE_t;

// The structure for collecting state of the current VAP being reported
// in the telemetry JSON
typedef struct _WT_JSON_TPL_VAP_STATE {
    int flags;      // Flags (nothing yet)
    int num;        // # of the VAP in the VAP list of the radio state data
    char ifname[IFNAMSIZ]; // The name of the VAP interface
    char ssid[65];  // SSID encoded in hexadecimal string form
    char bssid[MAC_ADDRSTRLEN];// BSSID as xx:xx:xx:xx:xx:xx string
    char mac[MAC_ADDRSTRLEN];  // STA MAC as xx:xx:xx:xx:xx:xx string (if STA)
    char mode[32];  // operation mode (defaults to "ap")
    int kind;      // Kind of interface for example 2.4Ghz home, 5Ghz guest etc
    JSON_KEYVAL_TPL_t *extras; // Ptr to template for platform extras
    int num_assocs; // Number of associations the current VAP or STA has
} WT_JSON_TPL_VAP_STATE_t;

// The structure for collecting state of the current STA being reported
// in the telemetry JSON
typedef struct _WT_JSON_TPL_STA_STATE {
    int flags;                // Flags (nothing yet)
    int num;                  // # of the STA we are reporting
    char mac[MAC_ADDRSTRLEN]; // MAC as xx:xx:xx:xx:xx:xx string of the STA
    int rssi;                 // Best (if multiple antennaes) signal strength
    JSON_KEYVAL_TPL_t *extras;// Ptr to template for platform extras
} WT_JSON_TPL_STA_STATE_t;

// The structure for collecting the current radio scan list for
// the telemetry JSON
typedef struct _WT_JSON_TPL_SCAN_RADIO {
    int flags;    // Flags (nothing yet)
    int num;      // The sequential number of the radio
    char name[IFNAMSIZ]; // The name of the radio interface
    int scan_entries;    // Number of scan entries captured
    void *data;   // Scan list data (optional, for use by the platform)
} WT_JSON_TPL_SCAN_RADIO_t;

// The structure for collecting state of the current STA being reported
// in the telemetry JSON
typedef struct _WT_JSON_TPL_SCAN_ENTRY {
    int flags;                 // Flags (nothing yet)
    int num;                   // # of the scan list entry being reported
    char bssid[MAC_ADDRSTRLEN];// BSSID as xx:xx:xx:xx:xx:xx string
    char ssid[65];             // SSID bytes in hex (i.e. each byte as "xx")
    int chan;                  // BSS's channel
    int rssi;                  // Signal strength
    JSON_KEYVAL_TPL_t *extras; // Ptr to template for platform extras
} WT_JSON_TPL_SCAN_ENTRY_t;

// The structure for returning wireless station couters.
// This is not specific to the wireless telemetry. It is used outside
// of the WT code. The rx_b and tx_b could wrap at 32bit (platform dependent) 
typedef struct _WIRELESS_COUNTERS {
#define W_COUNTERS_32B    0x01 // the byte counters are 32-bit wrapped
#define W_COUNTERS_MIF    0x02 // MAC was seen on multiple interfaces
#define W_COUNTERS_RIF    0x04 // using radio interfaces (not per-SSID)
    unsigned int flags;        // flags (see W_COUNTERS_* above)
    char ifname[IFNAMSIZ];     // interface where STA was found
    unsigned long rx_p;        // Rx packets (from the STA)
    unsigned long tx_p;        // Tx packets (to the STA)
    unsigned long long rx_b;   // Rx bytes (from the STA)
    unsigned long long tx_b;   // Tx bytes (to the STA)
} WIRELESS_COUNTERS_t;

typedef struct __WIRELESS_KIND {
    char ifname[IFNAMSIZ]; // The name of the VAP interface
    enum INTERFACE_KIND kind;
} WIRELESS_KIND_t;


// Converts SSID (up to 32 bytes) to the hex format
// (i.e. each byte -> 2 hex digits)
// Parameters:
// ssid_hex - ptr where to store the hex string (must be 65 bytes or more)
// ssid - ptr to the SSID string buffer to convert (no 0-termination)
// ssid_len - length of the "ssid" string
// Returns: 0 - ok, negative - failure
static inline int wt_mk_hex_ssid(char *ssid_hex, void *ssid, int ssid_len)
{
    int ii;
    if(ssid_len < 0 || ssid_len > 32)
    {
        return -1;
    }
    unsigned char *buf = (unsigned char *)ssid;
    char *cptr = ssid_hex;
    for(ii = 0; ii < ssid_len; ++ii) {
        *cptr++ = UTIL_MAKE_HEX_DIGIT(buf[ii] >> 4);
        *cptr++ = UTIL_MAKE_HEX_DIGIT(buf[ii] & 0xf);
    }
    *cptr = 0;
    return 0;
}

// Return the name of the radio
char *wt_get_radio_name(int ii);

// Returns the # of the radio the interface belongs to
// -1 if not a wireless interface
int wt_get_ifname_radio_num(char *ifname);

// Capture the radio info for radio telemetry JASON builder
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - no eror, just skip it
int wt_tpl_fill_radio_info(WT_JSON_TPL_RADIO_STATE_t *rinfo);

// Capture the VAP info for radio telemetry JASON builder
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - no eror, just skip it
int wt_tpl_fill_vap_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                         WT_JSON_TPL_VAP_STATE_t *vinfo);

// Capture the association info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_assoc_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                           WT_JSON_TPL_VAP_STATE_t *vinfo,
                           WT_JSON_TPL_STA_STATE_t *sinfo,
                           int sta_num);

// Capture the STA info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_sta_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                         WT_JSON_TPL_VAP_STATE_t *vinfo,
                         WT_JSON_TPL_STA_STATE_t *sinfo,
                         int sta_num);

// Tell all radios to scan wireless neighborhood
// Note: do not define it if the scan is not needed (i.e. returning 0)
// Return: how many seconds till the scan results are ready
//         0 - if no need to wait, -1 - scan not supported
// Note: if the return value is less than WIRELESS_ITERATE_PERIOD
//       the results are queried on the next iteration
int wireless_do_scan(void);

// Capture the scan list for a radio.
// The name and radio # in rscan structure are populated in the common code.
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_scan_radio_info(WT_JSON_TPL_SCAN_RADIO_t *rscan);

// Extract the scan entry info
// Returns: 0 - if successful (all required info captured),
//          negative - error, positive - skip (no error)
int wt_tpl_fill_scan_entry_info(WT_JSON_TPL_SCAN_RADIO_t *rscan,
                                WT_JSON_TPL_SCAN_ENTRY_t *entry, int ii);

// Notify platfrom code that the reported scan info has been collected
void wt_scan_info_collected(void);

// Get wireless counters for a station.
// Parameters:
// ifname - The ifname to look up the STA on (could be NULL, search all)
// mac_str - STA MAC address (binary)
// wc - ptr to WIRELESS_COUNTERS_t buffer to return the data in
// Returns:
// 0 - ok, negative - unable to get the counters for the given MAC address
int wireless_get_sta_counters(char *ifname, unsigned char* mac,
                              WIRELESS_COUNTERS_t *wc);

// Get wireless counters for a station.
// Parameters:
// ifname - The ifname to look up the STA on (could be NULL, search all)
// mac_str - STA MAC address (binary)
// wc - ptr to WIRELESS_COUNTERS_t buffer to return the data in
// Returns:
// 0 - ok, negative - unable to get the counters for the given MAC address
int __attribute__((weak)) wireless_iwinfo_platform_get_sta_counters(char *ifname,
                              unsigned char* mac,
                              WIRELESS_COUNTERS_t *wc);

// Init function for the platform wireless APIs (if needed,
// the default stub just returns TRUE).
// It is called at WIRELESS_ITERATE_PERIOD until TRUE is returned.
// Unless it returns true no other wireless APIs are called and
// no wireless telemetry is sent to the cloud.
int wt_platform_init(void);

// Function requesting the wireless scan to be performed immediately.
void cmd_wireless_initiate_scan(void);

// Platform init/deinit functions for each telemetry submission.
int wt_platform_subm_init(void);
int wt_platform_subm_deinit(void);

// Subsystem init function
int wireless_init(int level);

#endif // _WIRELESS_COMMON_H
