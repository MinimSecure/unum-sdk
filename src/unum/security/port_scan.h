// (c) 2017-2019 minim.co
// Port Scanning subsystem include file

#ifndef _PORT_SCAN_H
#define _PORT_SCAN_H

// Port scan queue limit
#define PORT_SCAN_QUEUE_LIMIT 64

// Default delay in milliseconds between SYN sends
#define PORT_SCANNER_DEFAULT_SCAN_DELAY 0

// Default time in milliseconds to wait after sending SYNs to
// all the ports being scanned (if retrying wait after each send cycle)
#define PORT_SCANNER_DEFAULT_WAIT_TIME 5000

// Default number of retries for the scan (0 - no retry, scan only once)
#define PORT_SCANNER_DEFAULT_SYN_RETRIES 1


// Download URL queue item data structure
typedef struct _PORT_SCAN_DEVICE {
    UTIL_Q_HEADER();   // declares fields used by queue macros
    json_t *devices;   // the JSON root element (for reference counting)
    json_t *device;    // the JSON representing the device
} PORT_SCAN_DEVICE_t;

// The "port_scan" command processor.
// Note: declared weak so platforms not doing it do not have to stub
int __attribute__((weak)) cmd_port_scan(char *cmd, char *s, int s_len);

#ifdef DEBUG
void test_port_scan(void);
#endif // DEBUG

#endif // _PORT_SCAN_H
