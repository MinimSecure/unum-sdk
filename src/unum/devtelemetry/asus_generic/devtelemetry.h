// (c) 2019 minim.co
// device telemetry collection and reporting

#ifndef _DEVTELEMETRY_H
#define _DEVTELEMETRY_H

// Pull in the common include
#include "../devtelemetry_common.h"

// Note: all the below limits are for the information
// collected during single telemetry time period

// Max number of devices
#define DTEL_MAX_DEV 256

// Max number of connections (total for all devices)
#define DTEL_MAX_CONN 4096

// Max DNS distinct names to keep
#define DTEL_MAX_DNS_NAMES 512
// Max IP addresses to map to the DNS names
#define DTEL_MAX_DNS_IPS  1024

#endif // _DEVTELEMETRY_H

