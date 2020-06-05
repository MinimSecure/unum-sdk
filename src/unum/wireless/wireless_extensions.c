// (c) 2017-2019 minim.co
// unum wireless extensions APIs, used only by platforms
// w/ wireless extensions based drivers

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Here we store the socket descriptor all the functions needing
// wireless extensions should use.
static int skfd = -1;

// Private IOCTLs buffer (WT_WLEXT_MAX_PRIV_PAYLOAD size)
static unsigned char *buffer = NULL;


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
                         void *buf, int buf_size)
{
    int ret;
    struct iwreq wrq;

    memset(&wrq, 0, sizeof(wrq));
    wrq.u.data.pointer = buffer;
    wrq.u.data.length = 0;
    wrq.u.data.flags = subcmd;
    *((char *)buffer) = 0;
    strncpy(wrq.ifr_name, ifname, IFNAMSIZ);

    // Perform the private ioctl
    ret = ioctl(skfd, icmd, &wrq);
    if(ret < 0)
    {
        log("%s: failed ioctl 0x%x for <%s>: %s\n",
            __func__, icmd, ifname, strerror(errno));
        return -1;
    }

    ret = wrq.u.data.length;
    if(buf && buf_size > 0 && ret > 0) {
        int len = (ret < buf_size) ? ret : buf_size;
        memcpy(buf, buffer, len);
        ret = len;
    }

    return ret;
}

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
int wlext_priv_cmd(int sock, char *ifname, char *cmd, void *buf, int buf_size)
{
    iwprivargs *priv;
    int priv_num;
    char *arg;
    int ret;
    int close_sock = FALSE;

    if(!ifname || !cmd || *cmd == 0) {
        return -1;
    }
    
    if(sock < 0) {
        sock = iw_sockets_open();
        if(sock < 0) {
            log("%s: iw_sockets_open() error: %s", __func__, strerror(errno));
            return -2;
        }
        close_sock = TRUE;
    }

    // Read the private ioctls
    priv_num = iw_get_priv_info(sock, ifname, &priv);

    for(;;)
    {
        if(priv_num <= 0)
        {
            ret = -3;
            break;
        }

        // cmd points to the command, arg to the following arguments
        arg = cmd + strlen(cmd) + 1;

        // Have to follow what iwpriv does... crazy
        struct iwreq wrq;
        int i = 0;        /* Start with first command arg */
        int k;            /* Index in private description table */
        int temp;
        int subcmd = 0;   /* sub-ioctl index */
        int offset = 0;   /* Space for sub-ioctl index */
        char *tmp;

        // Check if we have a token index
        if(*arg != 0 && (sscanf(arg, "[%i]", &temp) == 1))
        {
            subcmd = temp;
            arg += strlen(arg) + 1;
        }

        // Search the correct ioctl
        k = -1;
        while((++k < priv_num) && strcmp(priv[k].name, cmd));

        // If not found...
        if(k == priv_num)
        {
            log("%s: no private command <%s>\n", __func__, cmd);
            ret = -4;
            break;
        }

        // Watch out for sub-ioctls
        if(priv[k].cmd < SIOCDEVPRIVATE)
        {
            int j = -1;

            // Find the matching *real* ioctl
            while((++j < priv_num) && ((priv[j].name[0] != '\0') ||
                                       (priv[j].set_args != priv[k].set_args) ||
                                       (priv[j].get_args != priv[k].get_args)));

            // If not found...
            if(j == priv_num)
            {
                 log("%s: invalid private ioctl definition for <%s>\n", cmd);
                 ret = -5;
                 break;
            }

            // Save sub-ioctl number
            subcmd = priv[k].cmd;
            // Reserve one int (simplify alignment issues)
            offset = sizeof(int);
            // Use real ioctl definition from now on
            k = j;
        }

        // If we have to set some data
        if((priv[k].set_args & IW_PRIV_TYPE_MASK) &&
           (priv[k].set_args & IW_PRIV_SIZE_MASK))
        {
            ret = 0;
            switch(priv[k].set_args & IW_PRIV_TYPE_MASK)
            {
                case IW_PRIV_TYPE_BYTE:
                    // Number of args to fetch
                    wrq.u.data.length = priv[k].set_args & IW_PRIV_SIZE_MASK;

                    // Fetch args
                    for(tmp = arg;
                        i < wrq.u.data.length && *tmp &&
                            (i + 1) * sizeof(char) <= buf_size;
                        ++i, tmp += strlen(tmp) + 1)
                    {
                        sscanf(tmp, "%i", &temp);
                        ((char *)buf)[i] = (char)temp;
                    }
                    wrq.u.data.length = i;
                    break;

                case IW_PRIV_TYPE_INT:
                    // Number of args to fetch
                    wrq.u.data.length = priv[k].set_args & IW_PRIV_SIZE_MASK;

                    // Fetch args
                    for(tmp = arg;
                        i < wrq.u.data.length && *tmp &&
                            (i + 1) * sizeof(int) <= buf_size;
                        ++i, tmp += strlen(tmp) + 1)
                    {
                        sscanf(tmp, "%i", &temp);
                        ((int *)buf)[i] = (int)temp;
                    }
                    wrq.u.data.length = i;
                    break;

                case IW_PRIV_TYPE_CHAR:
                    if(*arg != 0)
                    {
                        // Size of the string to fetch
                        wrq.u.data.length = strlen(arg) + 1;
                        if(wrq.u.data.length >
                           (priv[k].set_args & IW_PRIV_SIZE_MASK))
                        {
                            wrq.u.data.length =
                               priv[k].set_args & IW_PRIV_SIZE_MASK;
                        }
                        if(wrq.u.data.length > buf_size) {
                            wrq.u.data.length = buf_size;
                        }
                        // Fetch string
                        memcpy(buf, arg, wrq.u.data.length);
                        ((char *)buf)[wrq.u.data.length - 1] = 0;
                    }
                    else if(buf_size > 0)
                    {
                        wrq.u.data.length = 1;
                        ((char *)buf)[0] = '\0';
                    }
                    else {
                        wrq.u.data.length = 0;
                    }
                    break;

                case IW_PRIV_TYPE_FLOAT:
                    log("%s: float paramters are not supported\n");
                    ret = -6;
                    break;

                case IW_PRIV_TYPE_ADDR:
                    // Number of args to fetch
                    wrq.u.data.length = priv[k].set_args & IW_PRIV_SIZE_MASK;

                    // Fetch args
                    for(tmp = arg;
                        i < wrq.u.data.length && *tmp &&
                            (i + 1) * sizeof(struct sockaddr) <= buf_size;
                        ++i, tmp += strlen(tmp) + 1)
                    {
                        if(iw_in_addr(sock, ifname, tmp,
                                      ((struct sockaddr *)buf) + i) < 0)
                        {
                            log("%s: invalid address <%s>\n", __func__, tmp);
                            ret = -7;
                        }
                    }
                    wrq.u.data.length = i;
                    break;

                default:
                    log("%s: unsupported data type %d\n",
                        __func__, priv[k].set_args & IW_PRIV_TYPE_MASK);
                    ret = -8;
                    break;
            }

            if(ret < 0) {
                break;
            }
            if((priv[k].set_args & IW_PRIV_SIZE_FIXED) &&
               (wrq.u.data.length != (priv[k].set_args & IW_PRIV_SIZE_MASK)))
            {
                 log("%s: the command <%s> needs exactly %d argument(s)\n",
                     __func__, cmd, priv[k].set_args & IW_PRIV_SIZE_MASK);
                 ret = -9;
                 break;
            }
        }
        else
        {
            wrq.u.data.length = 0;
        }

        strncpy(wrq.ifr_name, ifname, IFNAMSIZ);

        // These two tests determine how the driver will handle the data
        if((priv[k].set_args & IW_PRIV_SIZE_FIXED) &&
           ((iw_get_priv_size(priv[k].set_args) + offset) <= IFNAMSIZ))
        {
            // All the SET args fit within wrq
            if(offset) {
                wrq.u.mode = subcmd;
            }
            if(buf_size < IFNAMSIZ - offset) {
                 log("%s: data buffer too short, has to be at least %d bytes\n",
                     __func__, IFNAMSIZ - offset);
                 ret = -10;
                 break;
            }
            memcpy(wrq.u.name + offset, buf, IFNAMSIZ - offset);
        }
        else
        {
            if((priv[k].set_args == 0) &&
               (priv[k].get_args & IW_PRIV_SIZE_FIXED) &&
               (iw_get_priv_size(priv[k].get_args) <= IFNAMSIZ))
            {
                // No SET args, GET args fit within wrq
                if(offset) {
                    wrq.u.mode = subcmd;
                }
            }
            else
            {
                // args won't fit in wrq or variable number of args
                wrq.u.data.pointer = (void *)buf;
                wrq.u.data.flags = subcmd;
            }
        }

        // Perform the private ioctl
        if(ioctl(sock, priv[k].cmd, &wrq) < 0)
        {
            log("%s: failed ioctl <%s> (%X): %s\n",
                __func__, cmd, priv[k].cmd, strerror(errno));
            ret = -11;
            break;
        }

        // Figure the returned data len and location
        if((priv[k].get_args & IW_PRIV_SIZE_FIXED) &&
           (iw_get_priv_size(priv[k].get_args) <= IFNAMSIZ))
        {
            memcpy(buf, wrq.u.name, UTIL_MIN(IFNAMSIZ, buf_size));
            ret = iw_get_priv_size(priv[k].get_args);
        }
        else
        {
            ret = wrq.u.data.length;
        }

        // Add terminating 0 for char data
        if((priv[k].get_args & IW_PRIV_TYPE_MASK) == IW_PRIV_TYPE_CHAR &&
           ret > 0 && buf_size > 0)
        {
            if(ret < WT_WLEXT_MAX_PRIV_PAYLOAD) {
                ret++;
            }
            ((char *)buf)[UTIL_MIN(ret, buf_size) - 1] = 0;
        }

        break;
    }

    // Free the resources
    free(priv);
    if(close_sock) {
        iw_sockets_close(sock);
        sock = -1;
    }

    return ret;
}

// Perform pivate ioctl call (max data it passes or returns is 4096 bytes)
// ifname - interface to apply the call to
// cmd - double-0 terminated list of 0-separated command followed by arguments
// buf - pointer to the buffer for returned data (if any)
// buf_size - the buf size
// Returns: negative if error, number of bytes returned if success
//          if the # of bytes returned is greater than buf_size, then
//          only the buf_size portion of the data was returned
// Note: shares the temporary data buffer w/ wt_wlext_priv_direct()
int wt_wlext_priv(char *ifname, char *cmd, char *buf, int buf_size)
{
    if(skfd < 0 || buffer == NULL) {
        log("%s: wirelless telemetry subsystem is not initialized\n", __func__);
        return -1;
    }
    int ret = wlext_priv_cmd(skfd, ifname, cmd, buffer, WT_WLEXT_MAX_PRIV_PAYLOAD);
    if(ret >= 0 && buf && buf_size > 0) {
        int len = (ret < buf_size) ? ret : buf_size;
        memcpy(buf, buffer, len);
        buf[ret] = 0;
    }
    return ret;
}

// Get ESSID (up to IW_ESSID_MAX_SIZE == 32).
// essid - ptr to the buffer (32 bytes) where to save the ESSID (unless NULL)
// p_essid_len - ptr to int where to save the ESSID length (uness NULL)
// Note: essid can contain zeroes
// Returns: return 0 if successful, negative if error
int wt_wlext_get_essid(char *ifname, char *essid, int *p_essid_len)
{
    struct iwreq wrq;
    char buf[IW_ESSID_MAX_SIZE + 1];

    memset(buf, 0, sizeof(buf));
    memset(&wrq, 0, sizeof(wrq));
    wrq.u.essid.pointer = buf;
    wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
    if(iw_get_ext(skfd, ifname, SIOCGIWESSID, &wrq) < 0)
    {
        log_dbg("%s: SIOCGIWESSID failed for for %s, error: %s\n",
                __func__, ifname, strerror(errno));
        return -1;
    }
    buf[IW_ESSID_MAX_SIZE] = 0;
    if(essid) {
        memcpy(essid, buf, IW_ESSID_MAX_SIZE);
    }
    // The 'get' does not return length of SSID, so we will not know if
    // it was set up w/ 0s at the end or it ends w/ the last non-0 character.
    // Unlikely to happen unless an attempt is made to cause a crash/failure.
    if(p_essid_len) {
        int len;
        for(len = IW_ESSID_MAX_SIZE; len >= 0 && buf[len] == 0; --len);
        *p_essid_len = len + 1;
    }

    return 0;
}

// Get BSSID.
// bssid - ptr where to store 6 bytes of BSSID MAC (unless NULL)
// bssid_str - ptr where to store BSSID string xx:xx:xx:xx:xx:xx (unless NULL)
// Returns: return 0 if successful, negative if error
int wt_wlext_get_bssid(char *ifname, unsigned char *bssid, char *bssid_str)
{
    struct iwreq wrq;

    memset(&wrq, 0, sizeof(wrq));
    if(iw_get_ext(skfd, ifname, SIOCGIWAP, &wrq) < 0)
    {
        log_dbg("%s: SIOCGIWAP failed for for %s, error: %s\n",
                __func__, ifname, strerror(errno));
        return -1;
    }
    struct sockaddr *sa_ptr = (struct sockaddr*)&(wrq.u.ap_addr);
    if(bssid) {
        memcpy(bssid, sa_ptr->sa_data, 6);
    }
    if(bssid_str) {
        snprintf(bssid_str, MAC_ADDRSTRLEN, MAC_PRINTF_FMT_TPL,
                 MAC_PRINTF_ARG_TPL(sa_ptr->sa_data));
    }
    return 0;
}

// iw_enum_devices() callback for wt_wlext_enum_vaps
static int iw_enum_cb(int skfd, char *ifname, char *args[], int count)
{
    int radio_num = (int)(args[0]);
    int *vaps_count_p = (int *)(args[1]);
    char *ifname_buf = args[2] + (*vaps_count_p * IFNAMSIZ);

    // If the interface is a match
    if(wt_get_ifname_radio_num(ifname) == radio_num) {
        strncpy(ifname_buf, ifname, IFNAMSIZ);
        ifname_buf[IFNAMSIZ - 1] = 0;
        ++(*vaps_count_p);
    }
    return 0;
}

// Populate the VAPs array (ptr is passed in) w/ the
// VAP interface names for the `rinfo_num`
// Returns: the number of VAPs found
int wt_wlext_enum_vaps(int rinfo_num, char vaps[WIRELESS_MAX_VAPS][IFNAMSIZ])
{
    int vaps_count = 0;
    char *args[] = {(void*)rinfo_num, (void*)&vaps_count, (void*)&vaps[0][0]};
    iw_enum_devices(skfd, iw_enum_cb, args, 3);
    return vaps_count;
}

// Get the channel.
// Returns: channel if successful, negative error code if fails
int wt_wlext_get_channel(char *ifname)
{
    int chan = -1;
    struct iw_range    range;
    int have_range;
    struct iwreq wrq;

    // Get available frequency range (the libiw API will not convert
    // frequency to channel without it)
    memset(&wrq, 0, sizeof(wrq));
    memset(&range, 0, sizeof(range));
    have_range = (iw_get_range_info(skfd, ifname, &range) >= 0);

    /* Get frequency / channel */
    memset(&wrq, 0, sizeof(wrq));
    if(iw_get_ext(skfd, ifname, SIOCGIWFREQ, &wrq) >= 0)
    {
        double freq = iw_freq2float(&(wrq.u.freq));
        if(freq < 1000) {
            chan = (int)freq;
        } else if(have_range) {
            chan = iw_freq_to_channel(freq, &range);
        } else {
            // Let's try to map it without the dumb API
            int mghz = (int)((double)freq / (double)1000000);
            if(mghz >= 5170) {
                chan = (mghz - 5000) / 5;
            } else if(mghz >= 2484) {
                chan = 14;
            } else if(mghz >= 2412) {
                chan = (mghz - 2407) / 5;
            } else {
                log("%s: failed map %d MHz to channel for <%s>\n",
                    __func__, mghz, ifname);
            }
        }
    } else {
        log("%s: SIOCGIWFREQ has failed for <%s>, error: %s\n",
            __func__, ifname, strerror(errno));
    }

    return chan;
}

// Get max transmit power (in dBm if 'indbm' is TRUE)
// Returns: max Tx power in dBm if successful, negative if fails
int wt_wlext_get_max_txpwr(char *ifname, int indbm)
{
    struct iwreq wrq;

    memset(&wrq, 0, sizeof(wrq));
    if(iw_get_ext(skfd, ifname, SIOCGIWTXPOW, &wrq) < 0)
    {
        log_dbg("%s: SIOCGIWTXPOW failed for for %s\n",
                __func__, ifname);
        return -1;
    }

    struct iw_param *txpower = &wrq.u.txpower;
    int txpwr;

    if(txpower->disabled)
    {
        log_dbg("%s: txpower setting is disabled for %s\n",
                __func__, ifname);
        return -2;
    }

    if((txpower->flags & IW_TXPOW_RELATIVE) != 0)
    {
        if(!indbm) {
            txpwr = txpower->value;
            return txpwr;
        }
        log_dbg("%s: txpower is available in %% for %s\n",
                __func__, ifname);
        return -3;
    }

    if(!indbm) {
        log_dbg("%s: txpower is available in dBm for %s\n",
                __func__, ifname);
        return -4;
    }

    if((txpower->flags & IW_TXPOW_MWATT) != 0) {
        txpwr = iw_mwatt2dbm(txpower->value);
    } else {
        txpwr = txpower->value;
    }

    return txpwr;
}

// Get operation mode.
// Returns: static string w/ mode name or NULL
char *wt_wlext_get_mode(char *ifname)
{
    // use our own mode name array
    static char *names[] = { "auto",
                             "ad-hoc",
                             "managed",
                             "master",
                             "repeater",
                             "secondary",
                             "monitor" };
    struct iwreq wrq;

    if(iw_get_ext(skfd, ifname, SIOCGIWMODE, &wrq) >= 0)
    {
        int mode = wrq.u.mode;
        if((mode < 6) && (mode >= 0)) {
            return names[mode];
        }
        log("%s: unrecognized mode %d for %s\n", __func__, ifname);
        return NULL;
    }
    log("%s: SIOCGIWMODE has failed for %s, error: %s\n",
        __func__, ifname, strerror(errno));

    return NULL;
}

// Initializes wireless extensions
// Returns: 0 - success or negative int if error
int wt_wlext_init(void)
{
    if(buffer == NULL) {
        buffer = UTIL_MALLOC(WT_WLEXT_MAX_PRIV_PAYLOAD); // Never freed
    }
    if(buffer == NULL) {
        log("%s: failed to allocate memory for IOCTL buffer", __func__);
        return -1;
    }
    if(skfd >= 0) {
        log("%s: closing old wlext socket %d\n", __func__, skfd);
        iw_sockets_close(skfd);
        skfd = -1;
    }
    if((skfd = iw_sockets_open()) < 0)
    {
        log("%s: iw_sockets_open() error: %s", __func__, strerror(errno));
        return -1;
    }

    return 0;
}
