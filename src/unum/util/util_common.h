// (c) 2017-2020 minim.co
// unum helper utils include file

#ifndef _UTIL_COMMON_H
#define _UTIL_COMMON_H

// Only platform independent util code headers can be included here

// Pull in DNS utils header
#include "util_dns.h"
// Zlib wrappers header
#include "util_zlib.h"


// Get number of elements in an array
#define UTIL_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]) \
     + sizeof(typeof(int[1 - 2 * !!__builtin_types_compatible_p(typeof(a), \
              typeof(&a[0]))])) * 0)

// Offset of a field in the structure (using GCC built-in)
#define UTIL_OFFSETOF(type, member)  __builtin_offsetof(type, member)

// Stringify a definition
#define _UTIL_STR(x) #x
#define UTIL_STR(x) _UTIL_STR(x)

// TRUE/FALSE values
#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

// Agent main process exit status codes (restart/reboot
// require unum process monitoring to work/being enabled).
#ifndef EXIT_SUCCESS
#  define EXIT_SUCCESS  0 // Terminate agent with success
#endif
#ifndef EXIT_FAILURE
#  define EXIT_FAILURE  1 // Terminate agent with failure
#endif
#define EXIT_REBOOT     2 // Reboot the device

// Unum Start Reason codes (take the same numbering space as the above)
// Pass to util_restart() to report the restart reason to the server or
// (if the process is not monitored) just set the appropriate exit code
enum unum_start_reason {
    UNUM_START_REASON_UNKNOWN = 0,  // power cycle, reset or unknow (default)
    UNUM_RESERVED = EXIT_REBOOT,    // reserved for requesting system reboot
    // The reason codes below can be passed to util_restart()
    UNUM_START_REASON_CRASH,        // agent restart due to a crash
    UNUM_START_REASON_RESTART,      // self initiated restart
    UNUM_START_REASON_REBOOT,       // self initiated reboot (not yet)
    UNUM_START_REASON_UPGRADE,      // firmware upgrade (not yet)
    UNUM_START_REASON_START_FAIL,   // agent failed to start
    UNUM_START_REASON_MODE_CHANGE,  // opmode change
    UNUM_START_REASON_CONNCHECK,    // restart by conncheck module
    UNUM_START_REASON_PROVISION,    // restart by provisioning
    UNUM_START_REASON_SUPPORT_FAIL, // support portal failed to start
    UNUM_START_REASON_TEST_RESTART, // unit test initiated crash
    UNUM_START_REASON_TEST_FAIL,    // test failed to start
    UNUM_START_REASON_DEVICE_INFO,  // failed to get device info
    UNUM_START_REASON_WD_TIMEOUT,   // watchdog timeout
    UNUM_START_REASON_REBOOT_FAIL,  // failed to reboot
    UNUM_START_REASON_SERVER,       // restart command from server
    UNUM_START_REASON_FW_START_FAIL,// failed to start FW updater
    UNUM_START_REASON_FW_CONN_FAIL, // failed to reach release server
    UNUM_START_REASON_KILL,         // terminated by kill (set by monitor)
};

// Interface kind sent in wireless radio telemetry and dev telemtry
enum INTERFACE_KIND {
    UNUM_INTERFACE_KIND_HOME2       = 0,       // 2.4Ghz Home Network
    UNUM_INTERFACE_KIND_HOME5       = 1,       // 5Ghz Home Network
    UNUM_INTERFACE_KIND_GUEST2      = 2,       // 2.4Ghz Guest Network
    UNUM_INTERFACE_KIND_GUEST5      = 3,       // 5Ghz Guest Network
    UNUM_INTERFACE_KIND_MESH5       = 4,       // 5Ghz Mesh Network
    UNUM_INTERFACE_KIND_MULTIBAND   = 5,       // 5Ghz Multiband
    UNUM_INTERFACE_KIND_LAN         = 6,       // LAN
    UNUM_INTERFACE_KIND_WAN         = 7,       // LAN
    UNUM_INTERFACE_KIND_MOCA        = 8,       // MOCA
    UNUM_INTERFACE_KIND_LAN_BRIDGE  = 9,       // LAN Bridge
    UNUM_INTERFACE_KIND_ETHERNET    = 10,      // Ethernet
    UNUM_INTERFACE_KIND_MESH2       = 11,      // 2.4Ghz Mesh Network
    UNUM_INTERFACE_KIND_MAX,                   // For max size
};

// Radio kind sent in wireless radio telemetry
enum RADIO_KIND {
    // We are starting the unum with 5Ghz instead of 2.4Ghz
    // to be in sync with the values defined on the server
    UNUM_RADIO_KIND_5       = 0,       // 5Ghz Radio
    UNUM_RADIO_KIND_2       = 1,       // 2.4Ghz Radio
    UNUM_RADIO_KIND_6       = 2,       // 6Ghz Radio
};

// Makes a hex digit from a 4-bit value
#define UTIL_MAKE_HEX_DIGIT(a) ((((a)&0xf) > 9) ? (((a)&0xf)+87) : (((a)&0xf)+'0'))

// memory heap wrappers
#define UTIL_MALLOC(_s)      malloc(_s)
#define UTIL_CALLOC(_n, _p)  calloc((_n), (_p))
#define UTIL_REALLOC(_p, _s) realloc((_p), (_s))
#define UTIL_FREE(_p)        free(_p)

// Macros for handling bitmaps in arrays
#define UTIL_BIT_GET(_p, _i) (((_p)[((_i) / (sizeof(*(_p)) * 8))] >> \
                              ((_i) % (sizeof(*(_p)) * 8))) & 1)
#define UTIL_BIT_SET(_p, _i) ((_p)[((_i) / (sizeof(*(_p)) * 8))] |= \
                                  (1 << ((_i) % (sizeof(*(_p)) * 8))))
#define UTIL_BIT_CLR(_p, _i) ((_p)[((_i) / (sizeof(*(_p)) * 8))] &= \
                                  ~(1 << ((_i) % (sizeof(*(_p)) * 8))))

// MIN/MAX
#define UTIL_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define UTIL_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define UTIL_RANGE_CHECK(VAL, MIN, MAX) {\
    if(VAL > MAX) {\
        log("%s: %s (%d) above max (%d)\n", __func__, #VAL, VAL, MAX);\
        VAL = MAX;\
    } else if(VAL < MIN) {\
        log("%s: %s (%d) below min (%d)\n", __func__, #VAL, VAL, MIN);\
        VAL = MIN;\
    }\
};

// FIFO Queue macros (_h - queue head var ptr, _i - item var ptr),
// declare head as: UTIL_QI_t head = { &head; &head };
// queue item type as: struct <yourtype> { UTIL_Q_HEADER(); ... };
// UTIL_Q_ADD(_h, _i) - add item to the end of the queue
// UTIL_Q_REM(_h, _p) - remove the next queue item from the front, assign it
//                      to '_p' and resolve to '_p' (NULL if no more items)
// UTIL_Q_FOREACH(_h, _ii) {...} - loop through the items
// the rest is self explanatory
#define UTIL_Q_HEADER() struct _UTIL_QI *prev; struct _UTIL_QI *next
#define UTIL_Q_NEXT(_i) (typeof(_i)(((UTIL_QI_t *)(_i))->next))
#define UTIL_Q_PREV(_i) (typeof(_i)(((UTIL_QI_t *)(_i))->prev))
#define UTIL_Q_DEL_ITEM(_i) ( \
    ((_i)->next->prev = (_i)->prev), \
    ((_i)->prev->next = (_i)->next), \
    ((_i)->next = (_i)->prev = NULL) \
    )
#define UTIL_Q_ADD(_h, _i) ( \
    ((_i)->next = (_h)), \
    ((_i)->prev = (_h)->prev), \
    ((_h)->prev->next = ((UTIL_QI_t *)(_i))), \
    ((_h)->prev = ((UTIL_QI_t *)(_i))) \
    )
#define UTIL_Q_REM(_h, _p) ( \
    ((_h)->next == (_h)) ? ((_p) = NULL) : (((_p) = (void *)(_h)->next), \
                                            UTIL_Q_DEL_ITEM((_p)), (_p)) \
    )
// Here _ii should be the name of the pointer var you then use inside the
// loop code block to refer to the queue items the loop is iterating over.
#define UTIL_Q_FOREACH(_h, _ii) \
    for((_ii) = (typeof(_ii))(_h)->next; \
        (_h) != ((UTIL_QI_t *)(_ii)); \
        (_ii) = (typeof(_ii))(_ii)->next)
// Empty queue header type
typedef struct _UTIL_QI { UTIL_Q_HEADER(); } UTIL_QI_t;


// Convert string to lowercase
static __inline__ void str_tolower(char *str) {
    while(str && *str) {
        *str = tolower(*str);
        ++str;
    }
}

// Check if a pathname exists (returns non-FALSE value if pathname exists)
static __inline__ int util_path_exists(char *pathname)
{
    struct stat st;
    return (stat(pathname, &st) == 0);
}

// Get firmware version string (stored at a static location)
char *util_fw_version();

// Shutdown/terminate the agent (the function does not return to the caller)
// err - ERROR_SUCCESS or ERROR_FAILURE (any non-zero value is
//       treated as ERROR_FAILURE)
void util_shutdown(int err);

// Restart agent (the function does not return to the caller)
// reason - see enum unum_start_reason
void util_restart(int reason);

// Restart command from server
void util_restart_from_server(void);

// Reboot the device
void util_reboot(void);

// Factory reset the device (triggers reboot if successful)
// Some platforms (like linux_generic) might just ignore it.
void util_factory_reset(void);

// Platform specific factory reset function (used if the platform
// does not define FACTORY_RESET_CMD)
int __attribute__((weak)) util_platform_factory_reset(void);


// Populate auth_info_key Auth Info Key
// For Gl-inet B1300 it is serial number
// For now (03/17/19), for all other devices, auth_info_key is not updated
void __attribute__((weak)) util_get_auth_info_key(char *auth_info_key, int max_key_len);

// Populate Serial Number if the device supports
void __attribute__((weak)) util_get_serial_num(char *serial_num, int max_sn_len);

// Get all the MAC Addresses on the router
int  __attribute__((weak)) util_get_mac_list(char *mac_list, int max_len);

// Return uptime in specified fractions of the second (rounded to the low)
// Note: it's typically (but not necessarily) the same as uptime
unsigned long long util_time(unsigned int fraction);

// Sleep the specified number of milliseconds
void util_msleep(unsigned int msecs);

// Create a blank file
int touch_file (char *file, mode_t crmode);

// Save data from memory to a file, returns -1 if fails.
// crmode - permissions for creating new files (see man 2 open)
int util_buf_to_file(char *file, void *buf, int len, mode_t crmode);

// Read data from file to a buffer
// file - name of the file
// buf - buffer to save the data to
// buf_size - maximum size of the buffer
// Returns: negative - error, use errno to get the error code
//          positive - number of bytes read or the file size if
//                     buf_size was not large enough to read all
size_t util_file_to_buf(char *file, char *buf, size_t buf_size);

// Compare two files.
// Parameters:
// file1, file2 - pathnames of the files to compare
// must_exist - flag that tells the functon how to treat the non-existent files,
//              if TRUE both files must exist, if FALSE the non-existen files
//              are treated as empty.
// Returns: TRUE - files match, FALSE - differ, negative - error accesing
//          file(s) (-1 - file1, -2 - file2)
int util_cmp_files_match(char *file1, char *file2, int must_exist);

// Replace CR/LF w/ LF in a string
void util_fix_crlf(char *str_in);

// In general it is similar to system() call, but it does not change any
// signal handlers.
// It also allows to specify callback "cb" and time period in sconds
// after which to call that callback. Parameters:
// cmd - the commapd line to pass to shell
// pid_file - pointer to the PID file name, if not NULL the function tries to
//            store there the PID of the running process while it is executing
// cb_time - positive - specifies time in seconds till callback is called,
//           0 - call unconditionally after fork() (if process is created),
//           negative - the callback calling is disabled
// cb - pointer to the callback function (see below), NULL - no call
//
// The UTIL_SYSTEM_CB_t callback is passed in the following parameters:
// pid     - PID of the running process
// elapsed - the time in seconds since the command started
// cb_prm  - caller's parameter (void pointer)
// The callback return value can be negative to disable future calling.
// If the value is 0 or positive it specifies the number of seconds till
// next call to the callback (0 still means call in 1 second).
typedef int (*UTIL_SYSTEM_CB_t)(int /* pid */, unsigned int /* elapsed */,
                                void * /* param */);
int util_system_wcb(char *cmd,
                    int cb_time, UTIL_SYSTEM_CB_t cb, void *cb_prm);

// In general it is similar to system() call, but tries to kill the
// command being waited for if the timeout (in seconds) is reached
// and does not change any signal handlers.
// If pid_file is not NULL the function tries to store in that file
// the PID of the running process while it is executing.
int util_system(char *cmd, unsigned int timeout, char *pid_file);

// Call cmd in shell and capture its stdout in a buffer.
// Returns: negative if fails to start cmd (see errno for the error code), the
//          length of the captured info (excluding terminating 0) if success.
//          If the returned value is not negative the pstatus (unless NULL)
//          contains command execution status (zero if the command terminated
//          with status 0, negative if error).
// If the buffer is too short the remainig output is ignored.
// The captured output is always zero-terminated.
int util_get_cmd_output(char *cmd, char *buf, int buf_len, int *pstatus);

// Hash function
// dptr - ptr to data, must be 4-bytes aligned
// len - length of data
// Returns: 32 bit hash
unsigned int util_hash(void *dptr, int len);

// Clean up multiple tabs & spaces from a string
// str - ptr to the string to clean up
void util_cleanup_str(char *str);

// Configure the agent to the specified operation mode
// opmode - the operation mode string pointer
// Returns: 0 - success, negative - error setting the opmode
// Note: this function is called from command line procesing routines
//       before any init is done (i.e. even the log msgs would go to stdout)
int util_set_opmode(char *opmode);

// Optional platform init function
int __attribute__((weak)) util_platform_init(int level);

// Optional platform operation mode change handler, called if the
// opmode is changing after the INIT_LEVEL_SETUP is complete
// (i.e. at activate). The function does not have to set the new flags
// or modify the mode string in unum_config, just do platform specific
// canges and return 0 for the common code to finish making changes.
// old_flags - current opmode flags
// new_flags - new opmode flags
// Returns: 0 - ok, negative - can't accept the new flags
int __attribute__((weak)) platform_change_opmode(int old_flags, int new_flags);

// Subsystem init function
int util_init(int level);

#define RESOURCE_PROTO_HTTPS        "https"

#define RESOURCE_TYPE_API           0
#define RESOURCE_TYPE_API_STR       "api"

#define RESOURCE_TYPE_MY            1
#define RESOURCE_TYPE_MY_STR        "my"

#define RESOURCE_TYPE_RELEASES      2
#define RESOURCE_TYPE_RELEASES_STR  "releases"

#define RESOURCE_TYPE_PROVISION     3
#define RESOURCE_TYPE_PROVISION_STR "provision"

// Build URL
//
// eg: build_url(RESOURCE_PROTO_HTTPS, RESOURCEE_TYPE_API, string, 256,
//               "/v3/endpoint/%s", "abcdefg")
//     string = "https://api.minim.co/v3/endpoint/abcdefg"
//
// proto    - url protocol string HTTPS, HTTP, etc
// type     - resource type from RESOURCE_TYPE_* enums above
// url      - pointer to string
// length   - length of string to avoid overrun
// template - string format, will be appended after scheme, host, etc
// ...      - args for template
void util_build_url(char *proto, int type, char *url, unsigned int length,
                    const char *template, ...);

// This function performs a RELEASE / RENEW of DHCP lease on the WAN
// interface if the WAN interface is set for DHCP. If the platform
// doesn't support this, it should return without doing anything.
int __attribute__((weak)) platform_release_renew(void);

// These functions indicate if a platform supports the luci rpc
// mechanism and, if so, which WAN interface configuration mechanisms
// are supported.
int __attribute__((weak)) platform_luci_rpc_supported(void);
int __attribute__((weak)) platform_luci_rpc_wan_dhcpv4_supported(void);
int __attribute__((weak)) platform_luci_rpc_wan_pppoe_supported (void);
int __attribute__((weak)) platform_luci_rpc_wan_static_supported(void);

// Match pattern to a string
// ptr - pointer to the pattern, the pattern can contain following
//       special symbols:
//       '*' - matches 0 or more characters in the target string
//       '?' - matches exactly one character
//       '\' - escape (prefix any special pattern symbol to treat it as is)
// ptr_len - length of the pattern (can be -1 if the pattern is 0-terminated)
// str - pointer to a string to try matcing to the pattern
// str_len - length of the string (can be -1 if the string is 0-terminated)
// Returns: 0 - no match, 1 - match
// Note: the function uses space on stack proportional to the string length
//       (approximately 1 bit per character, i.e. 1KB for a 8K long string)
// Note1: incomplete escape sequence at the end (i.e. "abc\") is ignored
int util_match_str(char *ptr, int ptr_len, char *str, int str_len);

// Get Interface kind
// Parameters:
// ifname - The name of the interface
int __attribute__((weak)) util_get_interface_kind(char *ifname);
// Get Radio kind
// Returns Radio kind
// -1 if the Radio name is not found
int __attribute__((weak)) util_get_radio_kind(char *ifname);
#endif // _UTIL_COMMON_H
