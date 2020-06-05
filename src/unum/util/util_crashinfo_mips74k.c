// (c) 2019 minim.co
// Helper for retrieving various platform specific crashed thread
// state info.

#include "unum.h"

// Added an useful function from kernel code which shows how regs
// info is populated 
// The gcc defines could be used for detecting various architectures
// https://sourceforge.net/p/predef/wiki/Architectures/

// Look up the architecture's traps.c in the kernel sources or the
// platform gdb/gdbserv code to see an example how to decypher and
// print the registers.

// CPU Register printout support for Mips 74k platforms
// The registers from ptrace should come in "struct user_regs_struct"


// The defines below are for accessing the the specific
// specifc register data in the array of 32bit values by offset.
#define EF_R0	        6
#define EF_REG29        35
#define EF_LO           38
#define EF_HI           39
#define EF_CP0_EPC      40
#define EF_CP0_BADVADDR 41
#define EF_CP0_STATUS   42
#define EF_CP0_CAUSE    43

#define NUM_OF_REGS     44

// Memory for capturing the crashed thread registers
static long long regs[NUM_OF_REGS];

// Print MIPS 74K registers
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
        int ii, w = 8;
        unsigned long exccode, cause = regs[EF_CP0_CAUSE - EF_R0];

        PRNBUF("Registers:\n");

        // Print 32 main registers the same way as the gdb does
        char *regname[] = {
           "R0","R1", "R2", "R3", "R4", "R5", "R6", "R7",
           "R8","R9", "R10", "R11", "R12", "R13", "R14", "R15",
           "R16","R17", "R18", "R19", "R20", "R21", "R22", "R23",
           "R24","R25", "R26", "R27", "R28", "R29", "R30", "R31",
        };
        for (ii = 0; ii < 32; ii++) {
            PRNBUF(" %*s", w, regname[ii]);
            PRNBUF(" %0*lx ", w, (long)regs[ii + EF_R0]);
            if ((ii + 1) % 8 == 0) {
                PRNBUF("\n");
            }
        }
        PRNBUF("\n");

        PRNBUF(" hi    : %0*lx\n", w, (long)regs[EF_HI - EF_R0]);
        PRNBUF(" lo    : %0*lx\n", w, (long)regs[EF_LO - EF_R0]);

        PRNBUF("\n");

        ri->ip = (void*)((long)regs[EF_CP0_EPC - EF_R0]);
        ri->sp = (void*)((long)regs[EF_REG29 - EF_R0]);
        ri->flags |= UTIL_REGINFO_REGS;

        PRNBUF(" epc   : %0*lx\n", w, (long)regs[EF_CP0_EPC - EF_R0]);
        PRNBUF(" Status: %08lx\n", (long)regs[EF_CP0_STATUS - EF_R0]);

        exccode = (cause & (31UL << 2)) >> 2;
        PRNBUF(" Cause : %08lx (ExcCode %02lx)\n", cause, exccode);
        PRNBUF(" BadVA : %0*llx\n", w, regs[EF_CP0_BADVADDR - EF_R0]);
        PRNBUF("\n");

        break;
    }

    return count;
}

// Reference Kernel code from arch/mips/kernel/ptrace.c
#if 0
/*
 * Read a general register set.  We always use the 64-bit format, even
 * for 32-bit kernels and for 32-bit processes on a 64-bit kernel.
 * Registers are sign extended to fill the available space.
 */
int ptrace_getregs(struct task_struct *child, __s64 __user *data)
{
        struct pt_regs *regs;
        int i;

        if (!access_ok(VERIFY_WRITE, data, 38 * 8))
                return -EIO;

        regs = task_pt_regs(child);

        for (i = 0; i < 32; i++)
                __put_user((long)regs->regs[i], data + i);
        __put_user((long)regs->lo, data + EF_LO - EF_R0);
        __put_user((long)regs->hi, data + EF_HI - EF_R0);
        __put_user((long)regs->cp0_epc, data + EF_CP0_EPC - EF_R0);
        __put_user((long)regs->cp0_badvaddr, data + EF_CP0_BADVADDR - EF_R0);
        __put_user((long)regs->cp0_status, data + EF_CP0_STATUS - EF_R0);
        __put_user((long)regs->cp0_cause, data + EF_CP0_CAUSE - EF_R0);

        return 0;
}
#endif

