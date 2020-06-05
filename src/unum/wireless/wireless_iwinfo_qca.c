#include "unum.h"

int wt_platform_iwinfo_mk_if_list(
            char wt_interfaces[WT_MAX_IWINFO_VIFS][IFNAMSIZ],
            char wt_phys[WT_MAX_IWINFO_PHYS][IFNAMSIZ],
            int wt_if_to_phy[WT_MAX_IWINFO_VIFS],
            int *if_idx_next,
            int *phy_idx_next)
{
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
    *if_idx_next = *phy_idx_next = 0;
    while((de = readdir(dd)) != NULL)
    {
        // Skip anything that starts w/ .
        if(de->d_name[0] == '.') {
            continue;
        }
        char *ifname = de->d_name;
        if (strlen(ifname) > 9 || strncmp(ifname, "wifi", 4) == 0) {
            // Per QCA iwinfo, name length should always be less than 9
            // On DLink, there is one interface called, bonding_masters
            // iwinfo_backend iterates and checks through all the
            // iwinfo backends, with first being QCA iwinfo and followed-by
            // wext. Since bonding_masters does n't match with QCA interfaces,
            // it will be checked with wext backend. However with wext backend,
            // it seems to be crashing with Interface names of largers sizes

            // Also ignore wifiX interfaces. They are radio interfaces
            continue;
        }
        const struct iwinfo_ops *iw = iwinfo_backend(ifname);
        if(!iw) {
            continue;
        }
        if(*if_idx_next >= WT_MAX_IWINFO_VIFS) {
            log("%s: 1. too many (%d/%d) wireless interfaces\n",
                __func__, *if_idx_next, WT_MAX_IWINFO_VIFS);
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
        for(ii = 0; ii < *phy_idx_next; ++ii) {
            if(strcmp(phyname, wt_phys[ii]) == 0) {
                break;
            }
        }
        if(ii >= WT_MAX_IWINFO_PHYS) {
            log("%s: too many (%d) wireless radios/phys\n",
                __func__, ii);
            continue;
        }

        if(*phy_idx_next == ii) {
            ++(*phy_idx_next);
            strncpy(wt_phys[ii], phyname, IFNAMSIZ - 1);
        }

        wt_if_to_phy[*if_idx_next] = ii;
        strncpy(wt_interfaces[*if_idx_next], ifname, IFNAMSIZ - 1);
        ++(*if_idx_next);
    }
    closedir(dd);
    dd = NULL;

    return 0;
}

char* wt_iwinfo_get_country(char *ifname)
{
#ifdef WT_CC_FROM_CMD
    // Read Country code from a command
    static char cc[32];
    char *cc_ptr;
    FILE *fp = popen(COUNTRY_CODE_CMD, "r");
    if (fp == NULL) {
        return NULL;
    }
    fscanf(fp, "%s", cc);
    cc_ptr = cc;

    pclose(fp);
    return cc_ptr;
#else
    static char cc[32];
    char *cc_ptr;

    cc_ptr = (char *)nvram_get("wl_country_code");
    if (cc_ptr == NULL) {
        return NULL;
    }
    strncpy(cc, cc_ptr, 31);
    cc_ptr = cc;

    return cc_ptr;
#endif
}

// Get the channel
int wt_platform_iwinfo_get_channel(char *phyname)
{
    int ch;
    int phyidx = wt_iwinfo_get_phy_num(phyname);
    int idx = -1;
    char *ifname;

    if(phyidx < 0) {
        log("%s: invalid phy name <%s>\n", __func__, phyname);
        return -3;
    }
    if ((idx = wt_iwinfo_get_if_num_for_phy(phyidx, idx)) >= 0)
    {
        ifname = wt_iwinfo_get_ifname(idx);
    } else {
        log("%s: Non Interfaces <%s>\n", __func__, phyname);
        return -4;
    }
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

char*
wt_platform_iwinfo_get_vap(char *phyname)
{
    int phyidx = wt_iwinfo_get_phy_num(phyname);
    int idx = -1;
    char *ifname;

    if(phyidx < 0) {
        log("%s: invalid phy name <%s>\n", __func__, phyname);
        return NULL;
    }
    if ((idx = wt_iwinfo_get_if_num_for_phy(phyidx, idx)) >= 0)
    {
        ifname = wt_iwinfo_get_ifname(idx);
    } else {
        log("%s: No Interfaces <%s>\n", __func__, phyname);
        return NULL;
    }
    return ifname;
}
