// (c) 2018 minim.co
// Router cpu usage information

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// CPU info counter buffers
static cpuinfo_t buf1, buf2;

// Ptr to previous cpuinfo read (need it for calcualtiong offsets)
static cpuinfo_t *cpuinfo_prev_p = &buf1;
// Ptr to the current cpuinfo read
static cpuinfo_t *cpuinfo_cur_p = &buf2;

// Counter of the successful info reads
static int cpu_info_read_num = 0;
// Counter of the successful info updates
static int cpu_info_update_num = 0;

// Counters reported in JSON (all in %)
static int cpu_usage = 0;
static int cpu_softirq = 0;
static int cpu_max_usage = 0;
static int cpu_max_softirq = 0;


// Updates the % counters reported in JSON
// Returns: 0  - counters updated, negative - cannot update,
//          positive - counters unchanged
static int calculate_counters_for_report(void)
{
    int ii;
    cpuinfo_t *cur_p = cpuinfo_cur_p;
    cpuinfo_t *last_p = cpuinfo_prev_p;
    int num_cpus = cur_p->num_of_cpus;
    
    // We cannot calcualte if it's the first data capture
    // (i.e. there's still no previous set of counters)
    if(cpu_info_read_num < 2) {
        return -1;
    }

    // Calculate the counters
    unsigned long idle_cycles_sum = 0;
    unsigned long usage_cycles_sum = 0;
    unsigned long softirq_cycles_sum = 0;
    unsigned long max_usage_cycles = 0;
    unsigned long max_usage_total = 0;
    unsigned long max_softirq_cycles = 0;
    unsigned long max_softirq_total = 0;
    for(ii = 0; ii < num_cpus; ++ii)
    {
        unsigned long usage_cycles_cur = cur_p->stats[ii].cpu_usage_cycles;
        unsigned long idle_cycles_cur =  cur_p->stats[ii].cpu_idle_cycles;
        unsigned long softirq_cycles_cur = cur_p->stats[ii].cpu_softirq_cycles;
        unsigned long total_cycles_cur = usage_cycles_cur +
                                         idle_cycles_cur +
                                         softirq_cycles_cur;

        unsigned long usage_cycles_prev = last_p->stats[ii].cpu_usage_cycles;
        unsigned long idle_cycles_prev = last_p->stats[ii].cpu_idle_cycles;
        unsigned long softirq_cycles_prev= last_p->stats[ii].cpu_softirq_cycles;
        unsigned long total_cycles_prev = usage_cycles_prev +
                                          idle_cycles_prev +
                                          softirq_cycles_prev;

        // Compute the diffs of the counters from last iteration to the current
        unsigned long total_cycles_diff = total_cycles_cur -
                                          total_cycles_prev;

        // Accumulate the counters
        idle_cycles_sum += (idle_cycles_cur - idle_cycles_prev);
        usage_cycles_sum += (usage_cycles_cur - usage_cycles_prev);
        softirq_cycles_sum += (softirq_cycles_cur - softirq_cycles_prev);

        // Look for the most busy CPUs numbers
        unsigned long diff = (usage_cycles_cur - usage_cycles_prev);
        if(max_usage_cycles < diff) {
            max_usage_cycles = diff;
            max_usage_total = total_cycles_diff;
        }
        diff = (softirq_cycles_cur - softirq_cycles_prev);
        if(max_softirq_cycles < diff) {
            max_softirq_cycles = diff;
            max_softirq_total = total_cycles_diff;
        }
    }

    // Compute the % counters
    unsigned long ts = idle_cycles_sum + usage_cycles_sum + softirq_cycles_sum;
    if(ts == 0) {
        return -2;
    }
    unsigned long n_cpu_usage = ((ts / 2) + (usage_cycles_sum * 100)) / ts;
    unsigned long n_cpu_softirq = ((ts / 2) + (softirq_cycles_sum * 100)) / ts;
    if(max_usage_total == 0) {
        return -3;
    }
    unsigned long n_cpu_max_usage = ((max_softirq_total / 2) +
                                     (max_usage_cycles * 100)) /
                                    max_usage_total;
    if(max_softirq_total == 0) {
        return -4;
    }
    unsigned long n_cpu_max_softirq = ((max_softirq_total / 2) +
                                       (max_softirq_cycles * 100)) /
                                      max_softirq_total;

    if(n_cpu_usage == cpu_usage && n_cpu_softirq == cpu_softirq &&
       n_cpu_max_usage == cpu_max_usage && n_cpu_max_softirq == cpu_max_softirq)
    {
        // Nothing has changed
        return 1;
    }
    
    cpu_usage = n_cpu_usage;
    cpu_softirq = n_cpu_softirq;
    cpu_max_usage = n_cpu_max_usage;
    cpu_max_softirq = n_cpu_max_softirq;
    
    return 0;
}

// Parse cpu usage info. Fetch it from the file /proc/stat
// Returns: 0 - successful, negative - error
static int parse_proc_cpu_info(cpuinfo_t *cpuinfo)
{
#define CPU_LINE_MAX_LENGTH 256
    FILE* fp;
    char cpu_line[CPU_LINE_MAX_LENGTH];
    char *space = " ";
    char *value;
    char *valid_line;
    unsigned long idle_cycles;
    unsigned long usage_cycles;
    unsigned long softirq_cycles;
    int num_of_values;

    fp = fopen("/proc/stat", "r");
    if(fp == NULL) {
        log("%s: Error while opening /proc/stat file\n", __func__);
        return -1;
    }

    // Reset the structure
    memset(cpuinfo, 0, sizeof(cpuinfo_t));

    // Sample format of /proc/stat shown below
    // Starts with cumulative cpu usage information
    // followied by individual cpu usage
    // /proc/stat example
    // cpu  9297 127079 48040 1412602 0 0 12530 0 0 0
    // cpu0 9297 127079 48040 1412602 0 0 12530 0 0 0

    // Format of each entry is as follows in the order after the string cpu*
    // user: normal processes executing in user mode
    // nice: niced processes executing in user mode
    // system: processes executing in kernel mode
    // idle: twiddling thumbs
    // iowait: waiting for I/O to complete
    // irq: servicing interrupts
    // softirq: servicing softirqs

    // Skip the first line. It contains the cumulative values
    fgets(cpu_line, CPU_LINE_MAX_LENGTH - 1, fp);

    // From second line there is a line for each CPU
    while(cpuinfo->num_of_cpus < CPUINFO_MAX_CPUS &&
          fgets(cpu_line, CPU_LINE_MAX_LENGTH, fp) != NULL)
    {
        // Valid line contains (starts with) the string cpu
        valid_line = strstr(cpu_line, "cpu");
        if(valid_line == NULL) {
            break;
        }
        value = strtok(cpu_line, space);
        // First word is cumulative cpu stats. Skip it.
        // Start from the second word
        value = strtok(NULL, space);

        num_of_values = 0;
        usage_cycles = 0;
        idle_cycles = 0;
        softirq_cycles = 0;
        while(value != NULL)
        {
            if(num_of_values == 3) {
                idle_cycles = strtoul(value, NULL, 10);
            } else if(num_of_values == 6) {
                softirq_cycles = strtoul(value, NULL, 10);
            } else {
                usage_cycles += strtoul(value, NULL, 10);
            }
            num_of_values++;
            value = strtok(NULL, space);
        }

        cpuinfo->stats[cpuinfo->num_of_cpus].cpu_usage_cycles = usage_cycles;
        cpuinfo->stats[cpuinfo->num_of_cpus].cpu_idle_cycles = idle_cycles;
        cpuinfo->stats[cpuinfo->num_of_cpus].cpu_softirq_cycles = softirq_cycles;
        cpuinfo->num_of_cpus++;
    }
    fclose(fp);

    return 0;
}

// Calback returning pointer to the integer value JSON builder adds
// to the request.
int *cpu_usage_f(char *key)
{
    // No counters were calculated since we did not have 2 reads
    if(cpu_info_read_num < 2) {
        return NULL;
    }

    if(strcmp("cpu_usage", key) == 0) {
        return &cpu_usage;
    }
    if(strcmp("cpu_softirq", key) == 0) {
        return &cpu_softirq;
    }
    if(strcmp("cpu_max_usage", key) == 0) {
        return (cpuinfo_cur_p->num_of_cpus > 1 ? &cpu_max_usage : NULL);
    }
    if(strcmp("cpu_max_softirq", key) == 0) {
        return (cpuinfo_cur_p->num_of_cpus > 1 ? &cpu_max_softirq : NULL);
    }

    return NULL;
}

// Get counter of the latest CPU info update
int get_cpuinfo_counter(void)
{
    return cpu_info_update_num;
}

// Updates the CPU info. The old data end up in cpuinfo_prev_p, the new
// in cpuinfo_cur_p. Returns consecutive number of the CPU info captured
// (which is unchanged if it fails to capture the new info).
int update_cpuinfo(void)
{
    cpuinfo_t cpuinfo;
    if(parse_proc_cpu_info(&cpuinfo) != 0) {
        // Error, nothing to do, will try again
        return cpu_info_update_num;
    }
    // Swap the pointers
    cpuinfo_t *tmp_p = cpuinfo_prev_p;
    cpuinfo_prev_p = cpuinfo_cur_p;
    cpuinfo_cur_p = tmp_p;

    // Update the current info
    memcpy(cpuinfo_cur_p, &cpuinfo, sizeof(cpuinfo));
    ++cpu_info_read_num;

    // Calculate counters for the report
    int err = calculate_counters_for_report();
    // Increase the update counter only if counters changed or it's the
    // first successful counters calculation.
    if(err == 0 || (err > 0 && cpu_info_update_num == 0)) {
        ++cpu_info_update_num;
    }
    return cpu_info_update_num;
}
