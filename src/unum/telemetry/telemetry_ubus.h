// (c) 2018 minim.co
// unum router cpuinfo include file

#ifndef __TELEMETRY_UBUS_H
#define __TELEMETRY_UBUS_H

// refresh the ubus telemetry
void telemetry_ubus_refresh(void);

// initialise the ubus telemetry
void telemetry_ubus_init(void);

#ifdef FEATURE_IPV6_TELEMETRY
// Return a pointer to the table of prefixes - contains up to
// MAX_IPV6_ADDRESSES_PER_MAC entries, invalid entries are all zeroes.
const DEV_IPV6_CFG_t* telemetry_ubus_get_ipv6_prefixes(void);
#endif // FEATURE_IPV6_TELEMETRY

#endif // __TELEMETRY_UBUS_H
