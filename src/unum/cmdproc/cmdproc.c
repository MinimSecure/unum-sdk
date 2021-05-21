// (c) 2017-2019 minim.co
// unum command processor common code

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Command is available for the processing event
static UTIL_EVENT_t cmd_ready = UTIL_EVENT_INITIALIZER;

// Command queue access mutex
static UTIL_MUTEX_t cmd_q_m = UTIL_MUTEX_INITIALIZER;

// Command queue array
char *cmd_q_buf;
char *cmd_q[CMD_Q_MAX_SIZE];

// Command queue start and length
static unsigned int cmd_q_start = 0;
static unsigned int cmd_q_len = 0;

// Function for a debug command testing crashing the agent
#ifdef DEBUG
static void trigger_crash(void) {
    int *ptr = NULL;
    *ptr = 0;
    return;
}
#endif // DEBUG

// Generic command set. It contains all command rules that are the same
// for all the platforms, the platform specific rules can be added
// to the cmd_platform_rules[] array in the platform subfolder.
static CMD_RULE_t cmd_gen_rules[] = {
    { "reboot",             // reboot the device
      CMD_RULE_M_FULL | CMD_RULE_F_VOID,
      { .act_void = util_reboot }},
    { "restart_agent",      // restart the agent
      CMD_RULE_M_FULL | CMD_RULE_F_VOID,
      { .act_void = util_restart_from_server }},
    { "factory_reset",      // reset to factory defaults
      CMD_RULE_M_FULL | CMD_RULE_F_VOID,
      { .act_void = util_factory_reset }},
    { "wireless_scan",      // force wireless scan
      CMD_RULE_M_FULL | CMD_RULE_F_VOID,
      { .act_void = cmd_wireless_initiate_scan }},
    { "pull_router_config", // load and apply device config file
      CMD_RULE_M_FULL | CMD_RULE_F_RETRY,
      { .act_data = cmd_update_config }},
    { "speedtest", // runs a speedtest
      CMD_RULE_M_FULL | CMD_RULE_F_CMD,
      { .act_cmd = cmd_speedtest }},
    { "do_ssdp_discovery", // schedule SSDP discovery
      CMD_RULE_M_FULL | CMD_RULE_F_VOID,
      { .act_void = cmd_ssdp_discovery }},
    { "do_mdns_discovery", // schedule mDNS discovery
      CMD_RULE_M_FULL | CMD_RULE_F_DATA,
      { .act_data = cmd_mdns_discovery }},
    { "port_scan", // port scan devices
      CMD_RULE_M_FULL | CMD_RULE_F_DATA,
      { .act_data = cmd_port_scan }},
    { "fetch_urls", // fetch URLs requested by the server
      CMD_RULE_M_FULL | CMD_RULE_F_DATA,
      { .act_data = cmd_fetch_urls }},
#ifdef FW_UPDATER_RUN_MODE
    { "force_fw_update", // Force FW update (even for development versions)
      CMD_RULE_M_FULL | CMD_RULE_F_VOID,
      { .act_void = cmd_force_fw_update }},
#endif // FW_UPDATER_RUN_MODE
#ifdef DEBUG
    { "crash_me",        // Trigger a crash for debugging
      CMD_RULE_M_FULL | CMD_RULE_F_VOID,
      { .act_void = trigger_crash }},
#endif // DEBUG
#ifdef SUPPORT_RUN_MODE
    { "support_portal_connect",  // Create support check file
      CMD_RULE_M_FULL | CMD_RULE_F_VOID,
      { .act_void = create_support_flag_file }},
#endif // SUPPORT_RUN_MODE
    { "caf", // run a caf test
      CMD_RULE_M_SUBSTR | CMD_RULE_F_CMD,
      { .act_cmd = cmd_speedtest }},
    // CMD_RULE_M_ANY must be the last one
    { "shell_cmd", // generic commands, pass to shell
      CMD_RULE_M_ANY | CMD_RULE_F_DATA,
      { .act_data = cmd_to_shell }},
    { NULL, CMD_RULE_END }  // End command rules
};


// Generic processing function for commands running in shell.
// The parameters: command name, shell command script content, content len
// The content is 0-terminated, but 0 might be beyond len.
// Returns 0 if successful (exit code from the util_system() call)
int cmd_to_shell(char *cmd, char *s, int s_len)
{
    json_t *rsp_root = NULL;
    json_error_t jerr;
    char *cmd_str = NULL;
    int err = -1;

    for(;;)
    {
        const char *tmp_str;
        int len;

        if(!s || s_len <= 0) {
            log("%s: error, %s%s, len: %d\n",
                __func__, (s ? "data: " : "no data"), (s ? s : ""), s_len);
            break;
        }

        // Process the incoming JSON (etag and command)
        rsp_root = json_loads(s, JSON_REJECT_DUPLICATES, &jerr);
        if(!rsp_root) {
            log("%s: error at l:%d c:%d parsing command, msg: '%s'\n",
                __func__, jerr.line, jerr.column, jerr.text);
            break;
        }
        json_t *command = json_object_get(rsp_root, "command");
        if(!command || (tmp_str = json_string_value(command)) == NULL) {
            log("%s: error, invalid command response\n", __func__);
            break;
        }
        len = strlen(tmp_str);
        cmd_str = UTIL_MALLOC(len + 1);
        if(!cmd_str) {
            log("%s: error allocating %d bytes for command\n", __func__, len);
            break;
        }
        strcpy(cmd_str, tmp_str);
        // No longer need the json object
        json_decref(rsp_root);
        rsp_root = NULL;

        util_fix_crlf(cmd_str);
        len = strlen(cmd_str);

        err = util_buf_to_file(CMD_SHELL_PNAME, cmd_str, len, 00755);
        if(err != 0) {
            log("%s: error creating cmd file %s\n", __func__, CMD_SHELL_PNAME);
            break;
        }
        // No longer need the string
        UTIL_FREE(cmd_str);
        cmd_str = NULL;

        err = util_system(CMD_SHELL_PNAME, CMD_MAX_EXE_TIME, NULL);
#ifndef DEBUG
        unlink(CMD_SHELL_PNAME);
#endif // !DEBUG
        break;
    }

    if(rsp_root) {
        json_decref(rsp_root);
        rsp_root = NULL;
    }

    if(cmd_str != NULL) {
        UTIL_FREE(cmd_str);
        cmd_str = NULL;
    }

    return err;
}

// Add command to the commands queue, retunrs 0 if successful
int cmdproc_add_cmd(const char *cmd)
{
    unsigned int ii;
    int ret = 0;

    if(!cmd) {
        log("%s: error, invalid command pointer\n", __func__);
        return -1;
    }
    UTIL_MUTEX_TAKE(&cmd_q_m);
    for(;;) {
        // Only allow (max - 1) commands to be added as we might be
        // processing a command that was at the top of the queue
        // and do not want it to be overridden (the ptr to its
        // name is used outside of the mutex protecteed code).
        if(cmd_q_len >= CMD_Q_MAX_SIZE - 1) {
            log("%s: command queue overflow\n", __func__);
            ret = -1;
            break;
        }
        ii = (cmd_q_start + cmd_q_len) % CMD_Q_MAX_SIZE;
        ++cmd_q_len;
        strncpy(cmd_q[ii], cmd, CMD_STR_MAX_LEN - 1);
        cmd_q[ii][CMD_STR_MAX_LEN - 1] = 0;
        // Notify the command processor that a command is ready
        log("%s: set command received event for <%s>\n", __func__, cmd);
        UTIL_EVENT_SET(&cmd_ready);
        break;
    }
    UTIL_MUTEX_GIVE(&cmd_q_m);
    return ret;
}

// Find the rule matching the command "cmd" in the rule set "rule_set"
// Returns: pointer to the rule or NULL if not found
CMD_RULE_t *find_rule_by_cmd(CMD_RULE_t *rule_set, char *cmd)
{
    CMD_RULE_t *rule = NULL;

    // Find the rule matching the command
    for(rule = rule_set;
        (rule->flags & CMD_RULE_END) == 0;
        rule++)
    {
        if((rule->flags & CMD_RULE_M_FULL) != 0 &&
           strcmp(rule->cmd, cmd) == 0)
        {
            break;
        }
        if((rule->flags & CMD_RULE_M_SUBSTR) != 0 &&
           strncmp(rule->cmd, cmd, strlen(rule->cmd)) == 0)
        {
            break;
        }
        if((rule->flags & CMD_RULE_M_ANY) != 0)
        {
            break;
        }
    }
    if((rule->flags & CMD_RULE_END) != 0) {
        rule = NULL;
    }

    return rule;
}

// URL to request command data, parameters:
// - the URL prefix
// - MAC addr (in xx:xx:xx:xx:xx:xx format)
// - the command name
#define CMD_PATH "/v3/unums/%s/commands/%s"

// Command processor loop
static void cmdproc(THRD_PARAM_t *p)
{
    char *my_mac = util_device_mac();
    log("%s: started\n", __func__);

    // Check that we have MAC address
    if(!my_mac) {
        log("%s: cannot get device MAC\n", __func__);
        // We can't do much without MAC
        return;
    }

    log("%s: waiting for provision to complete\n", __func__);
    wait_for_provision();
    log("%s: done waiting for provision\n", __func__);

    util_wd_set_timeout(HTTP_REQ_MAX_TIME + CMD_RETRY_TIMEOUT +
                        CMD_MAX_EXE_TIME);

    for(;;) {
        int rc, err, len;
        char *cmd = NULL;
        char *data = NULL;
        http_rsp *rsp = NULL;
        CMD_RULE_t *rule = NULL;
        char url[256];

        util_wd_poll();

        rc = UTIL_EVENT_TIMEDWAIT(&cmd_ready, CMD_RETRY_TIMEOUT * 1000);
        if(rc == 0) {
            log("%s: command received event\n", __func__);
        } else if(rc < 0) {
            log("%s: internal error\n", __func__);
            sleep(CMD_RETRY_TIMEOUT);
        }

        UTIL_MUTEX_TAKE(&cmd_q_m);
        UTIL_EVENT_RESET(&cmd_ready);
        for(;;) {
            // If the command queue length is 0, nothing to do here
            if(cmd_q_len <= 0) {
                break;
            }

            // Grab the command pointer and advance the queue start
            cmd = cmd_q[cmd_q_start];
            cmd_q_start = (cmd_q_start + 1) % CMD_Q_MAX_SIZE;
            --cmd_q_len;

            // First try to see if any platform specific rule matches
            rule = find_rule_by_cmd(cmd_platform_rules, cmd);
            // If no platform rule matched try generic ones 
            rule = rule ? rule : find_rule_by_cmd(cmd_gen_rules, cmd);
            if(!rule) {
                log("%s: unknown command <%s>\n", __func__, cmd);
                break;
            }

            log("%s: using cmd rule <%s>, 0x%08x\n",
                __func__, (rule->cmd == NULL ? "No Name" : rule->cmd), rule->flags);
            break;
        }
        UTIL_MUTEX_GIVE(&cmd_q_m);

        // If no rule found, then nothing to do for the command
        if(!rule) {
            continue;
        }

        // Process the command according to the rule
        err = 0;

        // Downlaoding payload from the server for commands w/ data
        rsp = NULL;
        data = NULL;
        len = 0;
        for(;(rule->flags & CMD_RULE_F_DATA) != 0;)
        {
            // Prepare the URL string
            util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API, url,
                           sizeof(url), CMD_PATH, my_mac, cmd);

            // Get the command data
            rsp = http_get(url, "Accept: application/json\x00");

            if(rsp == NULL || (rsp->code / 100) != 2) {
                log("%s: command data request error, code %d%s\n",
                    __func__, rsp ? rsp->code : 0, rsp ? "" : "(none)");
                err = -1;
                break;
            }

            data = (char *)rsp->data;
            len = rsp->len;
            break;
        }

        // Call the command
        if(err == 0 && (rule->flags & CMD_RULE_F_VOID) != 0) {
            if(rule->act_void != NULL) {
                rule->act_void();
            } else {
                log("%s: cmd <%s> is not supported\n",
                    __func__, (rule->cmd == NULL ? "No Name" : rule->cmd));
            }
            err = 0;
        } else if ((rule->flags & CMD_RULE_F_CMD) != 0) {
            if(rule->act_cmd != NULL) {
                rule->act_cmd(cmd);
            } else {
                log("%s: cmd <%s> is not supported\n",
                    __func__, (rule->cmd == NULL ? "No Name" : rule->cmd));
            }
        } else {
            if(rule->act_data != NULL) {
                err = rule->act_data(cmd, data, len);
            } else {
                log("%s: cmd <%s> is not supported\n",
                    __func__, (rule->cmd == NULL ? "No Name" : rule->cmd));
            }
        }

        // No longer need the response data
        if(rsp) {
            free_rsp(rsp);
            rsp = NULL;
        }

        // If the command completed with error and requires retry
        // re-add it to the queue.
        if(err && (rule->flags & CMD_RULE_F_RETRY) != 0) {
            log("%s: command <%s> has failed, re-adding\n",
                __func__, cmd);
            cmdproc_add_cmd(cmd);
        }
    }

    // Never reaches here
    log("%s: done\n", __func__);
}

// Subsystem init fuction
int cmdproc_init(int level)
{
    int ret = 0;
    if(level == INIT_LEVEL_CMDPROC) {
        int ii;
        // Allocate the command array (never freed)
        cmd_q_buf = malloc(CMD_Q_MAX_SIZE * CMD_STR_MAX_LEN);
        if(!cmd_q_buf) {
            return -1;
        }
        for(ii = 0; ii < CMD_Q_MAX_SIZE; ii++) {
            cmd_q[ii] = &(cmd_q_buf[ii * CMD_STR_MAX_LEN]);
        }
        // Start the command processor job
        ret = util_start_thrd("cmdproc", cmdproc, NULL, NULL);
    }
    return ret;
}
