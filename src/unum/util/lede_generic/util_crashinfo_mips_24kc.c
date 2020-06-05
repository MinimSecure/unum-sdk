// (c) 2018 minim.co
// Helper for retrieving various platform specific crashed thread
// state info.

#include "unum.h"


// Pull in ELF_NGREG and ELF_NFPREG
// The regs data structure does not match though, they assume type long for
// general purpose registers, but the values come in the high word of ULL
#include "sys/user.h"


// The gcc defines could be used for detecting various architectures
// https://sourceforge.net/p/predef/wiki/Architectures/

// Look up the architecture's traps.c in the kernel sources or the
// platform gdb/gdbserv code to see an example how to decypher and
// print the registers.

// CPU Register printout support for Archer C7 32bit MIPS.
// The defines below are for accessing the specifc register data
// in the array of register values of the REG_DATA_t type.
#define EF_REG29        29
#define EF_LO           32
#define EF_HI           33
#define EF_CP0_EPC      34
#define EF_CP0_BADVADDR 35
#define EF_CP0_STATUS   36
#define EF_CP0_CAUSE    37

// Register data struct
typedef struct {
    unsigned long l;
    unsigned long h;
} REG_DATA_t;

// Memory for capturing the crashed thread registers
static REG_DATA_t regs[ELF_NGREG + ELF_NFPREG];

// Print MIPS32 registers for Archer C7
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
        unsigned long exccode, cause = regs[EF_CP0_CAUSE].h;

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
                    PRNBUF(" %0*lx", w, regs[ii].h);
                }
            }
            PRNBUF("\n");
        }
        PRNBUF("\n");

        PRNBUF(" hi    : %0*lx\n", w, regs[EF_HI].h);
        PRNBUF(" lo    : %0*lx\n", w, regs[EF_LO].h);

        PRNBUF("\n");

        ri->ip = (void*)regs[EF_CP0_EPC].h;
        ri->sp = (void*)regs[EF_REG29].h;
        ri->flags |= UTIL_REGINFO_REGS;

        PRNBUF(" epc   : %0*lx\n", w, regs[EF_CP0_EPC].h);
        PRNBUF(" Status: %08lx\n", regs[EF_CP0_STATUS].h);

        exccode = (cause & (31UL << 2)) >> 2;
        PRNBUF(" Cause : %08lx (ExcCode %02lx)\n", cause, exccode);
        if(1 <= exccode && exccode <= 5) {
            PRNBUF(" BadVA : %0*lx\n", w, regs[EF_CP0_BADVADDR].h);
        }
        PRNBUF("\n");

        break;
    }

    return count;
}
