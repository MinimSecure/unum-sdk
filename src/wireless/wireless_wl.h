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

// unum wl driver APIs include file (shared by platforms
// based on Broadcom's wl drivers)

#ifndef _WIRELESS_WL_H
#define _WIRELESS_WL_H


// Byte swap defines for data exchange between BRCM chip and the CPU
#define dtoh64(x) (wl_need_byte_swap ? __bswap_64(x) : (x))
#define htod64(x) (wl_need_byte_swap ? __bswap_64(x) : (x))
#define dtoh32(x) (wl_need_byte_swap ? __bswap_32(x) : (x))
#define htod32(x) (wl_need_byte_swap ? __bswap_32(x) : (x))
#define dtoh16(x) (wl_need_byte_swap ? __bswap_16(x) : (x))
#define htod16(x) (wl_need_byte_swap ? __bswap_16(x) : (x))


// external flag wl sets at init if data from driver need the byte swap
extern int wl_need_byte_swap;


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
