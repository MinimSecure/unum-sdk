// (c) 2018 minim.co
// Helper for retrieving various platform specific crashed thread
// state info.

#include "unum.h"


// The gcc defines could be used for detecting various architectures
// https://sourceforge.net/p/predef/wiki/Architectures/

// Look up the architecture's traps.c in the kernel sources or the
// platform gdb/gdbserv code to see an example how to decypher and
// print the registers.

// CPU Register printout support for MediaTek 76xx platforms
// The registers from ptrace should come in "struct user_regs_struct" as
// it is in the architecture's sys/user.h file, but what came w/ uclibc
// did not match the real data. The regs were stored starting w/ offset
// zero in 64-bit fields, the lower part containing the useful data.
// There could be up to 38 GP registers and 33 FP registers (according
// to 2.6 kernel's asm/reg.h for the platform).

// The defines below are for accessing the the specific
// specifc register data in the array of 32bit values by offset.
#define EF_REG29        58
#define EF_LO           64
#define EF_HI           66
#define EF_CP0_EPC      68
#define EF_CP0_BADVADDR 70
#define EF_CP0_STATUS   72
#define EF_CP0_CAUSE    74


// Memory for capturing the crashed thread registers
static unsigned long regs[(38 + 33) * 2];

// Print MIPS32 registers
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
        unsigned long exccode, cause = regs[EF_CP0_CAUSE];

        PRNBUF("Registers:\n");

        // Print 32 main registers the same way as the gdb does
        char *regname[] = {
           "zero","at", "v0", "v1", "a0", "a1", "a2", "a3",
           "t0",  "t1", "t2", "t3", "t4", "t5", "t6", "t7",
           "s0",  "s1", "s2", "s3", "s4", "s5", "s6", "s7",
           "t8",  "t9", "k0", "k1", "gp", "sp", "s8", "ra"
        };
        for(jj = 0; jj < 4; ++jj) {
            for(ii = jj * 8; ii < (jj + 1) * 8; ++ii) {
                PRNBUF(" %*s", w, regname[ii]);
            }
            PRNBUF("\n");
            for(ii = jj * 8; ii < (jj + 1) * 8; ++ii) {
                if(ii == 0) {
                    PRNBUF(" %0*lx", w, 0UL);
                } else if(ii == 26 || ii == 27) { // k0, k1 are not captured
                    PRNBUF(" %*s", w, "");
                } else {
                    PRNBUF(" %0*lx", w, regs[ii * 2]);
                }
            }
            PRNBUF("\n");
        }
        PRNBUF("\n");

        PRNBUF(" hi    : %0*lx\n", w, regs[EF_HI]);
        PRNBUF(" lo    : %0*lx\n", w, regs[EF_LO]);

        PRNBUF("\n");

        ri->ip = (void*)regs[EF_CP0_EPC];
        ri->sp = (void*)regs[EF_REG29];
        ri->flags |= UTIL_REGINFO_REGS;

        PRNBUF(" epc   : %0*lx\n", w, regs[EF_CP0_EPC]);
        PRNBUF(" Status: %08lx\n", regs[EF_CP0_STATUS]);

        exccode = (cause & (31UL << 2)) >> 2;
        PRNBUF(" Cause : %08lx (ExcCode %02lx)\n", cause, exccode);
        if(1 <= exccode && exccode <= 5) {
            PRNBUF(" BadVA : %0*lx\n", w, regs[EF_CP0_BADVADDR]);
        }
        PRNBUF("\n");

        break;
    }

    return count;
}
