#include "unum.h"
// All functions dependent on LEDE specific platform
// Given phy name, get the first interface (VAP) name
// Since the phy to interface mapping is maintained in wireless_iwinfo,
// we use iwinfo APIs
// Note: nl80211 would eventually be decoupled from iwinfo
char* wt_platform_nl80211_get_vap(char *phyname)
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
