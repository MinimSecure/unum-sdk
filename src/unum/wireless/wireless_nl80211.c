// (c) 2021 minim.co
// unum wireless extensions APIs, used only by platforms
// where drivers that work w/ nl80211 / mac80211 
// Note: all the functions here are designed to be called from
//       the wireless telemetry thread only

#include "unum.h"
#include "wireless_nl80211.h"

/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE

// Socket used to communicate with driver
struct nl_sock *nl_sock = NULL;
// NL80211 Identifier 
int driver_id;
struct handler_args {
    const char *group;
    int id;
};

// Differnt OUIs
static unsigned char ms_oui[3]          = { 0x00, 0x50, 0xf2 };
static unsigned char ieee80211_oui[3]   = { 0x00, 0x0f, 0xac };

// Next index entry into the scan entries table
int scan_index = 0;

// Init NL80211 socket to communicate with NL80211 / WLAN driver
static int wt_nl80211_init()
{
    int err;

    nl_sock = nl_socket_alloc();
    if (!nl_sock) {
        log("Failed to allocate netlink socket.\n");
        return -1;
    }

    if (genl_connect(nl_sock)) {
        log("%s: Failed to connect to generic netlink.\n", __func__);
        nl_socket_free(nl_sock);
        return -2;
    }

    nl_socket_set_buffer_size(nl_sock, 8192, 8192);

    /* try to set NETLINK_EXT_ACK to 1, ignoring errors */
    err = 1;
    if (setsockopt(nl_socket_get_fd(nl_sock), SOL_NETLINK,
            NETLINK_EXT_ACK, &err, sizeof(err)) != 0) {
        log("%s: Setting NETLINK_EXT_ACK failed\n", __func__);
    }

    driver_id = genl_ctrl_resolve(nl_sock, "nl80211");
    if (driver_id < 0) {
        log("%s: nl80211 not found.\n", __func__);
        nl_socket_free(nl_sock);
        return -3;
    }

    return 0;
}

// Cleanup everything allocated in init
static void wt_nl80211_cleanup()
{
    if (nl_sock != NULL) {
        nl_socket_free(nl_sock);
    }
}

// Print str into buf
// Don't print the string if there is an overflow
// Return updated length (if str is printed into buf)
static int wt_print_string(char *buf, int max_len, int offset, char *str)
{
    offset += snprintf(buf + offset, max_len - offset, "%s", str);
    return offset;
}

// Prints Cipher info into buf
// buf - Buffer to dump the data into
// buf_len - Length of the buffer
// data - Cipher info raw format from scan results
// Returns the number of bytes dumped into buf
static int wt_print_cipher(char *buf, int buf_len, const uint8_t *data)
{
    int buf_offset = 0;

    // Parsing is straight forward.
    // Depends on the OUI too.
    // Separate handler for each OUI.
    if (memcmp(data, ms_oui, 3) == 0) {
        switch (data[3]) {
        case 0:
            // Use same cipher suite as that of Group cipher
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                "GroupCipher");
            break;
        case 1:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                        "WEP-40");
            break;
        case 2:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                        "TKIP");
            break;
        case 4:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                        "CCMP");
            break;
        case 5:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                        "WEP-104");
            break;
        default:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                        "Unknown");
            break;
        }
    } else if (memcmp(data, ieee80211_oui, 3) == 0) {
        switch (data[3]) {
        case 0:
            // Use same cipher suite as that of Group cipher
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                "GroupCipher");
            break;
        case 1:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                        "WEP-40");
            break;
        case 2:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                        "TKIP");
            break;
        case 4:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                        "CCMP");
            break;
        case 5:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                        "WEP-104");
            break;
        case 6:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                        "AES-128-CMAC");
            break;
        case 7:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                        "NO-GROUP");
            break;
        case 8:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                        "GCMP"); 
            break;
        default:
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset,
                                                    "Unknown");
            break;
        }
    } else {
            buf_offset += wt_print_string(buf, buf_len - buf_offset, buf_offset, 
                                                    "Unknown");
    }

    return buf_offset;
}

// Prints Auth algorithm info into buf
// buf - Buffer to dump the data into
// buf_len - Length of the buffer
// data - Auth algorithm in raw format from scan results
// Returns the number of bytes dumped into buf
static int wt_print_auth(char *buf, int buf_len, const uint8_t *data)
{
    int buf_offset = 0;

    // Auth algorithm parsing is also straight forward
    if (memcmp(data, ms_oui, 3) == 0) {
        switch (data[3]) {
        case 1:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                            "802.1X");
            break;
        case 2:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                            "PSK");
            break;
        default:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "%.02x-%.02x-%.02x.%d",
                                data[0], data[1] ,data[2], data[3]);
            break;
        }
    } else if (memcmp(data, ieee80211_oui, 3) == 0) {
        switch (data[3]) {
        case 1:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "802.1X");
            break;
        case 2:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "PSK");
            break;
        case 3:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "802.1X");
            break;
        case 4:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "FT/PSK");
            break;
        case 5:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "802.1X/SHA-256");
            break;
        case 6:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "PSK/SHA-256");
            break;
        case 7:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "TDLS/TPK");
            break;
        case 8:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "SAE");
            break;
        case 9:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "FT/SAE");
            break;
        case 11:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "802.1X/SUITE-B");
            break;
        case 12:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "802.1X/SUITE-B-192");
            break;
        case 13:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "802.1X/SHA-384");
            break;
        case 14:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "FILS/SHA-256");
            break;
        case 15:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "FILS/SHA-384");
            break;
        case 16:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "FT/FILS/SHA-256");
            break;
        case 17:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "FT/FILS/SHA-384");
            break;
        case 18:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "OWE");
            break;
        default:
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                "%02x-%02x-%02x.%d",
                                data[0], data[1] ,data[2], data[3]);
            break;
        }
    } else {
         buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                        "%02x-%02x-%02x.%d",
                        data[0], data[1] ,data[2], data[3]);
    }

    return buf_offset;
}

// Parse an RSN element and dump details into buf
// buf - Buffer to dump the values into
// buf_len - Length of the buffer
// defcipher - Default  value for Cipher
// defauth - Default Authentication algorithm
// len - Length of the Information Element (from scan results)
// data - Information Element (from scan results)
static void wt_print_rsn_ie(char *buf, int buf_len, const char *defcipher,
                        const char *defauth, uint8_t len, const uint8_t *data)
{
    int i;
    __u16 count;
    int buf_offset = 0;

    /*
      Following is an example of how Group and Pairwise keys are used.
      {
          "bssid": "ec:ad:e0:df:50:20",
          "ssid": "6b736c766e65773246",
          "chan": 11,
          "rssi": -91,
          "extras": {
            "ch_width": "20",
            "encryption": "Group:TKIP Pairwise:TKIP/CCMP Auth:PSK"
          }
        },
     */
    // Skip T & L from TLV (Type Length Value)
    data += 2;
    len -= 2;

    if (len < 4) {
        // Too small. Skip
        buf_offset = snprintf(buf + buf_offset, buf_len,
                                        "Group:%s", defcipher);
        buf_offset += snprintf(buf + buf_offset, buf_len,
                                        " Pairwise:%s", defcipher);
        return;
    }

    // Group cipher, followed by Pairwise, followed by Auth algorithm
    // Start with Group Cipher
    buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset, "Group:");
    buf_offset += wt_print_cipher(buf + buf_offset, buf_len - buf_offset, data);

    data += 4;
    len -= 4;

    if (len < 2) {
        // Too small. Skip
        buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                        " Pairwise:%s", defcipher);
        return;
    }

    count = data[0] | (data[1] << 8);
    if (2 + (count * 4) > len) {
        // Too small. Skip
        return;
    }

    // Pairwise Cipher
    buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset, " Pairwise:");
    for (i = 0; i < count; i++) {
        if (i != 0) {
            // Add a / between each supported cipher
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset, "/");
        }
        buf_offset += wt_print_cipher(buf + buf_offset, buf_len - buf_offset,
                                    data + 2 + (i * 4));
    }

    data += 2 + (count * 4);
    len -= 2 + (count * 4);

    if (len < 2) {
        // Too small. Skip
        buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset,
                                            " Auth:%s", defauth);
        return;
    }

    count = data[0] | (data[1] << 8);
    if (2 + (count * 4) > len) {
        // Too small. Skip
        return;
    }

    // Auth algorithm
    buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset, " Auth:");
    for (i = 0; i < count; i++) {
        if (i != 0) {
            // Add a / between each supported cipher
            buf_offset += snprintf(buf + buf_offset, buf_len - buf_offset, "/");
        }
        buf_offset += wt_print_auth(buf + buf_offset, buf_len - buf_offset,
                                            data + 2 + (i * 4));
    }
}

// Print RSN information to buf.
// Actual processing is done in print_rsn_ie.
static void wt_print_rsn(char *buf, int buf_len, uint8_t len,
                    const uint8_t *data)
{
    wt_print_rsn_ie(buf, buf_len, "Unknown", "Unknown", len, data);
}

// Callback on error
static int wt_error_handler(struct sockaddr_nl *nla,
                            struct nlmsgerr *err, void *arg)
{
    struct nlmsghdr *nlh = (struct nlmsghdr *)err - 1;
    int len = nlh->nlmsg_len;
    struct nlattr *attrs;
    struct nlattr *tb[NLMSGERR_ATTR_MAX + 1];
    int *ret = arg;
    int ack_len = sizeof(*nlh) + sizeof(int) + sizeof(*nlh);

    *ret = err->error;

    if (!(nlh->nlmsg_flags & NLM_F_ACK_TLVS))
        return NL_STOP;

    if (!(nlh->nlmsg_flags & NLM_F_CAPPED))
        ack_len += err->msg.nlmsg_len - sizeof(*nlh);

    if (len <= ack_len)
        return NL_STOP;

    attrs = (void *)((unsigned char *)nlh + ack_len);
    len -= ack_len;

    nla_parse(tb, NLMSGERR_ATTR_MAX, attrs, len, NULL);
    if (tb[NLMSGERR_ATTR_MSG]) {
        len = strnlen((char *)nla_data(tb[NLMSGERR_ATTR_MSG]),
                              nla_len(tb[NLMSGERR_ATTR_MSG]));
        log("%s: scan error: %*s\n", __func__, len,
                        (char *)nla_data(tb[NLMSGERR_ATTR_MSG]));
    }
    return NL_STOP;
}

// Callback for NL_CB_FINISH.
static int wt_finish_handler(struct nl_msg *msg, void *arg)
{
    int *ret = arg;
    *ret = 0;
    return NL_SKIP;
}

// Callback for NL_CB_ACK.
static int wt_ack_handler(struct nl_msg *msg, void *arg)
{
    int *ret = arg;
    *ret = 0;
    return NL_STOP;
}

// Callback for NL_CB_SEQ_CHECK.
static int wt_no_seq_check(struct nl_msg *msg, void *arg)
{
    return NL_OK;
}

// Callback for NL_CB_VALID within nl_get_multicast_id().
static int wt_family_handler(struct nl_msg *msg, void *arg)
{
    struct handler_args *grp = arg;
    struct nlattr *tb[CTRL_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *mcgrp;
    int rem_mcgrp;

    nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
                            genlmsg_attrlen(gnlh, 0), NULL);

    if (!tb[CTRL_ATTR_MCAST_GROUPS]) {
        return NL_SKIP;
    }

    nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], rem_mcgrp) {
        struct nlattr *tb_mcgrp[CTRL_ATTR_MCAST_GRP_MAX + 1];

        nla_parse(tb_mcgrp, CTRL_ATTR_MCAST_GRP_MAX, nla_data(mcgrp),
                                                nla_len(mcgrp), NULL);

        if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME] ||
                              !tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]) {
            continue;
        }

        if (strncmp(nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]), grp->group,
                nla_len(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]))) {
            continue;
        }

        grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);
        break;
    }

    return NL_SKIP;
}

// Get the multicast id to listen for the events
// sock  - Nl80211 socket opened in init
// Family - Typically "nl80211"
// Group - Group where we are interested in, for example "scan"
// Returns multicast id on success, error otherwise
int wt_get_multicast_id(struct nl_sock *sock, const char *family,
                                    const char *group)
{
    struct nl_msg *msg;
    struct nl_cb *cb;
    int ret, ctrlid;
    struct handler_args grp = { .group = group, .id = -ENOENT, };

    msg = nlmsg_alloc();
    if (!msg) {
        log("%s: Error while allocating message \n", __func__);
        return -1;
    }

    cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb) {
        log("%s: Error while allocating callback \n", __func__);
        nlmsg_free(msg);
        return -2;
    }

    ctrlid = genl_ctrl_resolve(sock, "nlctrl");

    genlmsg_put(msg, 0, 0, ctrlid, 0, 0, CTRL_CMD_GETFAMILY, 0);

    ret = nla_put_string(msg, CTRL_ATTR_FAMILY_NAME, family);
    if (ret < 0) {
        log("%s: Error while setting family name\n", __func__);
        nl_cb_put(cb);
        nlmsg_free(msg);
        return -3;
    }

    ret = nl_send_auto_complete(sock, msg);
    if (ret < 0) {
        log("%s: Error while sending family name message \n", __func__);
        nl_cb_put(cb);
        nlmsg_free(msg);
        return -4;
    }

    ret = 1;
    nl_cb_err(cb, NL_CB_CUSTOM, wt_error_handler, &ret);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, wt_ack_handler, &ret);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, wt_family_handler, &grp);

    while (ret > 0) {
        nl_recvmsgs(sock, cb);
    }

    if (ret == 0) {
        ret = grp.id;
    }

    return ret;
}

// Walk through the IEs (Information Elements)
// We are interested in SSID, Channel width and Encryption algorithms
void wt_get_ies(unsigned char *ie, int ielen,
                    struct nl80211_scanlist_entry *entry)
{
    uint8_t len;
    uint8_t *data;

    while (ielen >= 2 && ielen >= ie[1]) {
        // A value of 0 ie[0] (type) is SSID
        if (ie[0] == 0 && ie[1] >= 0 && ie[1] <= 32) {
            len = ie[1];
            data = ie + 2;
            if (len > 64) {
                len = 64;
            }
            snprintf(entry->ssid, len + 1, "%s", data);
        } else if (ie[0] == 61) {
            // HT OP
            data = ie + 2;
            entry->ht_chwidth = (data[1] & 0x4)>>2;
            entry->ht_secondary = data[1] & 0x3;
        } else if (ie[0] == 192) {
            // VHT OP
            data = ie + 2;
            entry->vht_chwidth = data[0];
            entry->vht_secondary1 = data[1];
            entry->vht_secondary2 = data[2];
        } else if (ie[0] == 48) {
            // RSN OP
            data = ie + 2;
            wt_print_rsn(entry->enc_buf, ENC_BUF_SIZE, ie[1], data);
        }
        ielen -= ie[1] + 2;
        ie += ie[1] + 2;
    }
}

// Return channle number given frequency
int wt_frequency_to_channel(int freq)
{
    if (freq == 2484)
        return 14;
    else if (freq < 2484)
        return (freq - 2407) / 5;
    else if (freq >= 4910 && freq <= 4980)
        return (freq - 4000) / 5;
    else if (freq <= 45000) /* DMG band lower limit */
        return (freq - 5000) / 5;
    else if (freq >= 58320 && freq <= 64800)
        return (freq - 56160) / 2160;
    else
        return 0;
}

// Handle scan results returned from driver.
// Invoked as a callback for each scan result
// msg - Netlink message received from WLAN driver
// arg - Scanlist Buffer
static int wt_handle_scan_results(struct nl_msg *msg, void *arg)
{
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct nlattr *bss[NL80211_BSS_MAX + 1];
    // For now, we are interested only in the following
    static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
        [NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
        [NL80211_BSS_BSSID] = { },
        [NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
        [NL80211_BSS_INFORMATION_ELEMENTS] = { },
        [NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
    };
    char *scanbuf = (char *)arg;
    struct nl80211_scanlist_entry *entry;
    int crypto = 0;
    uint16_t caps;

    if ((sizeof(struct nl80211_scanlist_entry) * scan_index) > 
                                SCANLIST_BUF_SIZE) {
        log("%s: Can't handle these many entries for now. %d\n", scan_index);
        return NL_SKIP;
    }
    entry = (struct nl80211_scanlist_entry *)
              (scanbuf + (sizeof(struct nl80211_scanlist_entry) * scan_index));
    // Parse and error check.
    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
                                    genlmsg_attrlen(gnlh, 0), NULL);
    if (!tb[NL80211_ATTR_BSS]) {
        log("%s: BSSID is missing from scan results\n", __func__);
        return NL_SKIP;
    }
    if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS],
                                bss_policy)) {
        log("%s: failed to parse nested attributes!\n", __func__);
        return NL_SKIP;
    }

    // Check if BSSID is present
    if (!bss[NL80211_BSS_BSSID]) {
        return NL_SKIP;
    }
    // Check if IE's are present
    if (!bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
        return NL_SKIP;
    }
    // Check if Channel is present
    if (!bss[NL80211_BSS_FREQUENCY]) {
        return NL_SKIP;
    }
    // Check if Signal Strength is present
    if (!bss[NL80211_BSS_SIGNAL_MBM]) {
        return NL_SKIP;
    }

    if (bss[NL80211_BSS_CAPABILITY]) {
        caps = nla_get_u16(bss[NL80211_BSS_CAPABILITY]);
        if (caps & (1<<4)) {
            // Crypto enabled
            crypto = 1;
        }
    }

    entry->channel = wt_frequency_to_channel(
                                nla_get_u32(bss[NL80211_BSS_FREQUENCY]));
    entry->signal = nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
    entry->signal = entry->signal / 100;
    memcpy(entry->bssid, nla_data(bss[NL80211_BSS_BSSID]), ETHER_ADDR_LEN);
    wt_get_ies(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]),
                  nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]), entry);

    if (strlen(entry->enc_buf) == 0 && crypto) {
        // Crypto is enabled. But encryption buffer is empty
        // It is WEP
        strcpy(entry->enc_buf, "Group:WEP Pairwise:WEP Auth:Open/Shared");
    } else if (!crypto) {
        // No crypto
        strcpy(entry->enc_buf, "Group:Open Pairwise:Open Auth:Open");
    }
    scan_index++;

    return NL_SKIP;
}

// Called when the driver completes or aborts the scan
// Mark the status.done as accordingly ie 1 for aborted and 2 for completed
static int wt_scan_callback_handler(struct nl_msg *msg, void *arg)
{
    // Called by the kernel when the scan is done or has been aborted.
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct scan_status *status = arg;

    if (gnlh->cmd == NL80211_CMD_SCAN_ABORTED) {
        status->done = 1;
    } else if (gnlh->cmd == NL80211_CMD_NEW_SCAN_RESULTS) {
        status->done = 2;
    }

    return NL_SKIP;
}

// Send message to Kernel
// msg - Message (nl_msg) to be sent to the driver
// handler - callback from the driver
// data - Data that can be used by the callback
// is_done - Function specific to command to check whether the 
// driver completed its work. For eg with scan, we can know whether
// scan is completed
static int wt_send_message(struct nl_msg *msg,
                         int (*handler)(struct nl_msg *, void *), void *data,
                         int (*is_done)(void *))
{
    struct nl_cb *cb;
    int err;
    int ret;

    cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb) {
        log("%s: Error while allocating netlink callbacks.\n", __func__);
        return -1;
    }

    // Callback
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, handler, data);
    // Error and other handler
    nl_cb_err(cb, NL_CB_CUSTOM, wt_error_handler, &err);
    nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, wt_finish_handler, &err);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, wt_ack_handler, &err);
    nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, wt_no_seq_check, NULL);

    err = 1;
    // Issue the command
    ret = nl_send_auto_complete(nl_sock, msg);  // Send the message.

    while (err > 0) {
        ret = nl_recvmsgs(nl_sock, cb);
    }
    if (ret < 0) {
        log("%s: Error while invoking nl_recvmsgs() %d (%s).\n", __func__,
                                      ret, nl_geterror(-ret));
        return -2;
    }

    if (is_done) {
        // Wait until the driver replies back
        while (!is_done(data)) {
            nl_recvmsgs(nl_sock, cb);
        }
    }

    nl_cb_put(cb);
    return 0;
}

// Check whether scan is completed
// Returns 1 when scan is completed
// 0 Otherwise
int wt_is_scan_completed(void *data)
{
    struct scan_status *status = (struct scan_status *)data;
    if (status->done == 0) {
        return 0;
    }
    if (status->done) {
        log("%s: Scan is aborted for some reason\n", __func__);
    }
    return 1;
}

// Trigger scan and wait for its completion
// This will block until the scan is completed
// Returns 0 on success, error code otherwise
int wt_trigger_scan(int if_index)
{
    struct nl_msg *msg;
    int ret;
    int mcid;
    struct scan_status status;

    status.done = 0;
    mcid = wt_get_multicast_id(nl_sock, "nl80211", "scan");

    if (mcid < 0) {
        log("%s: Error while getting multicast id for nl80211 scan", __func__);
        return -1;
    }
    ret = nl_socket_add_membership(nl_sock, mcid);
    if (ret < 0) {
        log("%s: Error while adding multicast membership for scan\n", __func__);
        return -2;
    }

    // Allocate the messages and callback handler.
    msg = nlmsg_alloc();
    if (!msg) {
        log("%s: Error while allocating netlink message\n", __func__);
        return -3;
    }

    // Add scan command to message
    genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_TRIGGER_SCAN, 0);
    // Interface Index
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);

    // Send message to the driver and wait until the scan is completed
    // Wait is taken care in wt_scan_message
    // Once the scan is triggered, the wt_send_message function waits
    // until it hears from kernel. When the scan is completed,
    // wt_scan_callback_handler is called as callback and it breaks the
    // waiting loop in wt_send_message. wt_is_scan_completed polls
    // the status until wt_scan_callback_handler marks the status 
    // as done or aborted
    ret = wt_send_message(msg, wt_scan_callback_handler, &status,
                                                wt_is_scan_completed);
    if (ret < 0) {
        log("%s: Error while sending scan comand to kernel\n", __func__);
        nlmsg_free(msg);
        return -4;
    }
    // Cleanup.
    nlmsg_free(msg);
    nl_socket_drop_membership(nl_sock, mcid);  // No longer need this.
    return 0;
}

// Fetch scan results and put them into buffer
// Returns 0 success and error code on failure
int wt_get_scan_results(int if_index, char *buf)
{
    struct nl_msg *msg;
    // Allocate the messages and callback handler.
    msg = nlmsg_alloc();
    if (!msg) {
        log("%s: Error while allocating netlink message\n", __func__);
        return -1;
    }

    // Get scan results command
    genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0);
    // Interface Index
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);

    // Set callback
    // wt_handle_scan_results is called for every scan result
    nl_socket_modify_cb(nl_sock, NL_CB_VALID, NL_CB_CUSTOM,
                                    wt_handle_scan_results, buf);

    // Send message to the driver
    int ret = nl_send_auto_complete(nl_sock, msg);
    ret = nl_recvmsgs_default(nl_sock); 
    if (ret < 0) {
        log("%s: nl_recvmsgs_default() returned %d (%s).\n", __func__, ret,
                                            nl_geterror(-ret));
        return -2;
    }
    nlmsg_free(msg);
    return 0;
}

// Main function for scan
// ifname - Interface name
// Buf - Scanlist bufer
// Returns - 0 on success and error code on failure
int wt_nl80211_get_scan(char *ifname, char *buf)
{
    // Init nl80211 socket
    int err = wt_nl80211_init();
    int if_index;

    if (err != 0) {
        return -1;
    }

    // Get the Interface index
    if_index = if_nametoindex(ifname);
    if (if_index == 0) {
        log("%s: Unable to get index for the interface %s\n", __func__, ifname);
        wt_nl80211_cleanup();
        return -2;
    }

    // Trigger scan
    // This will block until the scan is completed
    err = wt_trigger_scan(if_index);
    if (err != 0) {
        log("%s: Unable to trigger scan for %s\n", __func__, ifname);
        wt_nl80211_cleanup();
        return -3;
    } 

    scan_index = 0;
    // Get scan results
    err = wt_get_scan_results(if_index, buf);
    if (err != 0) {
        log("%s: Unable to get scan results for the interface %s\n",
                                            __func__, ifname);
        wt_nl80211_cleanup();
        return -3;
    } 

    // Cleanup before exit
    wt_nl80211_cleanup();
    return scan_index;
}

// Callback for country code
// msg - Netlink message received from WLAN driver
// arg - Scanlist Buffer
static int wt_handle_country_code(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

    // parse the message
    nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
                genlmsg_attrlen(gnlh, 0), NULL);

    if (!tb_msg[NL80211_ATTR_WIPHY] && tb_msg[NL80211_ATTR_REG_ALPHA2]) {
        char *country = nla_data(tb_msg[NL80211_ATTR_REG_ALPHA2]);
        if (arg) {
            memcpy(arg, country, 2);
            ((char *)arg)[2] = 0;
        } else {
            log("%s: country code callback arg is NULL\n", __func__);
        }
    }
    return 0;
}

// Generic nl80211 command function
// Returns 0 success and error code on failure
int wt_handle_cmd(int if_index, int (*cb_func)(struct nl_msg *, void *),
                                    void *cb_arg)
{
    struct nl_msg *msg;

    // Allocate the messages and callback handler.
    msg = nlmsg_alloc();
    if (!msg) {
        log("%s: Error while allocating netlink message\n", __func__);
        return -1;
    }

    // Issue command to get country code
    genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_REG, 0);
    // Interface Index
    if (if_index > -1) {
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);
    }

    // Send message to the driver
    int ret = wt_send_message(msg, cb_func, cb_arg,
                                                NULL);
    if (ret < 0) {
        log("%s: Error while sending scan comand to kernel\n", __func__);
        nlmsg_free(msg);
        return -2;
    }

    nlmsg_free(msg);
    return 0;
}

// Main function for country code
// ifname - Interface name
// Returns - 0 on success and error code on failure
char* wt_nl80211_get_country(char *ifname)
{
    static char cc[32];
    // Init nl80211 socket
    int err = wt_nl80211_init();

    if (err != 0) {
        return NULL;
    }

    wt_handle_cmd(-1, wt_handle_country_code, cc);
    // Cleanup before exit
    wt_nl80211_cleanup();
    return cc;
}
