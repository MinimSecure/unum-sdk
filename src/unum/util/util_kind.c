#include "unum.h"

// Walkthrough the kind array and get the kind
// Returns wirless interface kind on success
// -1 otherwise
int _util_get_interface_kind(char *ifname, WIRELESS_KIND_t *kind)
{
    int ii;

    for(ii = 0; kind[ii].kind != -1; ii++) {
        if (strncmp(kind[ii].ifname, ifname, IFNAMSIZ) == 0) {
            return kind[ii].kind;
        }
    }
    return -1;
}

