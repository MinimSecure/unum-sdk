/*
 * Header bits derived from MadWifi source:
 *   Copyright (c) 2001 Atsushi Onoe
 *   Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 *   All rights reserved.
 *
 * Distributed under the terms of the GPLv2 license.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MADWIFI_H
#define _MADWIFI_H

/* ieee80211.h */
#define	IEEE80211_ADDR_LEN			6
#define	IEEE80211_RATE_VAL			0x7f
#define	IEEE80211_SEQ_SEQ_MASK		0xfff0
#define	IEEE80211_SEQ_SEQ_SHIFT		4


/* ieee80211_crypto.h */
#define	IEEE80211_KEYBUF_SIZE		16
#define	IEEE80211_MICBUF_SIZE		16
#define IEEE80211_TID_SIZE			17

#define	IEEE80211_CIPHER_WEP		0
#define	IEEE80211_CIPHER_TKIP		1
#define	IEEE80211_CIPHER_AES_OCB	2
#define	IEEE80211_CIPHER_AES_CCM	3
#define	IEEE80211_CIPHER_CKIP		5
#define	IEEE80211_CIPHER_NONE		6
#define	IEEE80211_CIPHER_MAX		(IEEE80211_CIPHER_NONE + 1)


/* ieee80211_ioctl.h */
#define	IEEE80211_KEY_DEFAULT		0x80
#define	IEEE80211_CHAN_MAX			255
#define	IEEE80211_CHAN_BYTES		32
#define	IEEE80211_RATE_MAXSIZE		44

#define	IEEE80211_IOCTL_GETKEY		(SIOCDEVPRIVATE+3)
#define	IEEE80211_IOCTL_STA_STATS	(SIOCDEVPRIVATE+5)
#define	IEEE80211_IOCTL_STA_INFO	(SIOCDEVPRIVATE+6)

#define	IEEE80211_IOCTL_GETPARAM	(SIOCIWFIRSTPRIV+1)
#define	IEEE80211_IOCTL_GETMODE		(SIOCIWFIRSTPRIV+3)
#define	IEEE80211_IOCTL_GETCHANLIST	(SIOCIWFIRSTPRIV+7)
#define	IEEE80211_IOCTL_GETCHANINFO	(SIOCIWFIRSTPRIV+7)

#define	SIOC80211IFCREATE			(SIOCDEVPRIVATE+7)
#define	SIOC80211IFDESTROY	 		(SIOCDEVPRIVATE+8)

#define	IEEE80211_CLONE_BSSID	0x0001	/* allocate unique mac/bssid */
#define	IEEE80211_NO_STABEACONS	0x0002	/* Do not setup the station beacon timers */

#define IEEE80211_HTCAP_C_ADVCODING             0x0001
#define IEEE80211_HTCAP_C_CHWIDTH40             0x0002
#define IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC    0x0000 /* Capable of SM Power Save (Static) */
#define IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC   0x0004 /* Capable of SM Power Save (Dynamic) */
#define IEEE80211_HTCAP_C_SM_RESERVED           0x0008 /* Reserved */
#define IEEE80211_HTCAP_C_SMPOWERSAVE_DISABLED            0x000c /* SM enabled, no SM Power Save */
#define IEEE80211_HTCAP_C_GREENFIELD            0x0010
#define IEEE80211_HTCAP_C_SHORTGI20             0x0020
#define IEEE80211_HTCAP_C_SHORTGI40             0x0040
#define IEEE80211_HTCAP_C_TXSTBC                0x0080
#define IEEE80211_HTCAP_C_TXSTBC_S                   7
#define IEEE80211_HTCAP_C_RXSTBC                0x0300  /* 2 bits */
#define IEEE80211_HTCAP_C_RXSTBC_S                   8
#define IEEE80211_HTCAP_C_DELAYEDBLKACK         0x0400
#define IEEE80211_HTCAP_C_MAXAMSDUSIZE          0x0800  /* 1 = 8K, 0 = 3839B */
#define IEEE80211_HTCAP_C_DSSSCCK40             0x1000
#define IEEE80211_HTCAP_C_PSMP                  0x2000
#define IEEE80211_HTCAP_C_INTOLERANT40          0x4000
#define IEEE80211_HTCAP_C_LSIGTXOPPROT          0x8000
#define IEEE80211_ELEMID_VENDOR 221
#define WME_OUI                 0xf25000
#define WME_OUI_TYPE            0x02
#define MAX_CHAINS 4
#define WME_NUM_AC      4

#define LE_READ_4(p)                                    \
	((u_int32_t)                                    \
	 ((((const u_int8_t *)(p))[0]      ) |          \
	  (((const u_int8_t *)(p))[1] <<  8) |          \
	  (((const u_int8_t *)(p))[2] << 16) |          \
	  (((const u_int8_t *)(p))[3] << 24)))


struct ieee80211_clone_params {
	char icp_name[IFNAMSIZ];		/* device name */
	u_int16_t icp_opmode;			/* operating mode */
	u_int16_t icp_flags;			/* see below */
};

enum ieee80211_opmode {
	IEEE80211_M_STA		= 1,	/* infrastructure station */
	IEEE80211_M_IBSS 	= 0,	/* IBSS (adhoc) station */
	IEEE80211_M_AHDEMO	= 3,	/* Old lucent compatible adhoc demo */
	IEEE80211_M_HOSTAP	= 6,	/* Software Access Point */
	IEEE80211_M_MONITOR	= 8,	/* Monitor mode */
	IEEE80211_M_WDS		= 2,	/* WDS link */
};

enum {
	IEEE80211_PARAM_AUTHMODE		= 3,	/* authentication mode */
	IEEE80211_PARAM_MCASTCIPHER		= 5,	/* multicast/default cipher */
	IEEE80211_PARAM_MCASTKEYLEN		= 6,	/* multicast key length */
	IEEE80211_PARAM_UCASTCIPHERS	= 7,	/* unicast cipher suites */
	IEEE80211_PARAM_WPA				= 10,	/* WPA mode (0,1,2) */
};

/*
 * Authentication mode.
 */
enum ieee80211_authmode {
	IEEE80211_AUTH_NONE	= 0,
	IEEE80211_AUTH_OPEN	= 1,	/* open */
	IEEE80211_AUTH_SHARED	= 2,	/* shared-key */
	IEEE80211_AUTH_8021X	= 3,	/* 802.1x */
	IEEE80211_AUTH_AUTO	= 4,	/* auto-select/accept */
	/* NB: these are used only for ioctls */
	IEEE80211_AUTH_WPA	= 5,	/* WPA/RSN w/ 802.1x/PSK */
};

struct ieee80211_channel {
	u_int16_t ic_freq;	/* setting in MHz */
	u_int32_t ic_flags;	/* see below */
	u_int8_t        ic_flagext;
	u_int8_t ic_ieee;	/* IEEE channel number */
	int8_t ic_maxregpower;	/* maximum regulatory tx power in dBm */
	int8_t ic_maxpower;	/* maximum tx power in dBm */
	int8_t ic_minpower;	/* minimum tx power in dBm */
	u_int8_t        ic_regClassId;  /* regClassId of this channel */
	u_int8_t        ic_antennamax;  /* antenna gain max from regulatory */
	u_int8_t        ic_vhtop_ch_freq_seg1;         /* Channel Center frequency */
	u_int8_t        ic_vhtop_ch_freq_seg2;     
};

struct ieee80211req_key {
	u_int8_t ik_type;		/* key/cipher type */
	u_int8_t ik_pad;
	u_int16_t ik_keyix;	/* key index */
	u_int8_t ik_keylen;		/* key length in bytes */
	u_int8_t ik_flags;
	u_int8_t ik_macaddr[IEEE80211_ADDR_LEN];
	u_int64_t ik_keyrsc;		/* key receive sequence counter */
	u_int64_t ik_keytsc;		/* key transmit sequence counter */
	u_int8_t ik_keydata[IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE];
};

struct ieee80211req_chanlist {
	u_int8_t ic_channels[IEEE80211_CHAN_BYTES];
};

struct ieee80211req_chaninfo {
	u_int ic_nchans;
	struct ieee80211_channel ic_chans[IEEE80211_CHAN_MAX];
};

struct ieee80211req_sta_info {
	u_int16_t       isi_len;                /* length (mult of 4) */
	u_int16_t       isi_freq;               /* MHz */
	u_int32_t       awake_time;             /* time is active mode */
	u_int32_t       ps_time;                /* time in power save mode */
	u_int32_t   isi_flags;      /* channel flags */
	u_int16_t       isi_state;              /* state flags */
	u_int8_t        isi_authmode;           /* authentication algorithm */
	int8_t          isi_rssi;
	int8_t          isi_min_rssi;
	int8_t          isi_max_rssi;
	u_int16_t       isi_capinfo;            /* capabilities */
	u_int8_t        isi_athflags;           /* Atheros capabilities */
	u_int8_t        isi_erp;                /* ERP element */
	u_int8_t    isi_ps;         /* psmode */
	u_int8_t        isi_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t        isi_nrates;
	/* negotiated rates */
	u_int8_t        isi_rates[IEEE80211_RATE_MAXSIZE];
	u_int8_t        isi_txrate;             /* index to isi_rates[] */
	u_int32_t   isi_txratekbps; /* tx rate in Kbps, for 11n */
	u_int16_t       isi_ie_len;             /* IE length */
	u_int16_t       isi_associd;            /* assoc response */
	u_int16_t       isi_txpower;            /* current tx power */
	u_int16_t       isi_vlan;               /* vlan tag */
	u_int16_t       isi_txseqs[17];         /* seq to be transmitted */
	u_int16_t       isi_rxseqs[17];         /* seq previous for qos frames*/
	u_int16_t       isi_inact;              /* inactivity timer */
	u_int8_t        isi_uapsd;              /* UAPSD queues */
	u_int8_t        isi_opmode;             /* sta operating mode */
	u_int8_t        isi_cipher;
	u_int32_t       isi_assoc_time;         /* sta association time */
	struct timespec isi_tr069_assoc_time;   /* sta association time in timespec format */


	u_int16_t   isi_htcap;      /* HT capabilities */
	u_int32_t   isi_rxratekbps; /* rx rate in Kbps */
	/* We use this as a common variable for legacy rates
	   and lln. We do not attempt to make it symmetrical
	   to isi_txratekbps and isi_txrate, which seem to be
	   separate due to legacy code. */
	/* XXX frag state? */
	/* variable length IE data */
	u_int8_t isi_maxrate_per_client; /* Max rate per client */
	u_int16_t   isi_stamode;        /* Wireless mode for connected sta */
	u_int32_t isi_ext_cap;              /* Extended capabilities */
	u_int8_t isi_nss;         /* number of tx and rx chains */
	u_int8_t isi_is_256qam;    /* 256 QAM support */
};

/*
 * Per/node (station) statistics available when operating as an AP.
 */
struct ieee80211_nodestats {
	u_int32_t    ns_rx_data;             /* rx data frames */
	u_int32_t    ns_rx_mgmt;             /* rx management frames */
	u_int32_t    ns_rx_ctrl;             /* rx control frames */
	u_int32_t    ns_rx_ucast;            /* rx unicast frames */
	u_int32_t    ns_rx_mcast;            /* rx multi/broadcast frames */
	u_int64_t    ns_rx_bytes;            /* rx data count (bytes) */
	u_int64_t    ns_last_per;            /* last packet error rate */
	u_int64_t    ns_rx_beacons;          /* rx beacon frames */
	u_int32_t    ns_rx_proberesp;        /* rx probe response frames */

	u_int32_t    ns_rx_dup;              /* rx discard 'cuz dup */
	u_int32_t    ns_rx_noprivacy;        /* rx w/ wep but privacy off */
	u_int32_t    ns_rx_wepfail;          /* rx wep processing failed */
	u_int32_t    ns_rx_demicfail;        /* rx demic failed */

	u_int32_t    ns_rx_tkipmic;          /* rx TKIP MIC failure */
	u_int32_t    ns_rx_ccmpmic;          /* rx CCMP MIC failure */
	u_int32_t    ns_rx_wpimic;           /* rx WAPI MIC failure */
	u_int32_t    ns_rx_tkipicv;          /* rx ICV check failed (TKIP) */
	u_int32_t    ns_rx_decap;            /* rx decapsulation failed */
	u_int32_t    ns_rx_defrag;           /* rx defragmentation failed */
	u_int32_t    ns_rx_disassoc;         /* rx disassociation */
	u_int32_t    ns_rx_deauth;           /* rx deauthentication */
	u_int32_t    ns_rx_action;           /* rx action */
	u_int32_t    ns_rx_decryptcrc;       /* rx decrypt failed on crc */
	u_int32_t    ns_rx_unauth;           /* rx on unauthorized port */
	u_int32_t    ns_rx_unencrypted;      /* rx unecrypted w/ privacy */

	u_int32_t    ns_tx_data;             /* tx data frames */
	u_int32_t    ns_tx_data_success;     /* tx data frames successfully
						transmitted (unicast only) */
	u_int64_t    ns_tx_wme[4];           /* tx data per AC */
	u_int64_t    ns_rx_wme[4];           /* rx data per AC */
	u_int32_t    ns_tx_mgmt;             /* tx management frames */
	u_int32_t    ns_tx_ucast;            /* tx unicast frames */
	u_int32_t    ns_tx_mcast;            /* tx multi/broadcast frames */
	u_int64_t    ns_tx_bytes;            /* tx data count (bytes) */
	u_int64_t    ns_tx_bytes_success;    /* tx success data count - unicast only
						(bytes) */
	u_int32_t    ns_tx_probereq;         /* tx probe request frames */
	u_int32_t    ns_tx_uapsd;            /* tx on uapsd queue */
	u_int32_t    ns_tx_discard;          /* tx dropped by NIC */
	u_int32_t    ns_is_tx_not_ok;        /* tx not ok */
	u_int32_t    ns_tx_novlantag;        /* tx discard 'cuz no tag */
	u_int32_t    ns_tx_vlanmismatch;     /* tx discard 'cuz bad tag */

	u_int32_t    ns_tx_eosplost;         /* uapsd EOSP retried out */

	u_int32_t    ns_ps_discard;          /* ps discard 'cuz of age */

	u_int32_t    ns_uapsd_triggers;      /* uapsd triggers */
	u_int32_t    ns_uapsd_duptriggers;    /* uapsd duplicate triggers */
	u_int32_t    ns_uapsd_ignoretriggers; /* uapsd duplicate triggers */
	u_int32_t    ns_uapsd_active;         /* uapsd duplicate triggers */
	u_int32_t    ns_uapsd_triggerenabled; /* uapsd duplicate triggers */
	u_int32_t    ns_last_tx_rate;         /* last tx data rate */
	u_int32_t    ns_last_rx_rate;         /* last received data frame rate */
	u_int32_t    ns_is_tx_nobuf;
	u_int32_t    ns_last_rx_mgmt_rate;    /* last received mgmt frame rate */
	int8_t       ns_rx_mgmt_rssi;         /* mgmt frame rssi */
	u_int32_t    ns_dot11_tx_bytes;
	u_int32_t    ns_dot11_rx_bytes;

	u_int32_t    ns_tx_assoc;            /* [re]associations */
	u_int32_t    ns_tx_assoc_fail;       /* [re]association failures */
	u_int32_t    ns_tx_auth;             /* [re]authentications */
	u_int32_t    ns_tx_auth_fail;        /* [re]authentication failures*/
	u_int32_t    ns_tx_deauth;           /* deauthentications */
	u_int32_t    ns_tx_deauth_code;      /* last deauth reason */
	u_int32_t    ns_tx_disassoc;         /* disassociations */
	u_int32_t    ns_tx_disassoc_code;    /* last disassociation reason */
	u_int32_t    ns_psq_drops;           /* power save queue drops */
	u_int32_t    ns_rssi_chain[MAX_CHAINS]; /* Ack RSSI per chain */
	u_int32_t    inactive_time;
	u_int32_t    ns_tx_mu_blacklisted_cnt;  /* number of time MU tx has been blacklisted */
	u_int64_t    ns_excretries[WME_NUM_AC]; /* excessive retries */
};

/*
 * Retrieve per-node statistics.
 */
struct ieee80211req_sta_stats {
	union {
		/* NB: explicitly force 64-bit alignment */
		u_int8_t macaddr[IEEE80211_ADDR_LEN];
		u_int64_t pad;
	} is_u;
	struct ieee80211_nodestats is_stats;
};

#endif
