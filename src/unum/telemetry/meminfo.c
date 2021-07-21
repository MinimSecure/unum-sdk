// (c) 2018 minim.co
// Router memory usage information
#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Counter of the succesful mem info update
static int mem_info_update_num = 0;

// Memory counters
static int mem_free;      // free memory
static int mem_available; // free plus what can be reclaimed


// Parse meminfo entry (based on a pattern) in /proc/meminfo
// meminfo: /proc/meminfo as a file
// name: Name of the counter as per the file content
// Returns the counter value
static long parse_meminfo_entry(const char* meminfo, const char *name)
{
    char* offset;
    unsigned long memvalue;

    offset = strstr(meminfo, name);
    if(offset == NULL) {
        return -1;
    }

    offset = offset + strlen(name);
    memvalue = strtoul(offset, NULL, 10);

    return memvalue;
}

// Read the file /proc/meminfo and invoke parsing for selected lines
// meminfo: %'s of free and available memory
int parse_proc_mem_info(meminfo_t *meminfo)
{
#define MEM_FILE_SIZE 4096
    FILE* fp;
    char meminfo_file[MEM_FILE_SIZE];
    size_t len;
    unsigned long buffers;
    unsigned long cached;
    unsigned long meminfo_total;
    unsigned long meminfo_free;
    unsigned long meminfo_available;
    int ret = -1;

    fp = fopen("/proc/meminfo", "r");

    if(fp == NULL) {
        log("%s: Error while opening /proc/meminfo file\n", __func__);
        return ret;
    }

    memset(meminfo_file, 0, sizeof(meminfo_file));
    memset(meminfo, 0, sizeof(meminfo_t));

    len = fread(meminfo_file, 1, sizeof(meminfo_file) - 1, fp);

    fclose(fp);
    if(len == 0) {
        log("%s: Error while reading data from /proc/meminfo\n", __func__);
        return ret;
    }

    meminfo_file[len] = '\0';
    meminfo_total = parse_meminfo_entry(meminfo_file, "MemTotal:");
    meminfo_free = parse_meminfo_entry(meminfo_file, "MemFree:");
    buffers = parse_meminfo_entry(meminfo_file, "Buffers:");
    cached = parse_meminfo_entry(meminfo_file, "Cached:");

    if(meminfo_total != -1 && meminfo_free != -1 && buffers != -1 && cached != -1) {
        meminfo->meminfo_free_perc = (meminfo_free * 100) / (meminfo_total);
        meminfo_available = meminfo_free + buffers + cached;
        meminfo->meminfo_available_perc = (meminfo_available * 100) / (meminfo_total);
        ret = 0;
    } else {
        // Some error while parsing /proc/meminfo.
        // Log the error
        // Dont update the % counters
        log("%s: Error while computing Available Memory \n", __func__);
    }

    return ret;
}

// Calback returning pointer to the integer value JSON builder adds
// to the request.
int *mem_info_f(char *key)
{
    if(strcmp("mem_free", key) == 0) {
        return &mem_free;
    }
    if(strcmp("mem_available", key) == 0) {
        return &mem_available;
    }

    return NULL;
}

// Get counter of the latest meminfo update
int get_meminfo_counter(void)
{
    return mem_info_update_num;
}

// Update the Memory Information counters. Returns consecutive number
// of the mem info captured (which is unchanged if it fails to capture
// the new info).
int update_meminfo(void)
{
    meminfo_t meminfo;
    if(parse_proc_mem_info(&meminfo) < 0) {
        // Error, nothing to do, will try again
        return mem_info_update_num;
    }

    // Update the counter only if the info changes,
    // that prevents sending the same numbers in the telemetry
    if(mem_free != meminfo.meminfo_free_perc ||
       mem_available != meminfo.meminfo_available_perc)
    {
        mem_free = meminfo.meminfo_free_perc;
        mem_available = meminfo.meminfo_available_perc;

        ++mem_info_update_num;
    }
    return mem_info_update_num;
}
