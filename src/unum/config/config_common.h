// (c) 2017-2018 minim.co
// config subsystem common include file

#ifndef _CONFIG_COMMON_H
#define _CONFIG_COMMON_H


// Config update check period (in sec)
#define CONFIG_PERIOD 60


#ifdef CONFIG_DOWNLOAD_IN_AGENT
// Called by command processing job when pull_router_config command is received
void config_download_req(void);

// Processing function for loading and applying device configuration.
// The parameter is the command name, returns 0 if successful
int cmd_update_config(char *cmd, char *s_unused, int len_unused);
#endif // CONFIG_DOWNLOAD_IN_AGENT

// Get device config
// Returns: pointer to the mem containing device config or NULL if fails,
//          the returned pointer has to be released with the
//          platform_cfg_free() call ASAP, if successful the returned config
//          length is stored in p_len (uness p_len is NULL). The length
//          includes the terminating 0.
char *platform_cfg_get(int *p_len);

// Get config if it has changed since the last send
// (in)  last_sent_uid - UID of the last sent config
// (out) new_uid - ptr to UID of the new config
// (out) p_len - ptr where to store config length (or NULL)
// Returns: pointer to the mem containing device config
//          or NULL if unchanged, new_uid - is filled in
//          with the new UID if returned pointer is not NULL,
//          the returned pointer has to be released with the
//          platform_cfg_free() call ASAP. The length
//          includes the terminating 0.
char *platform_cfg_get_if_changed(CONFIG_UID_t *last_sent_uid,
                                  CONFIG_UID_t *new_uid,
                                  int *p_len);

// Release the config mem returned by platform_cfg_get*()
void platform_cfg_free(char *buf);

// Apply configuration from the memory buffer
// Parameters:
// - pointer to the config memory buffer
// - config length
// Returns: 0 - if ok or error
// Note: might reboot the device instead of returning
//       if it is necessary to apply the configuration
int platform_apply_cloud_cfg(char *cfg, int cfg_len);

// Platform specific init for the config subsystem
int platform_cfg_init(void);

// Subsystem init function
int config_init(int level);

#endif // _CONFIG_COMMON_H
