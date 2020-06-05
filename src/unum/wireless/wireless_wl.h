// (c) 2018-2019 minim.co
// unum wl driver APIs include file (shared by platforms
// based on Broadcom's wl drivers)
/*
 * Custom OID/ioctl definitions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Definitions subject to change without notice.
 *
 * Copyright (C) 2016, Broadcom Corporation. All Rights Reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: wlioctl_defs.h 403826 2013-05-22 16:40:55Z $
 */

#ifndef _WIRELESS_WL_H
#define _WIRELESS_WL_H


// Byte swap defines for data exchange between BRCM chip and the CPU
#define dtoh64(x) (wl_need_byte_swap ? __bswap_64(x) : (x))
#define htod64(x) (wl_need_byte_swap ? __bswap_64(x) : (x))
#define dtoh32(x) (wl_need_byte_swap ? __bswap_32(x) : (x))
#define htod32(x) (wl_need_byte_swap ? __bswap_32(x) : (x))
#define dtoh16(x) (wl_need_byte_swap ? __bswap_16(x) : (x))
#define htod16(x) (wl_need_byte_swap ? __bswap_16(x) : (x))

#define WL_STA_VER_MAX 8

// external flag wl sets at init if data from driver need the byte swap
extern int wl_need_byte_swap;


#ifdef WIRELESS_WL_NO_LIBSHARED
// Generic Broadcom's ioctl function
// ifname - interface name
// cmd - IOCTL command code
// buf - buffer for/with data
// len - buffer size
// Returns: 0 - ok, negative - error
int wl_ioctl(const char *ifname, int cmd, void *buf, int len);

// Generic Broadcom's WLC_GET_VAR ioctl command helper
// ifname - interface name
// cmd - text command accepted by WLC_GET_VAR
// arg - command data (can be binary or text)
// arglen - command data length
// buf - buffer where to store the cmd & arg and where the data will be returned
// len - buffer size
// Returns: 0 - ok, negative - error
int wl_iovar_getbuf(const char *ifname, const char *cmd, void *arg,
                    int arglen, void *buf, int buflen);

// Another generic Broadcom's WLC_GET_VAR ioctl command helper
// ifname - interface name
// cmd - text command accepted by WLC_GET_VAR
// buf - buffer where to store the cmd & arg and where the data will be returned
// buflen - buffer size
// Returns: 0 - ok, negative - error
int wl_iovar_get(char *ifname, char *cmd, void *buf, int buflen);

// This is based on bcmwifi_channels.c (licensed as redistributable)
/*
 * This function returns the channel number that control traffic is being sent on, for 20MHz
 * channels this is just the channel number, for 40MHZ, 80MHz, 160MHz channels it is the 20MHZ
 * sideband depending on the chanspec selected
 */
unsigned char wf_chspec_ctlchan(chanspec_t chspec);
#endif // WIRELESS_WL_NO_LIBSHARED

// Return the name of the radio (from its number)
char *wt_get_radio_name(int ii);

// Returns the # of the radio the interface belongs to
// (radio 0 - 2.4GHz, 1 - 5GHz), -1 if not a wireless
// interface
int wt_get_ifname_radio_num(char *ifname);

// Return the name of the VAP interface given its radio # and VAP #.
// Returns NULL if radio # or VAP # is out of range.
char *wt_get_vap_name(int radio_num, int ii);

// Returns the # of the radio and the VAP the interface belongs to
// (radio 0 - 2.4GHz, 1 - 5GHz), -1 if not a wireless interface
int wt_get_ifname_radio_vap_num(char *ifname, int *p_radio_num);

// Retrieve in cspec the chanspec from wl_bss_info_t structure
// (the same way Broadcom does it)
// The return value is negative if the function fails.
int wl_chanspec_from_bssinfo(wl_bss_info_t *bi, chanspec_t *cspec);

// Extracts readable chanspec description string and channel width integer.
// Returns: channel width integer,
//          fills in buf w/ chanspec str (must be of CHANSPEC_STR_LEN),
//          should never fail (but might return a mess)
int wl_get_specstr_and_width(chanspec_t chspec, char *buf);

// Pointer to wireless telemetry temp buffer. Should only be used in the
// wireless telemetry main thread. It is of WLC_IOCTL_MAXLEN, so can be used
// in all IOCTLs. It is allocated at init and is never freed.
void *wt_get_ioctl_tmp_buf(void);

// Get association list for a VAP interface, write it into buf, the buf has to
// be at least WLC_IOCTL_MAXLEN bytes long. It is filled in with "maclist_t".
// The first 32 bit word - "count", the rest is "struct ether_addr" array "ea".
// Returns: the "count" if successful, negative if fails
int wl_get_stations(char *ifname, void *sta_info_buf);

// Retrieve, verify and return STA info in the supplied buffer.
// The buffer has to be at least WLC_IOCTL_MEDLEN bytes long.
// The function does the byte swap to covert the host byte order.
// Returns: negative if fails, zero if successful
int wl_get_sta_info(char *ifname, unsigned char *mac, void *buf);

// Initializes wl subsystem
// Returns: 0 - success or negative int if error
int wt_wl_init(void);

#endif // _WIRELESS_WL_H
