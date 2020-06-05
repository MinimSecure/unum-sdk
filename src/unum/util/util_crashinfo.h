// (c) 2018 minim.co
// unum helper utils, crash info collector common include

#ifndef _UTIL_CRASHINFO_H
#define _UTIL_CRASHINFO_H


// Crash data memory buffer size
#define UTIL_CRASHINFO_BUF_SIZE (4096 * 8)

// Crash info stack size to dump (in the # of 32bit words to print)
#define UTIL_CRASHINFO_STACK_WORDS (64 * 8)


// Struct/type for registers info common across platforms
typedef struct {
#define UTIL_REGINFO_REGS    0x01 // register content is avaialble
#define UTIL_REGINFO_IP_MAPS 0x02 // mem regions info is avaialble for IP
#define UTIL_REGINFO_SP_MAPS 0x04 // mem regions info is avaialble for SP
    int flags;// indicate what info is available
    void *ip; // instruction pointer
    void *sp; // stack pointer
    void *sp_r_start;   // sp mem region start
    void *sp_r_end;     // sp mem region end
    void *ip_r_start;   // sp mem region start
    void *ip_r_end;     // sp mem region end
    char ip_r_name[64]; // name of the ip region
} UTIL_REGINFO_COMMON_t;


// Print registers for the platform
// tid - id of the thread (must be stopped) to print the regs for
// buf - where to print
// len - length of the buffer
// ri - pointer to UTIL_REGINFO_COMMON_t where to save common regs info
// Returns: the same as for snprintf() or negative if fails to get registers
// Note: this function declared weak, platforms might not implement
//       it and the common crash handler should be able to deal w/ that
int __attribute__((weak)) util_crashinfo_regs(int tid, char *buf, int len,
                                              UTIL_REGINFO_COMMON_t *ri);

// pid - PID of the process
// tid - ID of the reported thread
// name - violation/event name
// ci - location where the caller wants us to put the pointer
//      w/ crash data collected. In the monitor process this
//      pointer is managed by the crash info collector code.
//      Do not free it till forked from the monitor process.
// returns: 0 - if successful, negative val othervise, the content
//          of ci ptr is NULL in case of an error
int util_crashinfo_get(int pid, int tid, char *name, TRACER_CRASH_INFO_t **ci);

#endif // _UTIL_CRASHINFO_H
