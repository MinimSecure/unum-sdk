// (c) 2017-2018 minim.co
// unum router telemetry include file

#ifndef _TELEMETRY_H
#define _TELEMETRY_H

#include "meminfo.h"
#include "cpuinfo.h"
#include "iptables.h"

// Telemetry reporting period (in sec)
#define TELEMETRY_PERIOD 15
// System info (mem and CPU usage reporting period)
#define SYSINFO_TELEMETRY_PERIOD 60
// iptables rules reporting period
#define IPT_TELEMETRY_PERIOD 300

// Subsystem init function
int telemetry_init(int level);

#endif // _TELEMETRY_H
