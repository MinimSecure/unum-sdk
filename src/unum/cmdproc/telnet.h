// (c) 2018 minim.co
// telnet include file

#ifndef _TELNET_H
#define _TELNET_H

#define MAX_TELNET_OUTPUT 256
#define MAX_URI_PART 32

typedef struct _TELNET_INFO {
    unsigned char output[MAX_TELNET_OUTPUT];
    int length;
    char ip_addr[MAX_URI_PART];
    int port;
    char username[MAX_URI_PART];
    char password[MAX_URI_PART];
} TELNET_INFO_t;

// Perform a telnet request with an attempted login
// Fills in the output (after the password was entered)
// Returns 0: success; -1: failure
int attempt_telnet_login(TELNET_INFO_t *info);

// Shatters a telnet URI into its component parts
// Fills in ip address, port, username, and password
// Note: REQUIRES a username and password
// Returns 0: success; -1: failure
int split_telnet_uri(TELNET_INFO_t *info, char *uri, int length);

#endif // _TELNET_H
