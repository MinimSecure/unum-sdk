// Copyright 2018 Minim Inc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// unum device configuration management code

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE

/* Drop all the debug log messages from this file until needed */
#undef LOG_DBG_DST
#define LOG_DBG_DST LOG_DST_DROP


// Get device config
// Returns: pointer to the 0-terminated config string or NULL if fails,
//          the returned pointer has to be released with the
//          platform_cfg_free() call ASAP, if successful the returned config
//          length is stored in p_len (uness p_len is NULL). The length
//          includes the terminating 0.
char *platform_cfg_get(int *p_len)
{
    return NULL;
}

// Calculate UID for the config passed in buf.
static int calc_cfg_uid(char *buf, CONFIG_UID_t *p_uid)
{
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

// Apply configuration from the memory buffer
// Parameters:
// - pointer to the config memory buffer
// - config length
// Returns: 0 - if ok or error
int platform_apply_cloud_cfg(char *cfg, int cfg_len)
{
    return 1;

#ifdef DEBUG
    // Do not reboot if running config test
    if(get_test_num() == U_TEST_FILE_TO_CFG) {
        printf("Reboot is required to apply the config.\n");
        return 0;
    }
#endif // DEBUG

    log("%s: new config saved, rebooting the device\n", __func__);
    util_reboot();
    log("%s: reboot request has failed\n", __func__);

    return -3;
}

// Platform specific init for the config subsystem
int platform_cfg_init(void)
{
    return 0;
}
