// Copyright 2019 Minim Inc
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

// config subsystem platform include file

#ifndef _CONFIG_H
#define _CONFIG_H

#include <openssl/md5.h>

// Enable internal config download support, comment it out if
// want the config download to be handled by a scripted command
#define CONFIG_DOWNLOAD_IN_AGENT


// Typedef for config UID on the platform (MD5 hash here).
typedef char CONFIG_UID_t[16];

// The linux_generic platform uses two shell scripts to manage device config.
// The "read" script is executed and its standard output is considered the
// current config. The "apply" script is executed with the updated config
// written to the script's standard input.
// The default implementations do nothing and can be customized to suit
// individual applications.
// See also: https://github.com/MinimSecure/unum-sdk/blob/master/README-linux_generic.md#configuration-format


// Define the "read" and "apply" scripts, as described above.
#ifndef PLATFORM_CONFIG_READ_SCRIPT
#  define PLATFORM_CONFIG_READ_SCRIPT   "/opt/unum/sbin/read_conf.sh"
#endif // ! PLATFORM_CONFIG_READ_SCRIPT

#ifndef PLATFORM_CONFIG_APPLY_SCRIPT
#  define PLATFORM_CONFIG_APPLY_SCRIPT  "/opt/unum/sbin/apply_conf.sh"
#endif // ! PLATFORM_CONFIG_APPLY_SCRIPT

// Pull in the common include
#include "../config_common.h"

#endif // _CONFIG_H
