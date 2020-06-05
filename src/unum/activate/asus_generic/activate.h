// (c) 2019 minim.co
// unum router platform specific activate include file

#ifndef _ACTIVATE_H
#define _ACTIVATE_H

#include "../activate_common.h"


// Override activate startup time for Asus Router since it
// always has to do provisioning and will be delayed there.

// The first try to activate is random at 0-ACTIVATE_PERIOD_START)
#undef ACTIVATE_PERIOD_START
#define ACTIVATE_PERIOD_START  0

#endif // _ACTIVATE_H
