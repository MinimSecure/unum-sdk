// (c) 2019 minim.co
// unum device specifc command handling

#include "unum.h"

/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE

// Command processing rules for the platform, the rules shared across all
// platforms should be added to the cmd_gen_rules[] in cmdproc.c
CMD_RULE_t cmd_platform_rules[] = {
    { NULL, CMD_RULE_END }  // End command rules
};

