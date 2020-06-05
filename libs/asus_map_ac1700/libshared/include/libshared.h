#ifndef _LIBSHARED_H
#define _LIBSHARED_H

// Some prototypes are taken from Asus SDK to use these
// functions in unum-v2
extern int notify_rc(const char *event_name);
extern int notify_rc_after_wait(const char *event_name);
extern int notify_rc_after_period_wait(const char *event_name, int wait);
extern int notify_rc_and_wait(const char *event_name);
extern int notify_rc_and_wait_1min(const char *event_name);
extern int notify_rc_and_wait_2min(const char *event_name);
extern int notify_rc_and_period_wait(const char *event_name, int wait);

#endif // _LIBSHARED_H
