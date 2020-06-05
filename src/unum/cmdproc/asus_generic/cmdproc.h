// (c) 2019 minim.co
// unum command processor platform specific include file

// Common cmdproc header shared across all platforms
#include "../cmdproc_common.h"

#ifndef _CMDPROC_H
#define _CMDPROC_H

// Processing function for loading and applying device configuration.
// The parameter is the command name, returns 0 if successful
int cmd_update_config(char *cmd, char *s_unused, int len_unused);

#endif // _CMDPROC_H
