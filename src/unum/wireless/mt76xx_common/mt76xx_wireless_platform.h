// (c) 2018 minim.co
// unum local platform specific include file for code that
// handles local platform specific stuff, it is included by
// .c files explicitly (not through common headers), it should
// only be included by files in this folder


#ifndef _MT76X0_WIRELESS_PLATFORM_H
#define _MT76X0_WIRELESS_PLATFORM_H

// Add types realtek uses
typedef unsigned short USHORT;
typedef unsigned char UCHAR;
typedef signed char CHAR;
typedef unsigned int UINT32;

#define WIFI_RADIO_24_PREFIX "ra"
#define WIFI_RADIO_5_PREFIX "rai"

// PHY modes
#define MODE_CCK          0
#define MODE_OFDM         1
#define MODE_HTMIX        2
#define MODE_HTGREENFIELD 3
#define MODE_VHT          4

// Rate table params
enum Rate_BW
{
        Rate_BW_20=0,
        Rate_BW_40,
        Rate_BW_80,
        Rate_BW_MAX
};

// MAC table structures
typedef union _MACHTTRANSMIT_SETTING {
        struct {
                USHORT MCS:7;
                USHORT BW:1;
                USHORT ShortGI:1;
                USHORT STBC:2;
                USHORT rsv:2;
                USHORT eTxBF:1;
                USHORT MODE:2;
        } field24;
        struct {
                USHORT MCS:6;
                USHORT ldpc:1;
                USHORT BW:2;
                USHORT ShortGI:1;
                USHORT STBC:1;
                USHORT eTxBF:1;
                USHORT iTxBF:1;
                USHORT MODE:3;
        } field5;
        USHORT word;
} MACHTTRANSMIT_SETTING, *PMACHTTRANSMIT_SETTING;

typedef struct _RT_802_11_MAC_ENTRY {
        UCHAR ApIdx;
        UCHAR Addr[6];
        UCHAR Aid;
        UCHAR Psm;    /* 0:PWR_ACTIVE, 1:PWR_SAVE */
        UCHAR MimoPs; /* 0:MMPS_STATIC, 1:MMPS_DYNAMIC, 3:MMPS_Enabled */
        CHAR AvgRssi0;
        CHAR AvgRssi1;
        CHAR AvgRssi2;
        UINT32 ConnectedTime;
        MACHTTRANSMIT_SETTING TxRate;
        // UINT32 LastRxRate; maybe
        // more stuff, total 40 bytes per entry
} RT_802_11_MAC_ENTRY, *PRT_802_11_MAC_ENTRY;


// Capture the STAs info for a radio interface (old drivers capture
// it for all radio VAPs)
// Returns: # of STAs - if successful, negative error otherwise
int __attribute__((weak)) wt_rt_get_stas_info(int radio_num, char *ifname);

// Capture the association(s) (normally 1) info for an interface
// Returns: # of asociations - if successful, negative error otherwise
int __attribute__((weak)) wt_rt_get_assoc_info(int radio_num, char *ifname);

#endif // _MT76X0_WIRELESS_PLATFORM_H
