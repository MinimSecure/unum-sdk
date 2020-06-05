// (c) 2019 minim.co
// unum platform code for collecting association information of a station

#include "unum.h"

// Include declarations for platform driver specific code
// we share locally.
#include "wireless_platform.h"

int wt_tpl_fill_assoc_info(WT_JSON_TPL_RADIO_STATE_t *rinfo,
                           WT_JSON_TPL_VAP_STATE_t *vinfo,
                           WT_JSON_TPL_STA_STATE_t *sinfo,
                           int ii)
{
    return wt_tpl_fill_sta_info(rinfo,
                                  vinfo,
                                  sinfo, ii);
}
