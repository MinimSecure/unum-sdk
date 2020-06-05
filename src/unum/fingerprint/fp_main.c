// (c) 2017-2019 minim.co
// fingerprinting main information collector

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


#ifdef DEBUG
#define DPRINTF(args...) ((tpcap_test_param.int_val == TPCAP_TEST_FP_JSON) ? \
                          (printf(args)) : 0)
#else  // DEBUG
#define DPRINTF(args...) /* Nothing */
#endif // DEBUG


// Forward declarations
static JSON_VAL_TPL_t *tpl_fp_dhcp_array_f(char *key, int idx);
static JSON_VAL_TPL_t *tpl_fp_mdns_array_f(char *key, int idx);
static JSON_VAL_TPL_t *tpl_fp_ssdp_array_f(char *key, int idx);
static JSON_VAL_TPL_t *tpl_fp_useragent_array_f(char *key, int idx);
static JSON_VAL_TPL_t *tpl_fp_tbl_stats_array_f(char *key, int ii);


// Template for root fingerprinting JSON object
static JSON_OBJ_TPL_t tpl_fp_root = {
  { "ssdp",        {.type = JSON_VAL_FARRAY, {.fa = tpl_fp_ssdp_array_f}}},
  { "dhcp",        {.type = JSON_VAL_FARRAY, {.fa = tpl_fp_dhcp_array_f}}},
  { "mdns",        {.type = JSON_VAL_FARRAY, {.fa = tpl_fp_mdns_array_f}}},
  { "useragent",   {.type = JSON_VAL_FARRAY, {.fa = tpl_fp_useragent_array_f}}},
  { "table_stats", {.type = JSON_VAL_FARRAY, {.fa = tpl_fp_tbl_stats_array_f}}},
  { NULL }
};


// Dynamically builds JSON template for SSDP fingerprinting info array.
// Note: the generated jansson objects can refer to string
//       pointers that can change when the function is called for the next
//       element of the array.
static JSON_VAL_TPL_t *tpl_fp_ssdp_array_f(char *key, int idx)
{
    int ii;
    // Buffer for the device MAC address string
    static char mac[MAC_ADDRSTRLEN];
    // Int value for TRUE
    static int true_val = 1;
    // Template for generating SSDP fingerprinting JSON.
    static JSON_OBJ_TPL_t tpl_tbl_ssdp_obj = {
      { "info",  { .type = JSON_VAL_STR,  { .s = NULL }}}, // must be first
      { "short", { .type = JSON_VAL_PINT, { .pi = NULL }}}, // must be second
      { "mac",   { .type = JSON_VAL_STR,  { .s = mac }}},
      { NULL }
    };
    static JSON_VAL_TPL_t tpl_tbl_ssdp_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_ssdp_obj }
    };
    static int last_idx = 0; // track last query index
    static int last_ii = 0;  // array index we stopped at for last_idx

    // If starting over
    if(idx == 0) {
        last_idx = last_ii = 0;
    }
    // The next query index should be the last +1, otherwise error
    else if(idx != last_idx + 1) {
        return NULL;
    }
    last_idx = idx;

    // Get the ptr to the devices table
    FP_SSDP_t **pp_dev = fp_get_ssdp_tbl();

    // Continue searching from where we ended last +1
    for(ii = last_ii + 1; ii < FP_MAX_DEV; ii++) {
        FP_SSDP_t *dev = pp_dev[ii];
        if(!dev || dev->data[0] == 0) {
            continue;
        }
        snprintf(mac, sizeof(mac), MAC_PRINTF_FMT_TPL,
                 MAC_PRINTF_ARG_TPL(dev->mac));
        // Set ptr to data
        tpl_tbl_ssdp_obj[0].val.s = dev->data;
        // Set "short" flag if the content was truncated
        tpl_tbl_ssdp_obj[1].val.pi = NULL;
        if(dev->truncated) {
            tpl_tbl_ssdp_obj[1].val.pi = &true_val;
        }
        break;
    }
    last_ii = ii;

    if(ii >= FP_MAX_DEV) {
        return NULL;
    }

    return &tpl_tbl_ssdp_obj_val;
}

// Dynamically builds JSON template for DHCP fingerprinting info array.
// Note: the generated jansson objects can refer to string
//       pointers that can change when the function is called for the next
//       element of the array.
static JSON_VAL_TPL_t *tpl_fp_dhcp_array_f(char *key, int idx)
{
    int ii;
    // Buffer for the device MAC address string
    static char mac[MAC_ADDRSTRLEN];
    // Buffer for the device DHCP options blob
    static char blob[(FP_MAX_DHCP_OPTIONS << 1) + 1];
    // Template for generating DHCP fingerprinting JSON.
    static JSON_OBJ_TPL_t tpl_tbl_dhcp_obj = {
      { "mac",  { .type = JSON_VAL_STR, { .s = mac }}},
      { "blob", { .type = JSON_VAL_STR, { .s = blob }}},
      { NULL }
    };
    static JSON_VAL_TPL_t tpl_tbl_dhcp_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_dhcp_obj }
    };
    static int last_idx = 0; // track last query index
    static int last_ii = 0;  // array index we stopped at for last_idx

    // If starting over
    if(idx == 0) {
        last_idx = last_ii = 0;
    }
    // The next query index should be the last +1, otherwise error
    else if(idx != last_idx + 1) {
        return NULL;
    }
    last_idx = idx;

    // Get the ptr to the devices table
    FP_DHCP_t **pp_dev = fp_get_dhcp_tbl();

    // Continue searching from where we ended last +1
    for(ii = last_ii + 1; ii < FP_MAX_DEV; ii++) {
        FP_DHCP_t *dev = pp_dev[ii];
        if(!dev || dev->blob_len == 0) {
            continue;
        }
        snprintf(mac, sizeof(mac), MAC_PRINTF_FMT_TPL,
                 MAC_PRINTF_ARG_TPL(dev->mac));
        int jj;
        char *cptr = blob;
        for(jj = 0; jj < dev->blob_len && cptr < blob + (sizeof(blob)-1); jj++)
        {
            *cptr++ = UTIL_MAKE_HEX_DIGIT(dev->blob[jj] >> 4);
            *cptr++ = UTIL_MAKE_HEX_DIGIT(dev->blob[jj] & 0xf);
        }
        *cptr = 0;
        break;
    }
    last_ii = ii;

    if(ii >= FP_MAX_DEV) {
        return NULL;
    }

    return &tpl_tbl_dhcp_obj_val;
}

// Dynamically builds JSON template for mDNS fingerprinting info array.
// Note: the generated jansson objects can refer to string
//       pointers that can change when the function is called for the next
//       element of the array.
static JSON_VAL_TPL_t *tpl_fp_mdns_array_f(char *key, int idx)
{
    int ii;
    // Buffer for the device MAC address string
    static char mac[MAC_ADDRSTRLEN];
    // Int value for port
    static int port;
    // Buffer for the device mDNS TXT blob
    static char blob[(FP_MAX_MDNS_TXT << 1) + 1];
    // Template for generating DHCP fingerprinting JSON.
    static JSON_OBJ_TPL_t tpl_tbl_mdns_obj = {
      { "name", { .type = JSON_VAL_STR,  { .s = NULL }}}, // has to be first
      { "mac",  { .type = JSON_VAL_STR,  { .s = mac }}},
      { "blob", { .type = JSON_VAL_STR,  { .s = blob }}},
      { "port", { .type = JSON_VAL_PINT, { .pi = &port }}},
      { NULL }
    };
    static JSON_VAL_TPL_t tpl_tbl_mdns_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_mdns_obj }
    };
    static int last_idx = 0; // track last query index
    static int last_ii = 0;  // array index we stopped at for last_idx

    // If starting over
    if(idx == 0) {
        last_idx = last_ii = 0;
    }
    // The next query index should be the last +1, otherwise error
    else if(idx != last_idx + 1) {
        return NULL;
    }
    last_idx = idx;

    // Get the ptr to the devices table
    FP_MDNS_t **pp_dev = fp_get_mdns_tbl();

    // Continue searching from where we ended last +1
    for(ii = last_ii + 1; ii < FP_MAX_DEV; ii++) {
        FP_MDNS_t *dev = pp_dev[ii];
        if(!dev || dev->blob_len == 0) {
            continue;
        }
        snprintf(mac, sizeof(mac), MAC_PRINTF_FMT_TPL,
                 MAC_PRINTF_ARG_TPL(dev->mac));
        tpl_tbl_mdns_obj[0].val.s = (char *)dev->name;
        int jj;
        char *cptr = blob;
        for(jj = 0; jj < dev->blob_len && cptr < blob + (sizeof(blob)-1); jj++)
        {
            *cptr++ = UTIL_MAKE_HEX_DIGIT(dev->blob[jj] >> 4);
            *cptr++ = UTIL_MAKE_HEX_DIGIT(dev->blob[jj] & 0xf);
        }
        *cptr = 0;
        port = dev->port;
        break;
    }
    last_ii = ii;

    if(ii >= FP_MAX_DEV) {
        return NULL;
    }

    return &tpl_tbl_mdns_obj_val;
}

// Dynamically builds JSON template for HTTP UserAgent fingerprinting info array.
// Note: the generated jansson objects can refer to string
//       pointers that can change when the function is called for the next
//       element of the array.
static JSON_VAL_TPL_t *tpl_fp_useragent_array_f(char *key, int idx)
{
    int ii, jj;

    // Buffer for the device MAC address string
    static char mac[MAC_ADDRSTRLEN];

    // Template array for UserAgent Strings
    static JSON_VAL_TPL_t arr[FP_MAX_USERAGENT_COUNT + 1] = {
        [0 ... (FP_MAX_USERAGENT_COUNT - 1)] = { .type = JSON_VAL_STR },
        [ FP_MAX_USERAGENT_COUNT ] = { .type = JSON_VAL_END }
    };

    // Template for generating UserAgent fingerprinting JSON.
    static JSON_OBJ_TPL_t tpl_tbl_useragent_obj = {
        { "mac",  { .type = JSON_VAL_STR, { .s = mac }}},
        { "data", { .type = JSON_VAL_ARRAY, { .a = arr }}},
        { NULL }
     };
    static JSON_VAL_TPL_t tpl_tbl_useragent_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_useragent_obj }
    };

    static int last_idx = 0; // track last query index
    static int last_ii = 0;  // array index we stopped at for last_idx

    // If starting over
    if(idx == 0) {
        last_idx = last_ii = 0;
    }
    // The next query index should be the last +1, otherwise error
    else if(idx != last_idx + 1) {
        return NULL;
    }
    last_idx = idx;

    // Get the ptr to the table
    FP_USERAGENT_t *p_dev = fp_get_useragent_tbl();

    // Continue searching from where we ended last +1
    for(ii = last_ii + 1; ii < FP_MAX_DEV; ii++)
    {
        FP_USERAGENT_t *dev = &p_dev[ii];

        if(!dev || !dev->busy) {
           continue;
        }

        snprintf(mac, sizeof(mac), MAC_PRINTF_FMT_TPL,
                 MAC_PRINTF_ARG_TPL(dev->mac));

        for(jj = 0; jj < dev->index && jj < FP_MAX_USERAGENT_COUNT; jj++)
        {
            arr[jj].type = JSON_VAL_STR;
            arr[jj].s = dev->ua[jj]->data;
        }

        if(jj < FP_MAX_USERAGENT_COUNT) {
           arr[jj].type = JSON_VAL_END;
        }

        break;
    }
    last_ii = ii;

    if(ii >= FP_MAX_DEV) {

        return NULL;
    }

    return &tpl_tbl_useragent_obj_val;
}

// Dynamically builds JSON template for fingerprinting
// table stats array.
static JSON_VAL_TPL_t *tpl_fp_tbl_stats_array_f(char *key, int ii)
{
    // Static buffer tpl_tbl_stats_obj refers to. The array builder just
    // has to memcpy() data here for the template to pick it up.
    static FP_TABLE_STATS_t tbl_stats;
    // Template for generating table stats JSON.
    static JSON_OBJ_TPL_t tpl_tbl_stats_obj = {
      { "name", { .type = JSON_VAL_STR, {.s = NULL}}}, // should be first
      { "add_all", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.add_all}}},
      { "add_limit", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.add_limit}}},
      { "add_busy", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.add_busy}}},
      { "add_10", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.add_10}}},
      { "add_found", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.add_found}}},
      { "add_new", { .type = JSON_VAL_PUL, {.pul = &tbl_stats.add_new}}},
      { NULL }
    };
    static JSON_VAL_TPL_t tpl_tbl_stats_obj_val = {
        .type = JSON_VAL_OBJ, { .o = tpl_tbl_stats_obj }
    };
    static char *name[] = {
        "dhcp",
        "ssdp",
        "mdns",
        "useragent"
    };
    static FP_TABLE_STATS_t *(*func_ptr[])(int) = {
        fp_dhcp_tbl_stats,
        fp_ssdp_tbl_stats,
        fp_mdns_tbl_stats,
        fp_useragent_tbl_stats
    };

    if(ii >= UTIL_ARRAY_SIZE(func_ptr)) {
        return NULL;
    }

    FP_TABLE_STATS_t *stats = (func_ptr[ii])(FALSE);
    if(!stats) {
        memset(&tbl_stats, 0, sizeof(tbl_stats));
    } else {
        memcpy(&tbl_stats, stats, sizeof(tbl_stats));
    }
    tpl_tbl_stats_obj[0].val.s = name[ii];

    return &tpl_tbl_stats_obj_val;
}

// Functions returning template for building fingerprinting
// JSON, called by devices telemetry when it prepares its own JSON.
JSON_KEYVAL_TPL_t *fp_mk_json_tpl_f(char *key)
{
    FP_TABLE_STATS_t *dhcp_stats = fp_dhcp_tbl_stats(FALSE);
    FP_TABLE_STATS_t *mdns_stats = fp_mdns_tbl_stats(FALSE);
    FP_TABLE_STATS_t *ssdp_stats = fp_ssdp_tbl_stats(FALSE);
    FP_TABLE_STATS_t *useragent_stats = fp_useragent_tbl_stats(FALSE);

    // If nothing to report return NULL (note add_all does not
    // guarantee entries in the tables, but it means there might
    // have been attempts to add data and we should report stats)
    if(dhcp_stats->add_all == 0 &&
       mdns_stats->add_all == 0 &&
       ssdp_stats->add_all == 0 &&
       useragent_stats->add_all == 0)
    {
        return NULL;
    }

    // Return the template
    return &(tpl_fp_root[0]);
}

// Reset all fingerprinting tables (including the table usage stats)
// Has to work (not crash) even if the subsystem is not initialized
// (otherwise it will break devices telemetry tests)
void fp_reset_tables(void)
{
    fp_reset_dhcp_tables();
    fp_reset_mdns_tables();
    fp_reset_ssdp_tables();
    fp_reset_useragent_tables();
}

// Subsystem init fuction. The subsystem runs individual fingerprinting
// modules. The data collected by the modules is added to the devices
// telemetry JSON.
int fp_init(int level)
{
    if(level == INIT_LEVEL_FINGERPRINT) {
        // Initialize the DHCP fingerprinting
        if(fp_dhcp_init() != 0) {
            return -1;
        }

        // Initialize the mDNS fingerprinting
        if(fp_mdns_init() != 0) {
            return -2;
        }

        // Initialize the mDNS fingerprinting
        if(fp_ssdp_init() != 0) {
            return -3;
        }

        // Initialize the UserAgent fingerprinting
        if(fp_useragent_init() != 0) {
            return -4;
        }
    }
    return 0;
}
