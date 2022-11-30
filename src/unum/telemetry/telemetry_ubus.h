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

#ifdef FEATURE_SUPPORTS_SAMBA

#define MAX_UUID_SIZE 37

// The structure for collecting state of the current device being reported
// in the telemetry JSON
typedef struct _SMBT_JSON_TPL_DEVICE_STATE {
    char id[MAX_UUID_SIZE];    // The id of the smb/usb device
    unsigned long bytes_free;  // bytes free on the smb/usb device
    unsigned long bytes_total; // tatal bytes on the smb/usb device
} SMBT_JSON_TPL_DEVICE_STATE_t;

extern JSON_VAL_FARRAY_t smb_fa_ptr;

#endif // FEATURE_SUPPORTS_SAMBA

#endif // __TELEMETRY_UBUS_H
