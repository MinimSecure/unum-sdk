// (c) 2018-2019 minim.co
// tracer's crash info collector for unum agent monitor

#include "unum.h"


// Use to disable debug logging even in debug builds if it is too much
#undef log_dbg
#define log_dbg(...) /* Nothing */


// Macro to use by the routines here that print into buffer
// and keep track of space used (it requires specific local vars)
#define PRNBUF(args...) \
    (offset = ((buf_len > count) ? count : buf_len),\
     count += snprintf(buf + offset, buf_len - offset, args))


// Crash info data block pointer, allocated only if/when needed
static TRACER_CRASH_INFO_t *myci = NULL;

// Common register info structure (filled by platform code)
static UTIL_REGINFO_COMMON_t rinfo;


// Capture the crash overview
// file - procfs cmdline file
// buf - where to print
// buf_len - buf length
// Returns: like snprintf()
static int crashinfo_overview(char *buf, int buf_len,
                              int pid, int tid, char *name)
{
    int count = 0;
    int offset = 0;

    // Note: do not add printouts that would make content change for the same
    //       crashes (like time, PID, etc)
    PRNBUF("Unum v%s on %s, stopped due to %s\n", VERSION, DEVICE_PRODUCT_NAME, name);

    return count;
}

// Capture the comamnd line
// file - procfs cmdline file
// buf - where to print
// buf_len - buf length
// Returns: like snprintf()
static int crashinfo_cmd(char *buf, int buf_len, char *file)
{
    int ii, val;
    int offset = 0;
    int count = 0;
    char str[256];

    FILE *f = fopen(file, "r");
    if(f == NULL) {
        log("%s: cannot open %s: %s\n", __func__, file, strerror(errno));
        return count;
    }

    for(;;) {
        val = fread(str, 1, sizeof(str), f);
        if(val <= 0 || val > sizeof(str)) {
            log("%s: fread returned %d for %s: %s\n",
                __func__, val, file, strerror(errno));
            break;
        }
        for(ii = 0; ii < val - 1; ++ii) {
            if(str[ii] == 0) {
                str[ii] = ' ';
            }
        }
        str[val - 1] = 0;
        PRNBUF("Cmd: %s\n\n", str);
        break;
    }

    fclose(f);
    f = NULL;

    return count;
}

// Capture the memory map and extract info about
// memory regions the IP and SP are pointing to.
// file - procfs mem regions file
// pr - ptr to info about IP and SP
// buf - where to print
// buf_len - buf length
// Returns: like snprintf(), "pr" is updated if the info is available
// Note: "pr" might not have data about IP and SP, the function
//       should be able to handle that.
static int crashinfo_maps(char *buf, int buf_len,
                          char *file, UTIL_REGINFO_COMMON_t *pr)
{
    int offset = 0;
    int count = 0;

    FILE *f = fopen(file, "r");
    if(f == NULL) {
        log("%s: cannot open %s: %s\n", __func__, file, strerror(errno));
        return count;
    }

    PRNBUF("Memory map:\n");

    // address           perms offset  dev   inode   pathname
    // 00400000-0044b000 r-xp 00000000 1f:02 170     /bin/busybox
    // offset - if the region was mapped from a file (using mmap),
    //          this is the offset in the file where the mapping begins
    char str[128];
    while(fgets(str, sizeof(str), f) != NULL)
    {
        int str_len;

        // Clean up the LF at the end and capture the line
        for(str_len = strlen(str);
            str_len > 0 && iscntrl(str[str_len - 1]);
            --str_len)
        {
            str[str_len - 1] = 0;
        }
        if(str_len == 0) {
            continue;
        }

        PRNBUF(" %s", str);

        // If we have SP/IP check the region against them
        while((pr->flags & UTIL_REGINFO_REGS) != 0)
        {
            void *r_start, *r_end;
            char *name;
            if(sscanf(str, "%p-%p", &r_start, &r_end) != 2) {
                break;
            }
            if((name = strchr(str, '/')) == NULL) {
                name = strchr(str, '[');
            }
            if(r_start <= pr->ip && pr->ip < r_end) {
                pr->ip_r_start = r_start;
                pr->ip_r_end = r_end;
                strncpy(pr->ip_r_name, name, sizeof(pr->ip_r_name));
                pr->ip_r_name[sizeof(pr->ip_r_name) - 1] = 0;
                PRNBUF(" ip@%p+%p", r_start, (void *)(pr->ip - r_start));
                pr->flags |= UTIL_REGINFO_IP_MAPS;
            }
            if(r_start <= pr->sp && pr->sp < r_end) {
                pr->sp_r_start = r_start;
                pr->sp_r_end = r_end;
                PRNBUF(" sp@%p+%p", r_start, (void *)(pr->sp - r_start));
                pr->flags |= UTIL_REGINFO_SP_MAPS;
            }
            break;
        }

        PRNBUF("\n");
    }

    fclose(f);
    f = NULL;

    PRNBUF("\n");

    return count;
}

// Capture the stack state
// tid - id of the thread
// pr - ptr to info about IP and SP
// buf - where to print
// buf_len - buf length
// Returns: like snprintf()
// Note: "pr" might not have data about IP and SP, the function
//       should be able to handle that.
static int crashinfo_stack(char *buf, int buf_len,
                           int tid, UTIL_REGINFO_COMMON_t *pr)
{
    int offset = 0;
    int count = 0;

    // If we do not have stack region, do nothing
    if((pr->flags & UTIL_REGINFO_SP_MAPS) == 0) {
        return count;
    }

    PRNBUF("Stack:\n");
    PRNBUF(" sp => %p\n", pr->sp);

    // Start at the lowest nearest 64 byte boundary
    // Assuming stack is growing down (unlikely to see anything else here)
    void *start = pr->sp - ((unsigned long)pr->sp & 63);
    void *end = start + sizeof(unsigned long) * UTIL_CRASHINFO_STACK_WORDS;

    // Make sure we stay within the region
    start = (start < pr->sp_r_start) ? pr->sp_r_start : start;
    end = (end > pr->sp_r_end) ? pr->sp_r_end : end;

    // Print it
    int ii, w = 8;
    unsigned long *ptr, val;
    for(ii = 0, ptr = start; (void*)ptr < end; ++ptr, ++ii) {
        if((ii % 8) == 0) {
            PRNBUF(" %p:", ptr);
        }
        errno = 0;
        val = ptrace(PTRACE_PEEKDATA, tid, (void*)ptr, NULL);
        if(errno != 0) {
            PRNBUF(" %*s", w, "");
        } else {
            PRNBUF(" %0*lx", w, val);
        }
        if(((ii + 1) % 8) == 0) {
            PRNBUF("\n");
        }
    }

    PRNBUF("\n");

    return count;
}

// Collect debug info about the stopped thread.
// pid - PID of the process
// tid - ID of the reported thread (PID of the crashed thread)
// name - violation/event name
// ci - location where the caller wants us to put the pointer
//      w/ crash data collected. In the monitor process this
//      pointer is managed by the tracer's crash info collector.
//      Do not free it till forked from the monitor process.
// returns: 0 - if successful, negative val othervise, the content of
//          ci ptr is NULL in case of an error
int util_crashinfo_get(int pid, int tid, char *name, TRACER_CRASH_INFO_t **ci)
{
    int err, buf_len, count, offset;
    char *buf;

    log_dbg("%s: enter, pid/tid: %d/%d, event: %s, ci: *(%p)=%p\n",
            __func__, pid, tid, name, ci, *ci);

    for(;;)
    {
        err = 0;
        *ci = NULL;

        // Allocate a new crash info buffer or reset the existent
        if(myci == NULL) {
            myci = UTIL_MALLOC(UTIL_CRASHINFO_BUF_SIZE);
            if(myci == NULL) {
                err = -1;
                break;
            }
        }
        // Reset the crash info data
        myci->type[0] = 0;
        myci->msg[0] = 0;
        myci->data_len = 0;
        myci->data[0] = 0;
        rinfo.flags = 0;

        // Set up vars for printing into the memory buffer
        buf = myci->data;
        buf_len = UTIL_CRASHINFO_BUF_SIZE - ((void*)buf - (void*)myci);
        offset = count = 0;

        // General info about the agent and the event
        count += crashinfo_overview(buf + offset, buf_len - offset,
                                    pid, tid, name);
        offset = (count >= buf_len) ? buf_len : count;

        // Capture the command line.
        char file[64];
        snprintf(file, sizeof(file), "/proc/%d/cmdline", pid);
        count += crashinfo_cmd(buf + offset, buf_len - offset, file);
        offset = (count >= buf_len) ? buf_len : count;

        // Capture registers
        if(util_crashinfo_regs != NULL) {
            count += util_crashinfo_regs(tid, buf + offset,
                                         buf_len - offset, &rinfo);
            offset = (count >= buf_len) ? buf_len : count;
        }

        // Capture the memory map and id the stack segment location.
        struct stat st;
        // Have to figure out if we have old threads flat mapping /proc/<TID>
        // or new /proc/<PID>/tasks/<TID> structure to get the thread map w/
        // stack segment mapped properly for the specific tid.
        snprintf(file, sizeof(file), "/proc/%d/maps", tid);
        if(stat(file, &st) < 0) {
            snprintf(file, sizeof(file),
                     "/proc/%d/tasks/%d/maps", pid, tid);
        }
        count += crashinfo_maps(buf + offset, buf_len - offset, file, &rinfo);
        offset = (count >= buf_len) ? buf_len : count;

        // Capture the stack state if we know about SP and a matching
        // memory region is found
        count += crashinfo_stack(buf + offset, buf_len - offset, tid, &rinfo);
        offset = (count >= buf_len) ? buf_len : count;

        // Length of data
        myci->data_len = offset + 1;
        // Crash event type/name
        snprintf(myci->type, sizeof(myci->type), "%s", name);
        time_t tt;
        time(&tt);
        char *tt_str = ctime(&tt);
        if((rinfo.flags & UTIL_REGINFO_REGS) != 0) {
            // Crash location
            snprintf(myci->location, sizeof(myci->location), "%p", rinfo.ip);
            // Crash event message
            snprintf(myci->msg, sizeof(myci->msg),
                     "Unum v%s, pid/tid %d/%d, stopped on %.24s due to %s at %p",
                     VERSION, pid, tid, tt_str, name, rinfo.ip);
        } else {
            // Crash location
            snprintf(myci->location, sizeof(myci->location), "unknown");
            // Crash event message
            snprintf(myci->msg, sizeof(myci->msg),
                     "Unum v%s, pid/tid %d/%d, stopped on %.24s due to %s",
                     VERSION, pid, tid, tt_str, name);
        }

        // Check what we put in the buffers
        log_dbg("%s: ci type    : %s\n", __func__, myci->type);
        log_dbg("%s: ci location: %s\n", __func__, myci->location);
        log_dbg("%s: ci data_len: %d/%d bytes\n",
                __func__, myci->data_len, buf_len);

        // Log info about the crash to the monitor log
        log("%s: %s\n", __func__, myci->msg);

        // So far only text crash data are collected, so log it too
        if(myci->data_len > 1) {
            char *start, *end;
            start = myci->data;
            for(;;) {
                end = strchrnul(start, '\n');
                log("%s: %.*s\n", __func__, end - start, start);
                if(*end == 0 || end - myci->data >= myci->data_len) {
                    break;
                }
                start = end + 1;
            }
        }

        // Warn if can't print all the data, do not treat it as an error
        if(count >= buf_len) {
            log("%s: crash info buffer too short, need %d, have %d bytes\n",
                __func__, count, buf_len);
        }

        break;
    }

    if(err != 0) {
        *ci = NULL;
    } else {
        *ci = myci;
    }

    log_dbg("%s: done, err: %d, ci: *(%p)=%p\n", __func__, err, ci, *ci);
    return err;
}
