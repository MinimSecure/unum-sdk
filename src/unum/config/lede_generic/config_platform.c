// (c) 2017-2019 minim.co
// unum device configuration management code

#include "unum.h"

// From uci_internal.h - only include public attributes
struct uci_parse_context {
  const char *reason;
  int line;
  int byte;
};

/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE

/* Drop all the debug log messages from this file until needed */
#undef LOG_DBG_DST
#define LOG_DBG_DST LOG_DST_DROP

// Script applying UCI config changes for LEDE platform
#define UNUM_RESTART_CONFIG_SCRIPT "/usr/bin/restart_config.sh"


// Get device config
// Returns: pointer to the 0-terminated config string or NULL if fails,
//          the returned pointer has to be released with the
//          platform_cfg_free() call ASAP, if successful the returned config
//          length is stored in p_len (uness p_len is NULL). The length
//          includes the terminating 0.
char *platform_cfg_get(int *p_len)
{
    int ret = UCI_ERR_UNKNOWN;
    struct uci_ptr ptr;
    struct uci_context *ctx = NULL;
    char *cfg_ptr = NULL;
    size_t cfg_size = 0;

    FILE *out = open_memstream(&cfg_ptr, &cfg_size);
    if(out == NULL) {
        log("%s: open_memstream() error: %s\n", __func__, strerror(errno));
        return NULL;
    }

    for(;;)
    {
        ctx = uci_alloc_context();
        if(!ctx) {
            log("%s: uci_alloc_context() has faileed\n", __func__);
            break;
        }

        char **configs = NULL;
        char **p;
        ret = uci_list_configs(ctx, &configs);
        if((ret != UCI_OK) || !configs) {
            log("%s: uci_list_configs() returned %d\n", __func__, ret);
            break;
        }
        for(p = configs; *p; p++) {
            ret = uci_lookup_ptr(ctx, &ptr, *p, TRUE);
            if(ret != UCI_OK) {
                log("%s: uci_lookup_ptr() returned %d for <%s>\n",
                    __func__, ret, *p);
                break;
            }
            ret = uci_export(ctx, out, ptr.p, TRUE);
            if(ret != UCI_OK) {
                log("%s: uci_export() returned %d for <%s>\n",
                    __func__, ret, *p);
                break;
            }
        }
        free(configs);

        break;
    }

    if(out != NULL) {
        fclose(out);
        out = NULL;
    }

    if(ctx != NULL) {
        uci_free_context(ctx);
        ctx = NULL;
    }

    if(ret != UCI_OK && cfg_ptr != NULL) {
        free(cfg_ptr);
        cfg_ptr = NULL;
        return cfg_ptr;
    }

    log_dbg("%s: config len: %lu\n", __func__, cfg_size);
    log_dbg("%s: content:\n%.256s...\n", __func__, cfg_ptr);

    if(p_len != NULL) {
        // The data returned by open_memstream() includes the terminating 0,
        // but the data length has to be incremented to account for it.
        *p_len = cfg_size + 1;
    }

    return cfg_ptr;
}

// Calculate UID for the config passed in buf.
static int calc_cfg_uid(char *buf, CONFIG_UID_t *p_uid)
{
    mbedtls_md5_context md5;

    mbedtls_md5_init(&md5);
    mbedtls_md5_starts(&md5);
    mbedtls_md5_update(&md5, (unsigned char*)buf, strlen(buf));
    mbedtls_md5_finish(&md5, (unsigned char*)p_uid);
    mbedtls_md5_free(&md5);

    return 0;
}

// Get config if it has changed since the last send
// (in)  last_sent_uid - UID of the last sent config
// (out) new_uid - ptr to UID of the new config
// (out) p_len - ptr where to store config length (or NULL)
// Returns: pointer to the 0-terminated config string
//          or NULL if unchanged, new_uid - is filled in
//          with the new UID if returned pointer is not NULL,
//          the returned pointer has to be released with the
//          platform_cfg_free() call ASAP. The length
//          includes the terminating 0.
char *platform_cfg_get_if_changed(CONFIG_UID_t *p_last_sent_uid,
                                  CONFIG_UID_t *p_new_uid, int *p_len)
{
    char *buf;

    buf = platform_cfg_get(p_len);
    if(!buf) {
        return buf;
    }
    if(calc_cfg_uid(buf, p_new_uid) != 0) {
        log("%s: failed to calculate UID for config in %p\n", __func__, buf);
        platform_cfg_free(buf);
        return NULL;
    }
    if(memcmp(p_last_sent_uid, p_new_uid, sizeof(CONFIG_UID_t)) == 0) {
        log_dbg("%s: UID unchanged for config in %p\n", __func__, buf);
        platform_cfg_free(buf);
        return NULL;
    }

    return buf;
}

// Release the config mem returned by platform_cfg_get*()
void platform_cfg_free(char *buf)
{
    // The buffer is allocated by the system standard malloc()
    // in open_memstream()
    free(buf);
    return;
}


#ifdef APPLY_CONFIG_CMD
// Apply new configuration or return error if reboot is required to apply
static int platform_apply_config(void)
{
    int err;
    struct stat st;
    char *apply_cmd = APPLY_CONFIG_CMD;
    int ret;

    err = stat(apply_cmd, &st);
    if(err == 0) {
        log("%s: applying config using %s\n", __func__, apply_cmd);
        ret = util_system(apply_cmd, APPLY_CONFIG_MAX_TIME, NULL);
        if(ret == 0) {
            log("%s: successfully applied new config\n", __func__);
            return 0;
        }
        if(ret == -1) {
            log("%s: apply config failed: %s\n", __func__, strerror(errno));
        } else if(WIFEXITED(ret) && WEXITSTATUS(ret) == 1) {
            log("%s: operation mode changed, restarting agent...\n", __func__);
            util_restart(UNUM_START_REASON_MODE_CHANGE);
            // never gets here
        } else if(WIFEXITED(ret)) {
            log("%s: apply config command returned error %d\n", __func__,
                WEXITSTATUS(ret));
        } else if(WIFSIGNALED(ret)) {
            log("%s: apply config command terminated by signal %d\n", __func__,
                WTERMSIG(ret));
        } else {
            log("%s: apply config error, return value: %d\n", __func__, ret);
        }
        return ret;
    }

    return -1;
}
#endif // APPLY_CONFIG_CMD

// Apply configuration from the memory buffer
// Parameters:
// - pointer to the config memory buffer
// - config length
// Returns: 0 - if ok or error
// Note: cfg must be 0-terminated (done automaticaly
//       if data come from the HTTP response structure)
// Note: This function reboots the device if
//       successful (only returns if failed)
int platform_apply_cloud_cfg(char *cfg, int cfg_len)
{
    struct uci_package *package = NULL;
    struct uci_context *ctx = NULL;
    int ret = UCI_ERR_UNKNOWN;

    FILE *input = fmemopen(cfg, cfg_len, "r");
    if(input == NULL) {
        log("%s: fmemopen() error: %s\n", __func__, strerror(errno));
        return -1;
    }

    for(;;)
    {
        struct uci_element *e;

        ctx = uci_alloc_context();
        if(!ctx) {
            log("%s: uci_alloc_context() has faileed\n", __func__);
            break;
        }
        ret = uci_import(ctx, input, NULL, &package, FALSE);
        switch(ret) {
            case UCI_OK: 
                // loop through all config sections and overwrite existing data
                uci_foreach_element(&ctx->root, e) {
                    struct uci_package *p = uci_to_package(e);
                    ret = uci_commit(ctx, &p, true);
                }
                break;
            case UCI_ERR_PARSE:
                if(ctx->pctx) {
                    log("%s: parse error importing configuration (%s) at line %d, byte %d\n",
                        __func__, (ctx->pctx->reason ? ctx->pctx->reason : "unknown"),
                        ctx->pctx->line, ctx->pctx->byte);
                } else {
                    log("%s: parse error importing configuration\n",
                        __func__);
                }
                break;
            default:
                log("%s: error %d importing configuration\n",
                    __func__, ret);
        }

        break;
    }

    if(ctx != NULL) {
        uci_free_context(ctx);
        ctx = NULL;
    }

    if(input != NULL) {
        fclose(input);
        input = NULL;
    }

    if(ret != UCI_OK) {
        return -2;
    }

#ifdef DEBUG
    // Do not reboot or apply automatically if running config test
    if(get_test_num() == U_TEST_FILE_TO_CFG) {
#ifdef APPLY_CONFIG_CMD
        printf("Execute <%s> to apply the changes.\n", APPLY_CONFIG_CMD);
#else // APPLY_CONFIG_CMD
        printf("Reboot is required to apply the changes.\n");
#endif // APPLY_CONFIG_CMD
        return 0;
    }
#endif // DEBUG

#ifdef APPLY_CONFIG_CMD
    log("%s: new config saved, applying...\n", __func__);
    if(platform_apply_config() == 0) {
        log("%s: applied the updated configuration\n", __func__);
        return 0;
    }
#endif // APPLY_CONFIG_CMD

    // Reboot if the config cannot be applied without it
    util_reboot();
    log("%s: reboot request has failed\n", __func__);

    return -3;
}

// Platform specific init for the config subsystem
int platform_cfg_init(void)
{
    // Nothing for LEDE here.
    // Note: Maybe we can use pre-allocated context, but it is not
    //       clear if there are going to be side effects due to
    //       that as we try to reuse it (plus the MT safety...)
    return 0;
}
