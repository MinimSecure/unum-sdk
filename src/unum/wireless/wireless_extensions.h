// (c) 2017-2018 minim.co
// unum wireless extensions APIs include file (shared by platforms
// based on wireless extensions drivers)

#ifndef _WIRELESS_EXT_H
#define _WIRELESS_EXT_H

// For WE 25 and up default max payload size is 4096
// Note: platform might override by redefining WT_WLEXT_MAX_PRIV_PAYLOAD
//       size after including this header.
#define WT_WLEXT_MAX_PRIV_PAYLOAD 4096


// Perform direct pivate ioctl get call (it does not have to be in the
// supported calls table). This can only be used for private GET IOCTLs
// that return data in a USER buffer (not within struct iwreq).
// ifname - interface to apply the call to
// icmd - IOCTL command code
// icmd - IOCTL sub-command code (0 if none)
// buf - pointer to the buffer for user data (both in and out)
// buf_size - the buf size
// Returns: negative if error, number of bytes returned if success
//          if the # of bytes returned is greater than buf_size, then
//          only the buf_size portion of the data was returned
// Note: this function shares the temporary data buffer w/ wt_wlext_priv()
int wt_wlext_priv_direct(char *ifname, int icmd, int subcmd,
                         void *buf, int buf_size);

// Perform pivate ioctl call. This function can be called from outside of
// the wireless telemetry subsystem.
// sock - wl extensions socket (if -1 creates its own)
// ifname - interface to apply the call to
// cmd - double-0 terminated list of 0-separated command followed by arguments
// buf - pointer to the buffer for sending/receiving data
// buf_size - the buffer size
// Returns: negative if error, number of bytes returned if success
//          if the # of bytes returned is greater than buf_size, then
//          only the buf_size portion of the data was returned
int wlext_priv_cmd(int sock, char *ifname, char *cmd, void *buf, int buf_size);

// Perform pivate ioctl call (max data it passes or returns is 4096 bytes)
// ifname - interface to apply the call to
// cmd - double-0 terminated list of 0-separated command followed by arguments
// buf - pointer to the buffer for returned data (if any)
// buf_size - the buf size
// Returns: negative if error, number of bytes returned if success
//          if the # of bytes returned is greater than buf_size, then
//          only the buf_size portion of the data was returned
// Note: shares the temporary data buffer w/ wt_wlext_priv_direct()
int wt_wlext_priv(char *ifname, char *cmd, char *buf, int buf_size);

// Get ESSID (up to IW_ESSID_MAX_SIZE == 32).
// essid - ptr to the buffer (32 bytes) where to save the ESSID (unless NULL)
// p_essid_len - ptr to int where to save the ESSID length (uness NULL)
// Note: essid can contain zeroes
// Returns: return 0 if successful, negative if error
int wt_wlext_get_essid(char *ifname, char *essid, int *p_essid_len);

// Get BSSID.
// bssid - ptr where to store 6 bytes of BSSID MAC (unless NULL)
// bssid_str - ptr where to store BSSID string xx:xx:xx:xx:xx:xx (unless NULL)
// Returns: return 0 if successful, negative if error
int wt_wlext_get_bssid(char *ifname, unsigned char *bssid, char *bssid_str);

// Populate the VAPs array (ptr is passed in) w/ the
// VAP interface names for the `rinfo_num`
// Returns: the number of VAPs found
int wt_wlext_enum_vaps(int rinfo_num, char vaps[WIRELESS_MAX_VAPS][IFNAMSIZ]);

// Get operation mode.
// Returns: static string w/ mode name or NULL
char *wt_wlext_get_mode(char *ifname);

// Get Transmit Power (in dBm if 'indbm' is TRUE)
// Returns: max Tx power in dBm if successful, negative if fails
int wt_wlext_get_max_txpwr(char *ifname, int indbm);

// Get the channel.
// Returns: channel if successful, negative error code if fails
int wt_wlext_get_channel(char *ifname);

// Initializes wireless extensions
// Returns: 0 - success or negative int if error
int wt_wlext_init(void);

#endif // _WIRELESS_EXT_H
