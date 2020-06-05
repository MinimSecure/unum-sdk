// (c) 2018 minim.co
// unum router iptables include file for telemetry

#ifndef __IPTABLES_H
#define __IPTABLES_H

// Note: this has to go to platform specific includes and
//       has to use full path to the iptables command.
// iptables command to use for getting filter table rules
#define IPTABLES_RULES "iptables --list-rules"
// iptables command to use for getting NAT table rules
#define IPTABLES_NAT_RULES "iptables -t nat --list-rules"

// Prefixes identifying tables, must be 1 character
#define IPTABLES_FLT_CMD "f" // filter table
#define IPTABLES_NAT_CMD "n" // NAT table

// Additional space to allocate when need more space for rules
#define IPTABLES_REALLOC_SIZE 4096

// Max entries to report in single request
#define MAX_IPT_DIFF_ENTRIES 32
// Max length of a rule in the report
#define MAX_IPT_ENTRY_LENGTH 256


// JSON teplate builder callback for NAT and forwarding table diffs.
// The callback is called for all the entries of one key then the other.
// It never mixes the calls up together. That allows to use single entry
// counter variable for all the JSON arrrays the function is used for.
JSON_VAL_TPL_t *ipt_diffs_array_f(char *key, int idx);

// Load rules reported by iptables command into a memory buffer.
// Rotate the buffers (to keep previous and the new rules.
void ipt_diffs_colect_data(void);

// Prepare to report diffs for the specified offsets.
// The diff entries are stored in the arrays and reported
// when the router telemetry request JSON payload is generated.
// Parameters:
// plus_offset - reported so far offset for "add" entries
// minus_offset - reported so far offset for "remove" entries
// next_plus_offset_p - ptr to write to plus offset for 
//                      querying the next chunk of diffs
// next_minus_offset_p - ptr to write to minus offset for 
//                       querying the next chunk of diffs
// Returns: TRUE if there are any diffs to report
int ipt_diffs_prep_to_send(int plus_offset, int minus_offset,
                           int *next_plus_offset_p, int *next_minus_offset_p);

#endif // __IPTABLES_H
