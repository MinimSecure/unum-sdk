// (c) 2017-2018 minim.co
// unum command processor common include file

#ifndef _CMDPROC_COMMON_H
#define _CMDPROC_COMMON_H

// The max size of the commands queue
#define CMD_Q_MAX_SIZE 24

// Maxsize of the command string in the command queue
#define CMD_STR_MAX_LEN 128

// Timeout (in sec) we retry failed commands that cannot be ignored.
#define CMD_RETRY_TIMEOUT 10

// Max time (in sec) for command execution (device specific .h can override)
#define CMD_MAX_EXE_TIME 60

// Shell command file location (device specific .h can override)
#define CMD_SHELL_PNAME "/tmp/unum_cmd.sh"

// Command processing function type for command with payload.
// The first parameter is the command string, the second is
// the command payload (downloaded from .../commands/<cmd>)
// and the 3rd one is the payload length.
// Unless CMD_RULE_F_DATA flag are used the second param is NULL
// the 3rd is 0.
// Returns 0 if successful
typedef int (*CMD_FUNC_DATA_t)(char *, char *, int);

// Processing function needs no params or return, always
// assumed to be successful.
typedef void (*CMD_FUNC_VOID_t)(void);

// Processing function needs command string, always
// assumed to be successful.
typedef void (*CMD_FUNC_CMD_t)(char *);

// Structure defining command processing rules
typedef struct {
    char *cmd; // Command string to match
    int flags; // Flags for the rule processing
    union {
        CMD_FUNC_DATA_t   act_data;   // CMD_RULE_F_DATA
        CMD_FUNC_VOID_t   act_void;   // CMD_RULE_F_VOID
        CMD_FUNC_CMD_t    act_cmd;    // CMD_RULE_F_CMD
    };
} CMD_RULE_t;

// CMD_RULE_t flags
#define CMD_RULE_M_FULL    0x00010000 // Match full string
#define CMD_RULE_M_SUBSTR  0x00020000 // Match the beginning of the string
#define CMD_RULE_M_ANY     0x00040000 // Matches anything (cmd can be NULL)
#define CMD_RULE_F_VOID    0x00000001 // Processing function is "void f(void)"
#define CMD_RULE_F_DATA    0x00000002 // Processing function needs payload data
#define CMD_RULE_F_RETRY   0x00000004 // Keep in queue till successful (forever)
#define CMD_RULE_F_CMD     0x00000008 // Processing function needs cmd string
#define CMD_RULE_END       0x10000000 // Empty rule indicating the end of the list

// Pointer to command processing rules array for the platform
extern CMD_RULE_t cmd_platform_rules[];

// Add command to the commands queue, retunrs 0 if successful
int cmdproc_add_cmd(const char *cmd);

// Generic processing function for commands running in shell.
// The parameters: command name, shell command script and its length
// Returns 0 if successful (exit code from the system() call)
int cmd_to_shell(char *cmd, char *script, int len);

// Subsystem init function
int cmdproc_init(int level);

// Pull in the telnet and pingflood includes
#include "telnet.h"
#include "pingflood.h"

// Pull in the fetch_urls generic command include
#include "fetch_urls.h"

#endif // _CMDPROC_COMMON_H
