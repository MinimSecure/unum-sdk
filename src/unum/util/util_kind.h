// (c) 2021 minim.co
// unum helper utils used to find the interface kind

#ifndef _UTIL_KIND_H
#define _UTIL_KIND_H
// Walkthrough the kind array and get the kind
// Returns wirless interface kind on success
// -1 otherwise
int _util_get_interface_kind(char *ifname, WIRELESS_KIND_t *kind);
#endif // _UTIL_KIND_H
