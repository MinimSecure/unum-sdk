// Copyright 2018 Minim Inc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// unum helper utils include file

#ifndef _UTIL_COMMON_H
#define _UTIL_COMMON_H

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
#define EXIT_RESTART  12 // Restart the process
#define EXIT_REBOOT   13 // Reboot the device

// Makes a hex digit from a 4-bit value
#define UTIL_MAKE_HEX_DIGIT(a) ((((a)&0xf) > 9) ? (((a)&0xf)+87) : (((a)&0xf)+'0'))

// memory heap wrappers
#define UTIL_MALLOC(_s)      malloc(_s)
#define UTIL_CALLOC(_n, _p)  calloc((_n), (_p))
#define UTIL_REALLOC(_p, _s) realloc((_p), (_s))
#define UTIL_FREE(_p)        free(_p)

// MIN/MAX
#define UTIL_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define UTIL_MAX(a, b) (((a) > (b)) ? (a) : (b))

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

// Get firmware version string (stored at a static location)
char *util_fw_version();

// Shutdown/terminate the agent (the function does not return to the caller)
void util_shutdown(int err_code);

// Restart agent (the function does not return to the caller)
void util_restart(void);

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

// Return uptime in specified fractions of the second (rounded to the low)
unsigned long long util_time(unsigned int fraction);

// Sleep the specified number of milliseconds
void util_msleep(unsigned int msecs);

// Save data from memory to a file, returns -1 if fails.
// crmode - permissions for creating new files (see man 2 open)
int util_buf_to_file(char *file, void *buf, int len, mode_t crmode);

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

// In general it is similar to system() call, but tries to kill the
// command being waited for if the timeout (in seconds) is reached
// and does not change any signal handlers.
// If pid_file is not NULL the function tries to store there the
// PID of the running process while it is executing.
int util_system(char *cmd, unsigned int timeout, char *pid_file);

// Call an executable in shell and capture its stdout in a buffer.
// Returns: negative if fails to start (see errno for the error code) or
//          the length of the captured info (excluding terminating 0),
//          if the returned value is not negative the status (if not NULL)
//          is filled in with the exit status of the command or
//          -1 if unable to get it.
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

#endif // _UTIL_COMMON_H
