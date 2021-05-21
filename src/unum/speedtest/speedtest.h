// (c) 2017-2018 minim.co
// Unum speedtest include file

#ifndef _SPEEDTEST_H
#define _SPEEDTEST_H

// Performs a speedtest, blocking until complete.
void speedtest_perform(THRD_PARAM_t *p);

// Performs a speedtest asynchronously.
// This function starts the speedtest in a new thread and returns immediately.
void cmd_speedtest(char *test_cmd);

#endif // _SPEEDTEST_H
