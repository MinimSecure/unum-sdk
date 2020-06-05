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

// unum command processor platform specific include file

// Common cmdproc header shared across all platforms
#include "../cmdproc_common.h"

#ifndef _CMDPROC_H
#define _CMDPROC_H

// Processing function for loading and applying device configuration.
// The parameter is the command name, returns 0 if successful
int cmd_update_config(char *cmd, char *s_unused, int len_unused);

#endif // _CMDPROC_H
