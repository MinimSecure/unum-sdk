// (c) 2018 minim.co
// Helper for retrieving various platform specific crashed thread
// state info.

#include "unum.h"


// The gcc defines could be used for detecting various architectures
// https://sourceforge.net/p/predef/wiki/Architectures/

// Look up the architecture's traps.c in the kernel sources or the
// platform gdb/gdbserv code to see an example how to decypher and
// print the registers.

// CPU Register printout support for 32bit ARM.
// The PTRACE_GETREGS should be returning 18 user registers (according
// to arch/arm/include/asm/ptrace.h for the platform).

// The defines below are for accessing the the specific
// specifc register data in the array of long values by offset.
#define ARM_REGS    18
#define ARM_ORIG_r0 17
#define ARM_cpsr    16
#define ARM_pc      15
#define ARM_lr      14
#define ARM_sp      13
#define ARM_r12     12
#define ARM_r11     11
#define ARM_r10     10
#define ARM_r9       9
#define ARM_r8       8
#define ARM_r7       7
#define ARM_r6       6
#define ARM_r5       5
#define ARM_r4       4
#define ARM_r3       3
#define ARM_r2       2
#define ARM_r1       1
#define ARM_r0       0

// Memory for capturing the crashed thread registers
static unsigned long regs[ARM_REGS];

// Print ARM registers
// tid - id of the thread (must be stopped) to print the regs for
// buf - where to print
// len - length of the buffer
// ri - pointer to UTIL_REGINFO_COMMON_t where to save common regs info
// Returns: the same as for snprintf() or negative if fails to get registers
int util_crashinfo_regs(int tid, char *buf, int len, UTIL_REGINFO_COMMON_t *ri)
{
#define PRNBUF(args...) \
    (offset = ((len > count) ? count : len),\
     count += snprintf(buf + offset, len - offset, args))

    int offset = 0;
    int count = 0;

    // Get the registers
    if(ptrace(PTRACE_GETREGS, tid, &regs, &regs) != 0) {
        log("%s: tid: %d, PTRACE_GETREGS error: %s\n",
            __func__, tid, strerror(errno));
        return count;
    }

    for(;;)
    {
        int ii, jj, w = 8;

        PRNBUF("Registers:\n");

        // Print user registers
        char *regname[] = {
           "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
           "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc",
           NULL
        };
        int regoffset[] = {
           ARM_r0, ARM_r1, ARM_r2, ARM_r3, ARM_r4, ARM_r5, ARM_r6, ARM_r7,
           ARM_r8, ARM_r9, ARM_r10, ARM_r11, ARM_r12, ARM_sp, ARM_lr, ARM_pc,
           -1
        };
        for(jj = 0; jj * 8 < ARM_REGS; ++jj) {
            for(ii = jj * 8; ii < (jj + 1) * 8 && regname[ii]; ++ii) {
                PRNBUF(" %*s", w, regname[ii]);
            }
            PRNBUF("\n");
            for(ii = jj * 8; ii < (jj + 1) * 8 && regoffset[ii] >= 0; ++ii) {
                PRNBUF(" %0*lx", w, regs[regoffset[ii]]);
            }
            PRNBUF("\n");
        }
        PRNBUF("\n");

        PRNBUF(" CPSR    : %0*lx\n", w, regs[ARM_cpsr]);
        PRNBUF(" ORIGR0  : %0*lx\n", w, regs[ARM_ORIG_r0]);

        ri->ip = (void*)regs[ARM_pc];
        ri->sp = (void*)regs[ARM_sp];
        ri->flags |= UTIL_REGINFO_REGS;

        PRNBUF("\n");

        break;
    }

    return count;
}
