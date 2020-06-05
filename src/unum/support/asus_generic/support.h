// (c) 2019 minim.co
// unum firmware updater platform specific include file

// Common firmware updater header shared across all platforms
#include "../support_common.h"

#ifndef _SUPPORT_H
#define _SUPPORT_H

// This define sets the file name that if created makes support
// process attempt to make a connection.
#define SUPPORT_CONNECT_FILE_NAME "/var/tmp/support_chk_flag"

// SSH command to try connecting to the support portal
// The available parameters are:
//   username - the user name in format mac<LAN_MAC_ADDR> (len w/ 0: 16)
//   host - the support host name or IP (in DNS failure mode)
//   lan_ip - the IP address of the LAN main interface
//            (or 127.0.0.1 if unable to get), (len w/ 0: 16)
#define SUPPORT_FORWARD_LIST "\
-R 10122:127.0.0.1:22 -R 10180:127.0.0.1:80 \
-R 10222:127.0.0.1:22 -R 10280:127.0.0.1:80 \
-R 10322:127.0.0.1:22 -R 10380:127.0.0.1:80 \
-R 10422:127.0.0.1:22 -R 10480:127.0.0.1:80 \
-R 10522:127.0.0.1:22 -R 10580:127.0.0.1:80 \
-R 10622:127.0.0.1:22 -R 10680:127.0.0.1:80 \
-R 10722:127.0.0.1:22 -R 10780:127.0.0.1:80 \
-R 10822:127.0.0.1:22 -R 10880:127.0.0.1:80 \
-R 10922:127.0.0.1:22 -R 10980:127.0.0.1:80 \
-R 11022:127.0.0.1:22 -R 11080:127.0.0.1:80 \
"
#define SUPPORT_CONNECT_CMD "/usr/bin/ssh -N -y -i /.ssh/id_rsa -p 2222 -g " \
                            "%s@%s " SUPPORT_FORWARD_LIST
#define SUPPORT_CONNECT_CMD_PARAMS username,host
#define SUPPORT_CONNECT_CMD_MAX_LEN (sizeof(SUPPORT_CONNECT_CMD) + \
                                     sizeof(SUPPORT_PORTAL_HOSTNAME) + \
                                     sizeof(SUPPORT_PORTAL_FB_IP) + 16)

#endif // _SUPPORT_H
