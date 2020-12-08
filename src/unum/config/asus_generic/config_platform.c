// (c) 2019-2020 minim.co
// unum device configuration management code

#include "unum.h"

/* Drop all the debug log messages from this file until needed */
#undef LOG_DBG_DST
#define LOG_DBG_DST LOG_DST_DROP


// Skip temporary vars (add them all to the string w/ \0 around each var, extra
// \0 at the end), the vars are skipped when the config is read and also when
// applied
static char skip_vars[] = "wan0_expires\0login_timestamp\0client_info_tmp\0"
                          "wanduck_start_detect\0login_ip\0login_ip_str\0"
                          "networkmap_fullscan\0reload_svc_radio\0rc_service\0"
                          "link_internet\0wan0_auxstate_t\0rc_service_pid\0"
                          "wlready\0ddns_status\0"
                          "\0";

// Check if string matches any var in skip_vars[] array
// The var can be either 0-terminated or end with '='
static char *is_skip_var(char *var)
{
    char *skip_var = skip_vars;
    int skip_var_len = strlen(skip_var);
    for(; skip_var_len > 0; 
        skip_var += skip_var_len + 1,
        skip_var_len = strlen(skip_var))
    {
        if(strncmp(var, skip_var, skip_var_len) == 0 &&
           (var[skip_var_len] == '=' || var[skip_var_len] == 0))
        {
            break;
        }
    }
    return (skip_var_len > 0) ? skip_var : NULL;
}

// Check if var contains invalid chracters or of zero length
// The var can be either 0-terminated or end with '='
static int is_var_valid(char *var)
{
    char *tptr;

    for(tptr = var; *tptr != 0 && *tptr != '='; ++tptr) {
        if(iscntrl(*tptr) || isspace(*tptr) || *tptr == 0xff) {
            return FALSE;
        }
    }
    if(tptr == var) {
        return FALSE;
    }

    return TRUE;
}

// Get device config
// Returns: pointer to the 0-terminated config string or NULL if fails,
//          the returned pointer has to be released with the
//          platform_cfg_free() call ASAP, if successful the returned config
//          length is stored in p_len (uness p_len is NULL). The length
//          includes the terminating 0.
char *platform_cfg_get(int *p_len)
{
    int err = 0;
    char *cfg_ptr = NULL;
    size_t cfg_size = 0;

    // The config corruption detection (below) does not help when config
    // is completely wiped and reset. The boot code in the Asus repo is
    // always resetting restore_from_cloud when the value is missing.
    // The non-zero value makes us apply cloud config and fix potential
    // corruption.
    // While restore_from_cloud is not 0 we are not sending config to
    // the cloud waiting for restore to happen first.
    int r_c_val = nvram_get_int("restore_from_cloud");
    if(r_c_val > 0) {
        log("%s: restore_from_cloud set, pulling from cloud, retries %d\n",
            __func__, r_c_val);
        --r_c_val;
        if(r_c_val > 0) {
            nvram_set_int("restore_from_cloud", r_c_val);
        } else {
            // Clear up restore_from_cloud to try restore on the next boot
            nvram_unset("restore_from_cloud");
        }
        err = nvram_commit();
        if(err != 0)
        {
            log("%s: failed to save decremented restore_from_cloud, error %d\n",
                __func__, err);
        }
        config_download_req();
        return NULL;
    }

    cfg_ptr = UTIL_MALLOC(MAX_NVRAM_SPACE);
    if(cfg_ptr == NULL) {
        log("%s: failed to allocate %d bytes\n", __func__, MAX_NVRAM_SPACE);
        return NULL;
    }
    for(;;) {
        err = nvram_getall(cfg_ptr, MAX_NVRAM_SPACE);
        if(err != 0) {
            log("%s: nvram_getall() error %d\n", __func__, err);
            break;
        }
        if(*cfg_ptr == 0) {
            log("%s: error empty NVRAM buffer\n", __func__);
            err = -1;
            break;
        }
        char *cfg_end = memmem(cfg_ptr, MAX_NVRAM_SPACE, "\0\0", 2);
        if(cfg_end == NULL) {
            log("%s: error no double zero terminator\n", __func__);
            err = -2;
            break;
        }
        cfg_end += 2; // past the double zero terminator

        // Walk through the vars and make the buffer to be text-only (no zeros)
        // Assume that nvram values can contain '\n', assume the keys cannot and
        // that should allow us to parse it even if there are LFs (as long as LFs
        // are not mixed with '=' in the values).
        // Note, there are non-ASCII characters also.
        char *ptr;
        char *prev_ptr = cfg_ptr;
        for(ptr = cfg_ptr + strlen(cfg_ptr);
            ((ptr + 1) - cfg_ptr) < MAX_NVRAM_SPACE && *(ptr + 1) != 0;
            ptr += strlen(ptr))
        {
            // Check the var=val string length
            if(ptr - prev_ptr + 1 > NVRAM_MAX_STRINGSIZE) {
                log("%s: config corruption, key-value over %d bytes: %.24s...\n",
                    __func__, ptr - prev_ptr + 1, prev_ptr);
                err = -3;
                break;
            }

            // Check the variable name
            if(!is_var_valid(prev_ptr)) {
                log("%s: invalid var name: %.24s...\n", __func__, prev_ptr);
                err = -4;
                break;
            }

            // Check if we need to skip the var
            if(is_skip_var(prev_ptr) != NULL)
            {
                memmove(prev_ptr, ptr + 1, (cfg_end - (ptr + 1)));
                cfg_end -= (ptr + 1) - prev_ptr;
                ptr = prev_ptr;
                continue;
            }

            *ptr++ = '\n'; // replace 0-separator w/ '\n' and advance
            prev_ptr = ptr;
        }
        if(err != 0) {
            break;
        }
        cfg_size = (ptr - cfg_ptr) + 2;
        if(cfg_size > MAX_NVRAM_SPACE) {
            log("%s: config exceeded %d bytes\n", __func__, MAX_NVRAM_SPACE);
            err = -5;
            break;
        }
        *ptr++ = '\n';
        *ptr = 0;
        break;
    }
    if(err != 0) {
        if(cfg_ptr != NULL) {
            UTIL_FREE(cfg_ptr);
            cfg_ptr = NULL;
        }
        // Config corruption detected. We observed this during the firmware
        // upgrades on Asus, erasing it and rebooting (which should restore
        // the config from the cloud).
        log("%s: corrupt configuration, erase & reboot...\n", __func__);
        util_factory_reset();
        // no return expected...
    } else {
        log_dbg("%s: config len: %lu\n", __func__, cfg_size);
        log_dbg("%s: content:\n%.256s...\n", __func__, cfg_ptr);
    }

    if(err == 0 && p_len != NULL) {
        *p_len = cfg_size;
    }

    return cfg_ptr;
}

// Calculate UID for the config passed in buf.
static int calc_cfg_uid(char *buf, CONFIG_UID_t *p_uid)
{
    MD5_CTX md5;
    MD5_Init(&md5);

    MD5_Update(&md5, (unsigned char*)buf, strlen(buf));

    MD5_Final((unsigned char*)p_uid, &md5);
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
    log_dbg("%s: Freeing configuration buffer.\n", __func__);
    if(buf == NULL) {
        return;
    }
    UTIL_FREE(buf);
    return;
}

// Apply configuration from the memory buffer
// Parameters:
// - pointer to the config memory buffer
// - config length
// Returns: 0 - if ok or error
// Note: cfg must be 0-terminated (done automaticaly
//       if data come from the HTTP response structure)
// Note: For this function reboots the device if
//       successful (only returns if failed)
int platform_apply_cloud_cfg(char *cfg, int cfg_len)
{
    int err = 0;

#ifdef PRE_APPLY_CONFIG_CMD
#ifdef DEBUG
    // Do not mess with the commands if running config test
    if(get_test_num() == U_TEST_FILE_TO_CFG) {
        printf("Execute <%s> before applying the changes.\n",
               PRE_APPLY_CONFIG_CMD);
    }
    else
#endif // DEBUG
    {
        util_system(PRE_APPLY_CONFIG_CMD, APPLY_CONFIG_MAX_TIME, NULL);
        log("%s: preparing to apply new config by running <%s>\n",
            __func__, PRE_APPLY_CONFIG_CMD);
    }
#endif // APPLY_CONFIG_CMD

    // Note: the values might contain '\n', assuming the key names cannot,
    //       so we should be able to parse the lines like this:
    //       key1=val\nval\nval1\nkey2=val2...
    //       as long as they do not mix '\n' and '=' in the values.
    char *ptr = cfg;
    while(ptr && *ptr != 0)
    {
        // find where the value starts
        char *eq_ptr = strchr(ptr, '=');
        if(!eq_ptr) {
            log("%s: corrupt var at offset %d\n", __func__, ptr - cfg);
            err = -1;
            break;
        }

        // look for the last '\n' before next '=' or '\0'
        char *val_end = NULL;
        char *next_ptr = NULL;
        char *tptr;
        for(tptr = eq_ptr + 1; *tptr && (*tptr != '=' || !val_end); ++tptr) {
            if(*tptr == '\n') {
                val_end = tptr;
                next_ptr = tptr + 1;
            }
        }
        if(*tptr == 0) {
            val_end = tptr;
        }
        if(!val_end) { // this should never happen
            log("%s: corrupt var at offset %d, unterminated value\n",
                __func__, ptr - cfg);
            err = -2;
            break;
        }

        // Check if the variable is in the skip list
        if(is_skip_var(ptr) != NULL) {
            ptr = next_ptr;
            continue;
        }

        // Do a check for max key-value length
        if(val_end - ptr + 1 > NVRAM_MAX_STRINGSIZE) {
            log("%s: corrupt cloud config, key-value over %d bytes: %.24s...\n",
                __func__, val_end - ptr + 1, ptr);
            err = -3;
            break;
        }

        // Save the value,
        // they just set the vars over what's there then commit.
        char str[NVRAM_MAX_STRINGSIZE];
        memcpy(str, ptr, val_end - ptr);
        str[val_end - ptr] = 0;
        str[eq_ptr - ptr] = 0;
        char *var = str;
        char *val = str + (eq_ptr - ptr) + 1;

        // If there are multipe '\n' at the value end need to strip them all
        for(tptr = &str[val_end - ptr] - 1; tptr > str && *tptr == '\n'; --tptr)
        {
            *tptr = 0;
        }

        // Check the variable name
        if(!is_var_valid(var)) {
            log("%s: invalid var name <%s>\n", __func__, var);
            err = -4;
            break;
        }

        // Set the variable
        if(nvram_set(var, val) != 0) {
            log("%s: error setting var <%s>\n", __func__, var);
            err = -5;
            break;
        }

        ptr = next_ptr;
    }

    // If all OK, commit the changes
    if(err == 0)
    {
        // Zero restore_from_cloud (no need to pull from cloud on boot)
        nvram_set_int("restore_from_cloud", 0);
        err = nvram_commit();
    }

#ifdef DEBUG
    // Do not reboot if running config test
    if(get_test_num() == U_TEST_FILE_TO_CFG) {
#ifdef APPLY_CONFIG_CMD
        printf("Execute <%s> to apply the changes.\n", APPLY_CONFIG_CMD);
#else // APPLY_CONFIG_CMD
        printf("Reboot is required to apply the changes.\n");
#endif // APPLY_CONFIG_CMD
        return 0;
    }
#endif // DEBUG

    if(err == 0) {
        log("%s: new config saved\n", __func__);
    } else {
        log("%s: config failed, rebooting the device to revert\n", __func__);
    }
#ifdef APPLY_CONFIG_CMD
    if(err == 0) {
        log("%s: applying config using %s\n", __func__, APPLY_CONFIG_CMD);
        err = util_system(APPLY_CONFIG_CMD, APPLY_CONFIG_MAX_TIME, NULL);
        if(err == 0) {
            log("%s: successfully applied new config\n", __func__);
            return 0;
        }
        if(err == -1) {
            log("%s: apply config failed: %s\n", __func__, strerror(errno));
        } else if(WIFEXITED(err) && WEXITSTATUS(err) == 1) {
            log("%s: operation mode changed, restarting agent...\n", __func__);
            util_restart(UNUM_START_REASON_MODE_CHANGE);
            // never gets here
        } else if(WIFEXITED(err)) {
            log("%s: apply config command errurned error %d\n", __func__,
                WEXITSTATUS(err));
        } else if(WIFSIGNALED(err)) {
            log("%s: apply config command terminated by signal %d\n", __func__,
                WTERMSIG(err));
        } else {
            log("%s: apply config error, return value: %d\n", __func__, err);
        }
        return err;
    }
#endif // APPLY_CONFIG_CMD

    util_reboot();
    log("%s: reboot request has failed\n", __func__);
    err = -5;

    return err;
}

// Platform specific init for the config subsystem
int platform_cfg_init(void)
{
    return 0;
}
