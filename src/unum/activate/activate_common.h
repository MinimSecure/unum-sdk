// (c) 2017-2019 minim.co
// unum router activate include file

#ifndef _ACTIVATE_COMMON_H
#define _ACTIVATE_COMMON_H

// The first try to activate is random at 0-ACTIVATE_PERIOD_START)
#define ACTIVATE_PERIOD_START   0
// After each failure the delay is incremented by random from
// 0-ACTIVATE_PERIOD_INC range
#define ACTIVATE_PERIOD_INC    30
// Ater reaching ACTIVATE_MAX_PERIOD the delay stays there
#define ACTIVATE_MAX_PERIOD   180

// Maximum size for buffer holding MAC Addresses
// This can hold upto 28 Addresses
#define MAX_MAC_LIST_LEN 1024

// This function blocks the caller till device is activated.
void wait_for_activate(void);

// This function returns TRUE if the agent is activated, FALSE otherwise.
int is_agent_activated(void);

// Set the activate event (for tests to bypass the activate step)
void set_activate_event(void);

// Subsystem init function
int activate_init(int level);

#endif // _ACTIVATE_COMMON_H
