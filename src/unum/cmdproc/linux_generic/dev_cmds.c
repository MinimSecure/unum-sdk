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

// unum device specifc command handling

#include "unum.h"

/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE

// Command processing rules for the platform
CMD_RULE_t cmd_rules[] = {
    { "reboot",             // reboot the device
      CMD_RULE_M_FULL | CMD_RULE_F_VOID,
      { .act_void = util_reboot }},
    { "restart_agent",      // restart the agent
      CMD_RULE_M_FULL | CMD_RULE_F_VOID,
      { .act_void = util_restart }},
    { "wireless_scan",      // force wireless scan
      CMD_RULE_M_FULL | CMD_RULE_F_VOID,
      { .act_void = cmd_wireless_initiate_scan }},
    { "pull_router_config", // load and apply device config file
      CMD_RULE_M_FULL | CMD_RULE_F_RETRY,
      { .act_data = cmd_update_config }},
    { "speedtest", // runs a speedtest
      CMD_RULE_M_FULL | CMD_RULE_F_VOID,
      { .act_void = cmd_speedtest }},
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
    // CMD_RULE_M_ANY must be the last one
    { "shell_cmd", // generic commands, pass to shell
      CMD_RULE_M_ANY | CMD_RULE_F_DATA,
      { .act_data = cmd_to_shell }},
    { NULL, CMD_RULE_END }  // End command rules
};

